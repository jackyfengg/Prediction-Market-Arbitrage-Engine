#include "ClobRestClient.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include <openssl/ssl.h>

#include <chrono>
#include <stdexcept>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;

using tcp = net::ip::tcp;

ClobRestClient::ClobRestClient(std::string host)
    : _host(std::move(host))
{
}

nlohmann::json ClobRestClient::getJson(const std::string& target) const {
    net::io_context ioc;
    ssl::context ctx(ssl::context::tls_client);

    ctx.set_verify_mode(ssl::verify_peer);
    ctx.set_default_verify_paths();

    tcp::resolver resolver(ioc);
    beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

    // Bound each blocking phase so a slow/rate-limited API degrades into a
    // failed fetch (handled by the caller) instead of hanging forever.
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(10));

    auto const results = resolver.resolve(_host, "443");

    beast::get_lowest_layer(stream).connect(results);

    if (!SSL_set_tlsext_host_name(stream.native_handle(), _host.c_str())) {
        throw std::runtime_error("Failed to set TLS SNI");
    }

    stream.set_verify_callback(ssl::host_name_verification(_host));
    stream.handshake(ssl::stream_base::client);

    http::request<http::empty_body> request{http::verb::get, target, 11};
    request.set(http::field::host, _host);
    request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    request.set(http::field::accept, "application/json");

    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(10));

    http::write(stream, request);

    beast::flat_buffer buffer;
    http::response<http::string_body> response;

    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(10));

    http::read(stream, buffer, response);

    if (response.result() != http::status::ok) {
        throw std::runtime_error(
            "GET " + target + " returned HTTP status " +
            std::to_string(response.result_int()) +
            ": " +
            response.body()
        );
    }

    beast::error_code ec;

    stream.shutdown(ec);

    if (ec == net::error::eof || ec == ssl::error::stream_truncated) {
        ec = {};
    }

    if (ec) {
        throw beast::system_error(ec);
    }

    return nlohmann::json::parse(response.body());
}

nlohmann::json ClobRestClient::getOrderBook(const std::string& assetId) const {
    return getJson("/book?token_id=" + assetId);
}
