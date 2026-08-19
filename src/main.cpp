#include "SocketClient.hpp"
#include "ArbitrageEngine.hpp"
#include "ClobRestClient.hpp"

#include <iostream>
#include <vector>

int main() {
    std::string yesAssetId =
        "3039641309958397001906153616677074061284510636204155275446291716739429262374";

    std::string noAssetId =
        "27828976648682466778776999076215423777766972981338264154049603024771135223200";

    std::vector<Market> markets = {
        {yesAssetId, noAssetId}
    };

    ArbitrageEngine engine(markets, 1.0);

    try {
        ClobRestClient clob(
            "clob.polymarket.com"
        );

        std::cout << "Fetching YES book...\n";

        auto yesBook =
            clob.getOrderBook(yesAssetId);

        engine.initializeBook(
            yesAssetId,
            yesBook
        );

        std::cout << "Fetching NO book...\n";

        auto noBook =
            clob.getOrderBook(noAssetId);

        engine.initializeBook(
            noAssetId,
            noBook
        );

        std::cout << "Initial books loaded\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize books: "
                  << e.what()
                  << '\n';

        return 1;
    }

    SocketClient client(
        "ws-subscriptions-clob.polymarket.com",
        "443",
        "/ws/market",
        engine
    );

    std::cout << "Connecting...\n";

    if (!client.connect()) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    std::cout << "Connection successful\n";

    client.subscribe(
        {yesAssetId, noAssetId},
        "market",
        true,
        0,
        false
    );

    return 0;
}