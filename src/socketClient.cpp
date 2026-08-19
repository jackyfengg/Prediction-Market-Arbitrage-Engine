#include "SocketClient.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include <nlohmann/json.hpp>

#include <openssl/err.h>
#include <openssl/ssl.h>

namespace {

// Exponential backoff: 1s, 2s, 4s, 8s, 16s, 30s, 30s, ...
std::chrono::seconds backoffDelay(int attempt) {
    int seconds = 1 << std::min(attempt, 5);
    return std::chrono::seconds(std::min(seconds, 30));
}

} // namespace

SocketClient::SocketClient(
    const std::string& host,
    const std::string& port,
    const std::string& target,
    ArbitrageEngine& engine,
    bool verifyCertificate
) : _host(host),
    _port(port),
    _target(target),
    _verifyCertificate(verifyCertificate),
    _engine(engine),
    _ioc(),
    _ctx(net::ssl::context::tls_client),
    _resolver(_ioc)
{
}

bool SocketClient::connect() {
    _state = ConnectionState::Connecting;

    try {
        // A fresh stream per connection attempt.
        _ws = std::make_unique<websocket::stream<net::ssl::stream<tcp::socket>>>(
            _ioc, _ctx);

        auto const results = _resolver.resolve(_host, _port);
        auto ep = net::connect(_ws->next_layer().next_layer(), results);

        std::string connectionHost = _host;

        if (_port != "443") {
            connectionHost += ':' + std::to_string(ep.port());
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
                _ws->next_layer().native_handle(),
                _host.c_str())) {
            boost::system::error_code ec{
                static_cast<int>(::ERR_get_error()),
                net::error::get_ssl_category()};
            throw boost::system::system_error{ec};
        }

        // Make sure the certificate matches the host we connected to
        if (_verifyCertificate) {
            _ws->next_layer().set_verify_callback(
                net::ssl::host_name_verification(_host));
        }

        // Start the TLS handshake
        _ws->next_layer().handshake(net::ssl::stream_base::client);

        // Start the WebSocket handshake
        _ws->handshake(connectionHost, _target);

        _state = ConnectionState::Connected;

        return true;
    }
    catch (const std::exception& e) {
        _state = ConnectionState::Disconnected;
        _ws.reset();

        std::cerr << "Connection error: " << e.what() << '\n';

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
    int attempt = 0;

    while (true) {
        if (_state != ConnectionState::Connected && !connect()) {
            std::cerr << "Reconnect failed; retrying in "
                      << backoffDelay(attempt).count()
                      << "s\n";

            std::this_thread::sleep_for(backoffDelay(attempt));
            ++attempt;

            continue;
        }

        try {
            nlohmann::json request;
            request["assets_ids"] = assetIds;
            request["type"] = type;
            request["initial_dump"] = initialDump;
            request["level"] = level;
            request["custom_feature_enabled"] = customFeatureEnabled;

            _ws->write(net::buffer(request.dump()));

            attempt = 0; // a successful subscribe resets the backoff

            std::cout << "Subscribed to " << assetIds.size()
                      << " assets\n";

            while (true) {
                std::string message = read();

                if (message.empty() || message == "PONG") {
                    continue;
                }

                try {
                    handleMessage(nlohmann::json::parse(message));
                }
                catch (const nlohmann::json::exception& e) {
                    // A malformed message should not kill the connection.
                    std::cerr << "Malformed message: " << e.what() << '\n';
                }
            }
        }
        catch (const std::exception& e) {
            // Expected for connection loss (e.g. EOF) - recoverable.
            std::cerr << "Connection lost: " << e.what() << '\n';

            _state = ConnectionState::Disconnected;
            _ws.reset();
            _buffer.consume(_buffer.size());
        }

        // REST resync before reconnecting: the local books may have missed
        // updates while the connection was down.
        if (_resync) {
            try {
                _resync();
                std::cout << "Books resynced from REST\n";
            }
            catch (const std::exception& e) {
                std::cerr << "Resync failed: " << e.what() << '\n';
            }
        }

        std::cout << "Reconnecting in " << backoffDelay(attempt).count()
                  << "s\n";

        std::this_thread::sleep_for(backoffDelay(attempt));
        ++attempt;
    }

    return true;
}

std::string SocketClient::read() {
    _ws->read(_buffer);

    std::string message = beast::buffers_to_string(_buffer.data());
    _buffer.consume(_buffer.size());

    return message;
}

void SocketClient::setOnMessage(std::function<void(const nlohmann::json&)> handler) {
    _onMessage = std::move(handler);
}

void SocketClient::setResyncCallback(std::function<void()> callback) {
    _resync = std::move(callback);
}

void SocketClient::handleMessage(const nlohmann::json& json) {
    if (_onMessage) {
        _onMessage(json);
        return;
    }

    auto opportunities = _engine.processMessage(json);

    for (const auto& opportunity : opportunities) {
        std::cout << "Arbitrage found: net=$"
                  << opportunity.netProfit
                  << '\n';
    }
}
