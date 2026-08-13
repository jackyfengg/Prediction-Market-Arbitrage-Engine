#pragma once

#include <map>
#include <functional>
#include <nlohmann/json.hpp>

class OrderBook {
    public:
        void applySnapshot(const nlohmann::json& json);
        void applyPriceChange(const nlohmann::json& json);

    private:
        std::map<double, double> _bids;
        std::map<double, double, std::greater<double>> _asks;
};