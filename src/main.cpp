#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ArbitrageEngine.hpp"
#include "ClobRestClient.hpp"
#include "MarketLoader.hpp"
#include "SocketClient.hpp"

namespace {

constexpr std::size_t kSnapshotSeconds = 30;
constexpr unsigned kBookWorkers = 16;

void printSeparator() {
    std::cout << "--------------------------------------------------\n";
}

std::string marketKey(const Market& market) {
    return market.conditionId.empty()
        ? market.yesAssetId + market.noAssetId
        : market.conditionId;
}

std::string shortLabel(const Market& market, std::size_t maxLen = 48) {
    if (!market.question.empty()) {
        std::string q = market.question;
        return q.size() > maxLen ? q.substr(0, maxLen) + "..." : q;
    }

    if (!market.conditionId.empty()) {
        return market.conditionId.substr(
            0, std::min(maxLen, market.conditionId.size()));
    }

    return market.yesAssetId.substr(0, 12);
}

// ---------------------------------------------------------------------------
// Opportunity reporting. This intentionally keeps the simple streaming
// format: every detected opportunity is printed with its market, prices,
// economics, tokens, and link.
// ---------------------------------------------------------------------------

void reportOpportunity(const ArbitrageOpportunity& opp) {
    const double qty = opp.quantity;
    const double yesPrice = qty > 0 ? opp.yesCost / qty : 0.0;
    const double noPrice = qty > 0 ? opp.noCost / qty : 0.0;

    std::cout << "Arbitrage found: " << opp.market.question << "\n";
    std::cout << "  qty " << std::fixed << std::setprecision(0) << qty
              << ": buy YES @ " << std::setprecision(3) << yesPrice
              << " + NO @ " << noPrice
              << " = $" << std::setprecision(3) << opp.totalCost
              << " -> payout $" << std::setprecision(0) << opp.payout
              << " | net $" << std::setprecision(3) << opp.netProfit
              << " | roi " << std::setprecision(6)
              << (opp.returnOnCapital * 100.0) << "%\n";
    std::cout << "  tokens " << opp.market.yesAssetId.substr(0, 16)
              << "... (YES), " << opp.market.noAssetId.substr(0, 16)
              << "... (NO)";

    // Canonical market page. The condition-id URL form 404s, so use the
    // slug pair from the Gamma API instead.
    std::string url;

    if (!opp.market.slug.empty()) {
        if (!opp.market.eventSlug.empty() &&
            opp.market.eventSlug != opp.market.slug) {
            url = "https://polymarket.com/event/" + opp.market.eventSlug
                + "/" + opp.market.slug;
        } else {
            url = "https://polymarket.com/market/" + opp.market.slug;
        }
    }

    if (!url.empty()) {
        std::cout << " | " << url;
    }

    std::cout << "\n";
}

// Kept as a small registry for the periodic best-opportunity view, while the
// main event stream remains identical to the original logging behavior.
void reportImprovedOpportunity(const ArbitrageOpportunity&, double) {
}

void reportNewOpportunity(const ArbitrageOpportunity& opp) {
    reportOpportunity(opp);
}

struct TrackedOpportunity {
    ArbitrageOpportunity opportunity;
    double bestNet = 0.0;
    std::chrono::steady_clock::time_point lastSeen;
};

class OpportunityTracker {
public:
    explicit OpportunityTracker(double minNetProfit)
        : _minNetProfit(minNetProfit) {}

    void onOpportunities(const std::vector<ArbitrageOpportunity>& opportunities) {
        for (const auto& opp : opportunities) {
            if (opp.netProfit < _minNetProfit) {
                continue;
            }

            const std::string key = marketKey(opp.market);
            auto it = _registry.find(key);

            // Restore the original streaming behavior: report every
            // opportunity emitted by the detector, including repeated updates
            // for the same market.
            reportNewOpportunity(opp);

            if (it == _registry.end()) {
                TrackedOpportunity tracked;
                tracked.opportunity = opp;
                tracked.bestNet = opp.netProfit;
                tracked.lastSeen = std::chrono::steady_clock::now();
                _registry.emplace(key, std::move(tracked));
            } else {
                if (opp.netProfit > it->second.bestNet) {
                    it->second.bestNet = opp.netProfit;
                    it->second.opportunity = opp;
                }
                it->second.lastSeen = std::chrono::steady_clock::now();
            }
        }

        prune();
    }

    std::vector<const TrackedOpportunity*> currentBest() const {
        std::vector<const TrackedOpportunity*> result;

        for (const auto& [key, tracked] : _registry) {
            (void)key;
            result.push_back(&tracked);
        }

        std::sort(
            result.begin(),
            result.end(),
            [](const TrackedOpportunity* a, const TrackedOpportunity* b) {
                return a->bestNet > b->bestNet;
            }
        );

        return result;
    }

private:
    void prune() {
        const auto now = std::chrono::steady_clock::now();

        for (auto it = _registry.begin(); it != _registry.end();) {
            if (now - it->second.lastSeen > std::chrono::minutes(2)) {
                it = _registry.erase(it);
            } else {
                ++it;
            }
        }
    }

    double _minNetProfit;
    std::unordered_map<std::string, TrackedOpportunity> _registry;
};

// ---------------------------------------------------------------------------
// Hot-path statistics + periodic snapshot
// ---------------------------------------------------------------------------

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
};

void reportClosestMarkets(const ArbitrageEngine& engine) {
    struct Closest {
        double combined;
        std::string label;
    };

    std::vector<Closest> closest;

    for (const Market& market : engine.markets()) {
        double combined = engine.combinedBestAsk(market);

        if (combined > 0.0) {
            closest.push_back({combined, shortLabel(market, 26)});
        }
    }

    std::sort(
        closest.begin(),
        closest.end(),
        [](const Closest& a, const Closest& b) {
            return a.combined < b.combined;
        }
    );

    std::cout << "[closest to arb] ";

    for (std::size_t i = 0; i < std::min<std::size_t>(3, closest.size()); ++i) {
        std::cout << std::fixed << std::setprecision(3)
                  << closest[i].combined << " \"" << closest[i].label << "\"";

        if (i + 1 < std::min<std::size_t>(3, closest.size())) {
            std::cout << ", ";
        }
    }

    std::cout << "  (combined YES+NO best ask; < 1.00 = tradeable)\n";
}

void reportCurrentBest(const OpportunityTracker& tracker) {
    const auto best = tracker.currentBest();

    std::cout << "    best    :";

    if (best.empty()) {
        std::cout << " none right now\n";
        return;
    }

    std::cout << "\n";

    for (std::size_t i = 0; i < std::min<std::size_t>(3, best.size()); ++i) {
        const TrackedOpportunity& t = *best[i];

        std::cout << "      #" << (i + 1) << "  net $" << std::fixed
                  << std::setprecision(4) << t.bestNet << " ("
                  << std::setprecision(2) << (t.opportunity.returnOnCapital * 100.0)
                  << "%)  " << shortLabel(t.opportunity.market, 44) << "\n";
    }
}

void reportSnapshot(
    Stats& stats,
    const ArbitrageEngine& engine,
    const OpportunityTracker& tracker
) {
    (void)tracker;
    const auto now = std::chrono::steady_clock::now();

    if (now - stats.lastReport < std::chrono::seconds(kSnapshotSeconds)) {
        return;
    }

    const double avgUs =
        stats.messages > 0 ? stats.totalLatencyUs / stats.messages : 0.0;
    const double rate =
        stats.messages /
        std::chrono::duration<double>(now - stats.lastReport).count();

    std::cout << "[stats] " << stats.messages
              << " messages (" << std::fixed << std::setprecision(4)
              << rate << "/s), avg " << std::setprecision(4) << avgUs
              << "us, max " << std::setprecision(1)
              << stats.maxLatencyUs << "us\n";

    reportClosestMarkets(engine);

    stats = Stats{};
}

} // namespace

int main(int argc, char* argv[]) {
    // Tunable parameters:
    //   prediction_market [marketCount] [quantity] [minNetProfit]
    std::size_t marketCount = 500;
    double quantity = 1.0;
    double minNetProfit = 0.0;

    if (argc > 1) {
        marketCount = std::stoul(argv[1]);
    }

    if (argc > 2) {
        quantity = std::stod(argv[2]);
    }

    if (argc > 3) {
        minNetProfit = std::stod(argv[3]);
    }


    // Polymarket currently charges 0 trading fees; set a rate to model costs.
    const double feeRate = 0.0;

    // --- 1. Market discovery (Gamma API) ---
    std::vector<Market> markets;

    try {
        ClobRestClient gammaClient("gamma-api.polymarket.com");
        MarketLoader loader(gammaClient);

        markets = loader.loadMarkets(marketCount);

        std::cout << "Discovered " << markets.size()
                  << " active markets\n";
        std::cout << "Requested quantity per market: " << quantity
                  << ", fee rate: " << feeRate << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[error] market discovery failed: " << e.what() << "\n";
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

    ArbitrageEngine engine(markets, quantity, feeRate);

    // --- 2. Initialize every book from REST before the socket starts ---
    ClobRestClient clob("clob.polymarket.com");

    // Fetches every book in parallel (each REST call opens its own
    // connection, so a shared client is safe across threads), then applies
    // the snapshots to the engine single-threaded. Bounded by a deadline:
    // missing books are repopulated by the WebSocket initial dump, so a
    // slow or rate-limited API must not block startup forever.
    auto initializeBooks = [&]() {
        const std::vector<std::string> assetIds = engine.assetIds();

        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(45);

        std::vector<nlohmann::json> books(assetIds.size());
        std::vector<bool> ok(assetIds.size(), false);

        std::atomic<std::size_t> next{0};

        auto worker = [&]() {
            while (true) {
                if (std::chrono::steady_clock::now() > deadline) {
                    break;
                }

                std::size_t i = next.fetch_add(1);

                if (i >= assetIds.size()) {
                    break;
                }

                try {
                    books[i] = clob.getOrderBook(assetIds[i]);
                    ok[i] = true;
                }
                catch (const std::exception&) {
                    // Leave ok[i] = false; the book is skipped this round.
                }
            }
        };

        std::vector<std::thread> threads;
        threads.reserve(kBookWorkers);

        for (unsigned t = 0; t < kBookWorkers; ++t) {
            threads.emplace_back(worker);
        }

        for (auto& thread : threads) {
            thread.join();
        }

        std::size_t initialized = 0;

        for (std::size_t i = 0; i < assetIds.size(); ++i) {
            if (!ok[i]) {
                continue;
            }

            engine.initializeBook(assetIds[i], books[i]);
            ++initialized;
        }

        return initialized;
    };

    try {
        std::cout << "Fetching initial books (" << engine.assetIds().size()
                  << " assets, " << kBookWorkers << " workers)...\n";

        std::size_t initialized = initializeBooks();

        std::cout << "Initialized " << initialized << " books\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[error] failed to initialize books: " << e.what() << "\n";
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
        std::cout << "[resync] re-fetching all books from REST...\n";
        initializeBooks();
    });

    // --- 4. Hot path: process, track, report ---
    Stats stats;
    OpportunityTracker tracker(minNetProfit);

    client.setOnMessage([&](const nlohmann::json& json) {
        auto start = std::chrono::steady_clock::now();

        auto opportunities = engine.processMessage(json);

        auto end = std::chrono::steady_clock::now();

        stats.record(
            std::chrono::duration<double, std::micro>(end - start).count());

        tracker.onOpportunities(opportunities);

        reportSnapshot(stats, engine, tracker);
    });

    std::cout << "Connecting...\n";

    if (!client.connect()) {
        std::cerr << "[error] connection failed\n";
        return 1;
    }

    std::cout << "Connection successful\n";

    // REST already initialized every book. Do not request another full
    // snapshot burst for all 1000 assets: the synchronous message handler can
    // fall behind that burst and the server closes the socket with 1013
    // "slow consumer: send buffer full". We only need incremental updates.
    client.subscribe(
        engine.assetIds(),
        "market",
        false,
        0,
        false
    );

    return 0;
}
