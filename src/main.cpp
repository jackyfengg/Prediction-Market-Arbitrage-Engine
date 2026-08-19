#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

#include "ArbitrageEngine.hpp"
#include "ClobRestClient.hpp"
#include "MarketLoader.hpp"
#include "SocketClient.hpp"

namespace {

// Simple rolling throughput/latency statistics for the hot path.
struct Stats {
    std::size_t messages = 0;
    double totalLatencyUs = 0;
    double maxLatencyUs = 0;

    std::chrono::steady_clock::time_point lastReport =
        std::chrono::steady_clock::now();

    void record(double latencyUs) {
        ++messages;
        totalLatencyUs += latencyUs;
        maxLatencyUs = std::max(maxLatencyUs, latencyUs);
    }

    // Prints and resets the window; returns true when a report was printed.
    bool maybeReport() {
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::seconds>(
                now - lastReport).count() < 30) {
            return false;
        }

        double avgUs = messages > 0 ? totalLatencyUs / messages : 0.0;
        double msgPerSec =
            messages / std::chrono::duration<double>(now - lastReport).count();

        std::cout << "[stats] " << messages
                  << " messages (" << msgPerSec << "/s), avg processMessage "
                  << avgUs << "us, max " << maxLatencyUs << "us\n";

        messages = 0;
        totalLatencyUs = 0;
        maxLatencyUs = 0;
        lastReport = now;

        return true;
    }
};

void reportOpportunity(const ArbitrageOpportunity& opportunity) {
    std::cout << "Arbitrage found: qty=" << opportunity.quantity
              << " cost=$" << opportunity.totalCost
              << " fees=$" << opportunity.totalFees
              << " slippage=$" << opportunity.slippage
              << " net=$" << opportunity.netProfit
              << " roi=" << (opportunity.returnOnCapital * 100.0)
              << "%\n";
}

} // namespace

int main() {
    // --- 1. Market discovery (Gamma API) ---
    std::vector<Market> markets;

    try {
        ClobRestClient gammaClient("gamma-api.polymarket.com");
        MarketLoader loader(gammaClient);

        markets = loader.loadMarkets(50);

        std::cout << "Discovered " << markets.size() << " active markets\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Market discovery failed: " << e.what() << '\n';
    }

    // Fall back to a single known market if discovery is unavailable.
    if (markets.empty()) {
        std::cout << "Using fallback market\n";

        markets = {
            {
                "3039641309958397001906153616677074061284510636204155275446291716739429262374",
                "27828976648682466778776999076215423777766972981338264154049603024771135223200"
            }
        };
    }

    // Polymarket currently charges 0 trading fees; set a rate to model costs.
    const double feeRate = 0.0;
    const double quantity = 100.0;

    ArbitrageEngine engine(markets, quantity, feeRate);

    std::cout << "Requested quantity per market: " << quantity
              << ", fee rate: " << feeRate << '\n';

    // --- 2. Initialize every book from REST before the socket starts ---
    ClobRestClient clob("clob.polymarket.com");

    auto initializeBooks = [&]() {
        for (const Market& market : engine.markets()) {
            engine.initializeBook(
                market.yesAssetId,
                clob.getOrderBook(market.yesAssetId));
            engine.initializeBook(
                market.noAssetId,
                clob.getOrderBook(market.noAssetId));
        }
    };

    try {
        std::cout << "Fetching initial books...\n";

        initializeBooks();

        std::cout << "Initialized " << (engine.markets().size() * 2)
                  << " books\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize books: " << e.what() << '\n';
        return 1;
    }

    // --- 3. WebSocket with reconnect/resync ---
    SocketClient client(
        "ws-subscriptions-clob.polymarket.com",
        "443",
        "/ws/market",
        engine
    );

    // On connection loss: re-fetch fresh REST snapshots before resubscribing,
    // because incremental updates missed during the outage leave books stale.
    client.setResyncCallback([&]() {
        std::cout << "Resyncing books from REST...\n";
        initializeBooks();
    });

    // --- 4. Measure hot-path latency + report opportunities ---
    Stats stats;

    client.setOnMessage([&](const nlohmann::json& json) {
        auto start = std::chrono::steady_clock::now();

        auto opportunities = engine.processMessage(json);

        auto end = std::chrono::steady_clock::now();

        double latencyUs =
            std::chrono::duration<double, std::micro>(end - start).count();

        stats.record(latencyUs);

        for (const auto& opportunity : opportunities) {
            reportOpportunity(opportunity);
        }

        stats.maybeReport();
    });

    std::cout << "Connecting...\n";

    if (!client.connect()) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    std::cout << "Connection successful\n";

    // Subscribe once to every asset; the server sends initial book snapshots.
    client.subscribe(
        engine.assetIds(),
        "market",
        true,
        0,
        false
    );

    return 0;
}
