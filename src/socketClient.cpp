#include "SocketClient.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
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

nlohmann::json makeSubscriptionRequest(const std::vector<std::string>& assetIds, const std::string& type, bool initialDump, int level, bool customFeatureEnabled) {
    nlohmann::json request;
    request["assets_ids"] = assetIds;
    request["type"] = type;
    request["initial_dump"] = initialDump;
    request["level"] = level;
    request["custom_feature_enabled"] = customFeatureEnabled;
    return request;
}

}
SocketClient::SocketClient(const std::string& host, const std::string& port, const std::string& target, ArbitrageEngine& engine, bool verifyCertificate) : _host(host),
    _port(port),
    _target(target),
    _verifyCertificate(verifyCertificate),
    _engine(engine),
    _ioc(),
    _ctx(net::ssl::context::tls_client),
    _resolver(_ioc),
    _ws(_ioc, _ctx)
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

        std::cerr << "[error] connection error: " << e.what() << '\n';

        return false;
    }
}

bool SocketClient::subscribe(const std::vector<std::string>& assetIds, const std::string& type, bool initialDump, int level, bool customFeatureEnabled) {
    int attempt = 0;
    bool hasSubscribed = false;

    while (true) {
        if (_state != ConnectionState::Connected && !connect()) {
            std::cerr << "Reconnect failed; retrying in "
                      << backoffDelay(attempt).count()
                      << "s\n";

            std::this_thread::sleep_for(backoffDelay(attempt));
            ++attempt;

            continue;
        }

        if (hasSubscribed) {
            std::cout << "Connection successful\n";
        }

        const auto connectionStarted = std::chrono::steady_clock::now();

        try {
            // The server accepts exactly one subscription message per
            // connection, so all assets must go in a single message.
            _ws->write(net::buffer(
                makeSubscriptionRequest(
                    assetIds, type, initialDump, level,
                    customFeatureEnabled).dump()));

            hasSubscribed = true;

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
                    std::cerr << "[error] malformed message: " << e.what()
                              << " | raw: " << message.substr(0, 120)
                              << '\n';
                }
            }
        }
        catch (const boost::system::system_error& e) {
            // A server-initiated WebSocket close is expected and recoverable.
            if (e.code() == websocket::error::closed) {
                if (_ws) {
                    const auto& reason = _ws->reason();
                    std::cerr << "[ws] connection closed by server"
                              << " (code "
                              << static_cast<unsigned>(reason.code)
                              << ", reason: \"" << reason.reason << "\")\n";
                } else {
                    std::cerr << "[ws] connection closed by server\n";
                }
            } else {
                std::cerr << "[error] connection lost: " << e.what() << '\n';
            }

            _state = ConnectionState::Disconnected;
            _ws.reset();
            _buffer.consume(_buffer.size());
        }
        catch (const std::exception& e) {
            std::cerr << "[error] connection lost: " << e.what() << '\n';

            _state = ConnectionState::Disconnected;
            _ws.reset();
            _buffer.consume(_buffer.size());
        }

        // A connection that lived for a while should start the next recovery
        // with the short delay; consecutive immediate closes retain
        // exponential backoff instead of hammering the server.
        if (std::chrono::steady_clock::now() - connectionStarted
                >= std::chrono::seconds(30)) {
            attempt = 0;
        }

        // REST resync before reconnecting: the local books may have missed
        // updates while the connection was down.
        if (_resync) {
            try {
                _resync();
                std::cout << "[resync] books resynced from REST\n";
            }
            catch (const std::exception& e) {
                std::cerr << "[error] resync failed: " << e.what() << '\n';
            }
        }

        std::cout << "[ws] reconnecting in " << backoffDelay(attempt).count()
                  << "s\n";

        std::this_thread::sleep_for(backoffDelay(attempt));
        ++attempt;
    }

    return true;
}

std::string SocketClient::read() {
    _ws.read(_buffer);

    std::string message = beast::buffers_to_string(_buffer.data());
    _buffer.consume(_buffer.size());

    return message;
}

void SocketClient::setOnMessage(std::function<void(const nlohmann::json&)> handler) {
    _onMessage = std::move(handler);
}

void SocketClient::setResyncCallback(std::fucntion<void()> callback) {
    _resync = std::move(callback);
}


void SocketClient::handleMessage(const nlohmann::json& json) {
    if (_onMessage) {
        _onMessage(json);
        return;
    }

    auto opportunities = _engine.processMessage(json);

    for (const auto& opportunity : opportunities) {
        std::cout << "[arb] net $" << std::fixed << std::setprecision(4)
                  << opportunity.netProfit << '\n';
    }
}