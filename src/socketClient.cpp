#include "socketClient.hpp"

#include <nlohmann/json.hpp>

namespace beast = boost::beast;         
namespace http = beast::http;          
namespace websocket = beast::websocket; 
namespace net = boost::asio;  

using tcp = boost::asio::ip::tcp;       

class socketClient {
public:
    socketClient(const std::string& host, const std::string& port, const std::string& target)
    : _host(host),
      _port(port),
      _target(target),
      _ioc(),
      _resolver(_ioc),
      _ws(_ioc)
    {}

    bool connect() {
        try {
            // Look up domain name and connect to IP address
            auto const results = _resolver.resolve(_host, _port);
            auto ep = net::connect(_ws.next_layer(), results);

            std::string connectionhost = _host;

            if (_port != "80" && _port != "443") {
                connectionhost += ':' + std::to_string(ep.port());
            }

            // Start connection (Handshake)
            _ws.handshake(connectionhost, _target);
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return false; 
        }
    };

    bool subscribe(
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

            beast::flat_buffer buffer;

            // Read message
            _ws.read(buffer);

            std::string message = beast::buffers_to_string(buffer.data());

            auto json = nlohmann::json::parse(message);

            std::cout << json.dump(2) << '\n';

            return true;
            
        } catch(std::exception const& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return false;
        }
        return true;
    };

    void read() {

    };

    void ping() {
        // TODO: send a WebSocket ping.
    };

    private:
        std::string _host;
        std::string _port;
        std::string _target;

        net::io_context _ioc;   // Input/Output
        tcp::resolver _resolver;    // Perform I/O
        websocket::stream<tcp::socket> _ws;
        beast::flat_buffer _buffer;
};
