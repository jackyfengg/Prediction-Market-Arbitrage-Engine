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