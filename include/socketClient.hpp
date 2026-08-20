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

class SocketClient {
public:

    enum class ConnectionState {Disconnected, Connecting, Conneceted};

    SocketClient(
        const std::string& host,
        const std::string& port,
        const std::string& target,
        ArbitrageEngine& engine,
        bool verifyCertificate = true
    );

    bool connect();

    bool subscribe(
        const std::vector<std::string>& assetIds,
        const std::string& type,
        bool initialDump,
        int level,
        bool customFeatureEnabled
    );

    std::string read();

    void setOnMessage(std::function<void(const nlohmann::json&)> handler);

private:
    void handleMessage(const nlohmann::json& json);

    void reconnectAndResubscribe(const std::vector<std::string>& assetIds, const std::string& type, bool initalDump, int level, bool customFeatureEnabled);

    std::string _host;
    std::string _port;
    std::string _target;
    bool _verifyCertificate;

    ConnectionState _state = ConnectionState::Disconnected;

    net::io_context _ioc;
    net::ssl::context _ctx;
    tcp::resolver _resolver;

    std::unique_ptr<websocket::stream<net::ssl::stream<tcp::socket>>> _ws;
    beast::flat_buffer _buffer;

    ArbitrageEngine& _engine;
    std::function<void(const nlohmann::json&)> _onMessage;
    std::function<void()> _resync;
};
