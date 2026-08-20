#pragma once

#include <functional>
#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>

class OrderBook {
public:
    struct CalculationResult {
        double quantity = 0.0;
        double totalCost = 0.0;
        double averagePrice = 0.0;
        double bestPrice = 0.0;
        double slippage = 0.0;
    };

    void applySnapshot(const nlohmann::json& json);
    void applyPriceChange(const nlohmann::json& json);

    double getBestBid() const;
    double getBestAsk() const;

    CalculationResult calculateBuyCost(double quantity) const;
    CalculationResult calculateSellRevenue(double quantity) const;


private:
    std::map<double, double, std::greater<double>> _bids;
    std::map<double, double> _asks;
};