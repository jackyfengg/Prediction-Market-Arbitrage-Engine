#pragma once

#include <functional>
#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>

class OrderBook {
public:
    struct CalculationResult {
        double quantity;
        double totalCost;
        double averagePrice;
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