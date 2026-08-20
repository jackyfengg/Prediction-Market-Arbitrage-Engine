#include "OrderBook.hpp"

void OrderBook::applySnapshot(const nlohmann::json& json) {
    _bids.clear();
    _asks.clear();

    for (auto& bid : json["bids"]) {
        double price = std::stod(bid["price"].get<std::string>());
        double size = std::stod(bid["size"].get<std::string>());

        _bids[price] = size;
    }

    for (auto& ask : json["asks"]) {
        double price = std::stod(ask["price"].get<std::string>());
        double size = std::stod(ask["size"].get<std::string>());

        _asks[price] = size;
    }
}

void OrderBook::applyPriceChange(const nlohmann::json& change) {
    double price = std::stod(change["price"].get<std::string>());
    double size = std::stod(change["size"].get<std::string>());

    const std::string side = change["side"].get<std::string>();

    if (side == "BUY") {
        if (size == 0) {
            _bids.erase(price);
        } else {
            _bids[price] = size;
        }
    }
    else if (side == "SELL") {
        if (size == 0) {
            _asks.erase(price);
        } else {
            _asks[price] = size;
        }
    }
}

double OrderBook::getBestBid() const {
    if (_bids.empty()) return 0.0;

    return _bids.begin()->first;
}

double OrderBook::getBestAsk() const {
    if (_asks.empty()) return 0.0;

    return _asks.begin()->first;
}

OrderBook::CalculationResult OrderBook::calculateBuyCost(double quantity) const {
    double unfilled = quantity, totalCost = 0;
    double bestPrice = _asks.empty() ? 0.0 : _asks.begin()->first;

    for (const auto& [price, amountofsellers] : _asks) {
        double filled = std::min(unfilled, amountofsellers);

        totalCost += filled * price;
        unfilled -= filled;

        // Unfilled will never go negative since we have min(unfilled, amountofsellers)
        if (unfilled == 0) break;
    }

    double filledQuantity = quantity - unfilled;
    double averagePrice = filledQuantity > 0 ? totalCost / filledQuantity : 0.0;
    double slippage = (averagePrice - bestPrice) * filledQuantity;

    return {filledQuantity, totalCost, averagePrice, bestPrice, slippage};
}

OrderBook::CalculationResult OrderBook::calculateSellRevenue(double quantity) const {
    double unfilled = quantity, totalRevenue = 0;

    double bestPrice = _bids.empty() ? 0.0 : _bids.begin()->first;

    for (const auto& [price, amountofbuyers] : _bids) {
        double filled = std::min(unfilled, amountofbuyers);

        totalRevenue += filled * price;
        unfilled -= filled;

        if (unfilled == 0) break;
    }

    double filledQuantity = quantity - unfilled;
    double averagePrice = filledQuantity > 0 ? totalRevenue / filledQuantity : 0.0;

    // Selling walks down the bid ladder, so the average execution price is
    // <= the best bid. The gap times the filled quantity is the liquidity cost.
    double slippage = (bestPrice - averagePrice) * filledQuantity;

    return {filledQuantity, totalRevenue, averagePrice, bestPrice, slippage};
}
