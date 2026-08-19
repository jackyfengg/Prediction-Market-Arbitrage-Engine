#pragma once

#include <cstddef>
#include <vector>

#include <nlohmann/json.hpp>

#include "Market.hpp"

class ClobRestClient;

// Converts a Gamma API markets response into usable markets. Pure function
// (no network) so it can be unit-tested with synthetic JSON.
//
// Markets are filtered out when they are inactive, closed, not accepting
// orders, missing YES/NO token IDs, or have zero liquidity.
std::vector<Market> parseMarkets(const nlohmann::json& json);

// Market discovery layer: fetches active markets from the Gamma API and
// extracts the clobTokenIds (asset IDs) needed to subscribe to books.
class MarketLoader {
public:
    explicit MarketLoader(ClobRestClient& client);

    std::vector<Market> loadMarkets(std::size_t limit = 100) const;

private:
    ClobRestClient& _client;
};
