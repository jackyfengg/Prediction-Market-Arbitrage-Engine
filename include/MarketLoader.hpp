#pragma once

#include <cstddef>
#include <vector>

#include <nlohmann/json.hpp>

#include "Market.hpp"

class ClobRestClient;

std::vector<Market> parseMarkets(const nlohmann::json& json);

class MarketLoader{
public:

    explicit MarketLoader(ClobRestClient& client);

    std::vector<Market> loadMarkets(std::size_t limit = 100) const;

private:
    ClobRestClient& _client;
};