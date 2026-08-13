#pragma once

#include "OrderBook.hpp"

#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;

using tcp = boost::asio::ip::tcp;

class SocketClient {
public:
    SocketClient(
        const std::string& host,
        const std::string& port,
        const std::string& target,
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

    void ping();

    void handleMessage(const nlohmann::json& json);

private:
    std::string _host;
    std::string _port;
    std::string _target;
    bool _verifyCertificate;

    net::io_context _ioc;
    net::ssl::context _ctx;
    tcp::resolver _resolver;
    websocket::stream<net::ssl::stream<tcp::socket>> _ws;
    beast::flat_buffer _buffer;

    std::unordered_map<std::string, OrderBook> _books;
};
