#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <nlohmann/json.hpp>

#include "ArbitrageEngine.hpp"
#include "ArbitrageOpportunity.hpp"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;

using tcp = boost::asio::ip::tcp;

// Handles the WebSocket connection, subscription, reading messages, and
// forwarding them to the engine. Also owns the reconnect state machine:
// CONNECTED -> CONNECTION_LOST -> BACKOFF -> RECONNECT -> REST RESYNC ->
// RESUBSCRIBE -> CONNECTED.
class SocketClient {
public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected
    };

    SocketClient(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        ArbitrageEngine& engine,
        bool verifyCertificate = true
    );

    // Establishes a fresh WebSocket connection (safe to call again after a
    // connection loss, since the underlying stream is recreated each time).
    bool connect();

    // Sends the subscription message, then enters the read/reconnect loop.
    // Only returns on unrecoverable failure.
    bool subscribe(
        const std::vector<std::string>& assetIds,
        const std::string& type,
        bool initialDump,
        int level,
        bool customFeatureEnabled
    );

    std::string read();

    // Optional hook for the engine consumer (e.g. latency statistics).
    void setOnMessage(std::function<void(const nlohmann::json&)> handler);

    // Invoked before reconnecting so the caller can re-fetch REST snapshots.
    void setResyncCallback(std::function<void()> callback);

    ConnectionState state() const { return _state; }

private:
    // Default handler: forward to the engine and report opportunities.
    void handleMessage(const nlohmann::json& json);

    void reconnectAndResubscribe(
        const std::vector<std::string>& assetIds,
        const std::string& type,
        bool initialDump,
        int level,
        bool customFeatureEnabled
    );

    std::string _host;
    std::string _port;
    std::string _target;
    bool _verifyCertificate;

    ConnectionState _state = ConnectionState::Disconnected;

    net::io_context _ioc;
    net::ssl::context _ctx;
    tcp::resolver _resolver;

    // Recreated on every connect: a closed stream cannot be reused.
    std::unique_ptr<websocket::stream<net::ssl::stream<tcp::socket>>> _ws;

    beast::flat_buffer _buffer;

    ArbitrageEngine& _engine;
    std::function<void(const nlohmann::json&)> _onMessage;
    std::function<void()> _resync;
};
