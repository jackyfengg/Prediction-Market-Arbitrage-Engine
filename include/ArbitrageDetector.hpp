#pragma once

#include <unordered_map>
#include <vector>

#include "ArbitrageOpportunity.hpp"
#include "FeeModel.hpp"
#include "Market.hpp"
#include "OrderBook.hpp"

// Decides whether a market has an executable binary arbitrage (YES + NO
// costing less than $1) and computes its economics: costs, slippage, fees,
// gross/net profit, and return on capital.
class ArbitrageDetector {
public:
    explicit ArbitrageDetector(const FeeModel& feeModel = FeeModel());

    ArbitrageOpportunity checkBinaryArbitrage(
        const Market& market,
        const std::unordered_map<std::string, OrderBook>& books,
        double quantity
    ) const;

    std::vector<ArbitrageOpportunity> scan(
        const std::vector<Market>& markets,
        const std::unordered_map<std::string, OrderBook>& books,
        double quantity
    ) const;

private:
    FeeModel _feeModel;
};
