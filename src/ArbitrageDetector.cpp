#include "ArbitrageDetector.hpp"

ArbitrageOpportunity ArbitrageDetector::checkBinaryArbitrage(const Market& market, const std::unordered_map<std::string, OrderBook>& books, double quantity) const {
    const OrderBook& yesOrderBook = books.at(market.yesAssetId);
    const OrderBook& noOrderBook = books.at(market.noAssetId);

    OrderBook::CalculationResult yesResult = yesOrderBook.calculateBuyCost(quantity);
    OrderBook::CalculationResult noResult = noOrderBook.calculateBuyCost(quantity);

    double executableQuantity = std::min(yesResult.quantity, noResult.quantity);

    if (executableQuantity == 0) {
        return {};
    }

    if (executableQuantity != yesResult.quantity) {
       yesResult = yesOrderBook.calculateBuyCost(executableQuantity);
    }

    if (executableQuantity != noResult.quantity) {
        noResult = noOrderBook.calculateBuyCost(executableQuantity);
    }

    // Buying one YES and one NO is profitable when their combined cost is below $1.
    double totalCost = yesResult.totalCost + noResult.totalCost;

    // Binary contracts settle at $1 if the outcome wins and $0 otherwise.
    // Buying one YES and one NO guarantees a $1 payout per pair,
    // so the total payout is equal to the number of complete pairs.
    double payout = executableQuantity;

    if (totalCost >= payout) return {};

    double grossProfit = payout - totalCost;

    return {executableQuantity, yesResult.totalCost, noResult.totalCost, totalCost, payout, grossProfit};
}

std::vector<ArbitrageOpportunity> ArbitrageDetector::scan(const std::vector<Market>& markets, const std::unordered_map<std::string, OrderBook>& books, double quantity) const {
    std::vector<ArbitrageOpportunity> Opportunities;

    for (const Market& market : markets) {
        ArbitrageOpportunity opportunity = checkBinaryArbitrage(market, books, quantity);

        if (opportunity.grossProfit > 0) {
            Opportunities.push_back(opportunity);
        }
    }

    return Opportunities;
}