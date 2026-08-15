#pragma once

#include <unordered_map>

#include "ArbitrageOpportunity.hpp"
#include "Market.hpp"
#include "OrderBook.hpp"

class ArbitrageDetector {
public:
    ArbitrageOpportunity checkBinaryArbitrage(const Market& market, const std::unordered_map<std::string, OrderBook>& books, double quantity) const;    
};