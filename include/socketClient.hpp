#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class socketClient {
public:
    socketClient(
        const std::string& host,
        const std::string& port,
        const std::string& target
    );

    bool connect();

    bool subscribe(
        const std::vector<std::string>& assetIds,
        const std::string& type,
        bool initialDump,
        int level,
        bool customFeatureEnabled
    );

    void read();
    void ping();

private:
    std::string _host;
    std::string _port;
    std::string _target;

    net::io_context _ioc;
    tcp::resolver _resolver;
    websocket::stream<tcp::socket> _ws;
    beast::flat_buffer _buffer;
};
