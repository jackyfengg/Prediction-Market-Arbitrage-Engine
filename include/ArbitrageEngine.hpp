# pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "OrderBook.hpp"
#include "Market.hpp"
#include "ArbitrageDetector.hpp"

class ArbitrageEngine{
public:
    ArbitrageEngine(const std::vector<Market>& markets, double quantity);

    std::vector<ArbitrageOpportunity> processMessage(const nlohmann::json& josn);

private:
    std::vector<Market> _markets;
    std::unordered_map<std::string, OrderBook> _books;
    ArbitrageDetector _detector;
    double _quantity;
    std::unordered_map<std::string, Market> _assetToMarket;
};  

