#include "SocketClient.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

#include <openssl/err.h>
#include <openssl/ssl.h>

SocketClient::SocketClient(
    const std::string& host,
    const std::string& port,
    const std::string& target,
    bool verifyCertificate
) : _host(host),
    _port(port),
    _target(target),
    _verifyCertificate(verifyCertificate),
    _ioc(),
    _ctx(net::ssl::context::tls_client),
    _resolver(_ioc),
    _ws(_ioc, _ctx)
{
}

bool SocketClient::connect() {
    try {
        // Look up domain name and connect to IP address
        auto const results = _resolver.resolve(_host, _port);
        auto ep = net::connect(_ws.next_layer().next_layer(), results);

        std::string connectionhost = _host;

        if (_port != "443") {
            connectionhost += ':' + std::to_string(ep.port());
        }

        // Verify the server's certificate chain against the system CA store
        if (_verifyCertificate) {
            _ctx.set_verify_mode(net::ssl::verify_peer);
            _ctx.set_default_verify_paths();
        } else {
            // Only for local testing with self-signed certificates
            _ctx.set_verify_mode(net::ssl::verify_none);
        }

        // Send the hostname as SNI so the server picks the right certificate
        if (!SSL_set_tlsext_host_name(
                _ws.next_layer().native_handle(),
                _host.c_str())) {
            boost::system::error_code ec{
                static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category()};
            throw boost::system::system_error{ec};
        }

        // Make sure the certificate matches the host we connected to
        if (_verifyCertificate) {
            _ws.next_layer().set_verify_callback(
                net::ssl::host_name_verification(_host));
        }

        // Start the TLS handshake
        _ws.next_layer().handshake(net::ssl::stream_base::client);

        // Start the WebSocket handshake
        _ws.handshake(connectionhost, _target);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }
}

bool SocketClient::subscribe(
    const std::vector<std::string>& assetIds,
    const std::string& type,
    bool initialDump,
    int level,
    bool customFeatureEnabled
) {
    try {
        nlohmann::json request;
        request["assets_ids"] = assetIds;
        request["type"] = type;
        request["initial_dump"] = initialDump;
        request["level"] = level;
        request["custom_feature_enabled"] = customFeatureEnabled;

        const std::string payload = request.dump();

        // Send message
        _ws.write(net::buffer(payload));

        while (true) {
            std::string message = read();

            auto json = nlohmann::json::parse(message);

            handleMessage(json);
        }
    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
    return true;
}

std::string SocketClient::read() {
    _ws.read(_buffer);

    std::string message = beast::buffers_to_string(_buffer.data());
    _buffer.consume(_buffer.size());

    return message;
}

void SocketClient::ping() {
    _ws.ping({});
}

void SocketClient::handleMessage(const nlohmann::json& json) {
    std::cout << json.dump(2) << '\n';

    if (json.is_array()) {
        for (auto& message : json) {
            handleMessage(message);
        }
    }

    if (!json.is_object()) {
        return;
    }

    std::string eventType = json.value("event_type", "");

    if (eventType == "book") {
        std::string assetId = json["asset_id"].get<std::string>();
        _books[assetId].applySnapshot(json);

    } else if (eventType == "price_change") {
        for (auto& change : json["price_changes"]) {
            std::string assetId = change["asset_id"].get<std::string>();
            _books[assetId].applyPriceChange(change);
        }

    } else if (eventType == "best_bid_ask") {
        // update best bid ask
    } else if (eventType == "last_trade_price") {
        // update last trade
    }
}
