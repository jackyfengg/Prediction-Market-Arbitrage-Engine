#include "SocketClient.hpp"
#include <iostream>

int main() {
    SocketClient client(
        "ws-subscriptions-clob.polymarket.com",
        "443",
        "/ws/market"
    );

    std::cout << "Connecting...\n";

    if (!client.connect()) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    std::cout << "Connection successful\n";

    client.subscribe(
        {
            "3039641309958397001906153616677074061284510636204155275446291716739429262374"
        },
        "market",
        true,
        0,
        false
    );

    return 0;
}