#pragma once

#include <functional>
#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>

class OrderBook {
public:
    struct CalculationResult {
        double quantity = 0.0;    // how much was actually filled
        double totalCost = 0.0;   // total paid for the filled quantity
        double averagePrice = 0.0;
        double bestPrice = 0.0;   // best (top-of-book) price on the relevant side
        double slippage = 0.0;    // cost of consuming liquidity: (avg - best) * filled for buys
    };

    void applySnapshot(const nlohmann::json& json);
    void applyPriceChange(const nlohmann::json& json);

    double getBestBid();
    double getBestAsk();

    CalculationResult calculateBuyCost(double quantity) const;
    CalculationResult calculateSellRevenue(double quantity) const;

private:
    std::map<double, double, std::greater<double>> _bids;
    std::map<double, double> _asks;
};