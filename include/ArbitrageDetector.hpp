#pragma once

#include <unordered_map>
#include <vector>

#include "ArbitrageOpportunity.hpp"
#include "Market.hpp"
#include "OrderBook.hpp"

class ArbitrageDetector {
public:
    ArbitrageOpportunity checkBinaryArbitrage(const Market& market, const std::unordered_map<std::string, OrderBook>& books, double quantity) const; 
      
    std::vector<ArbitrageOpportunity> scan(const std::vector<Market>& markets, const std::unordered_map<std::string, OrderBook>& books, double quantity) const;
};