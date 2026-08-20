#include "ArbitrageDetector.hpp"

ArbitrageDetector::ArbitrageDetector(const FeeModel& feeModel) : _feeModel(feeModel) {}

ArbitrageOpportunity ArbitrageDetector::checkBinaryArbitrage(const Market& market, const std::unordered_map<std::string, OrderBook>& books, double quantity) const {
    auto yesIt = books.find(market.yesAssetId);
    auto noIt = books.find(market.noAssetId);

    if (yesIt == books.end() || noIt == books.end()) {
        return {};
    }

    const OrderBook& yesOrderBook = yesIt->second;
    const OrderBook& noOrderBook = noIt->second;

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

    double grossProfit = payout - totalCost;

    if (grossProfit >= 0) return {};

    double yesFee = _feeModel.calculateFee(yesResult.totalCost);
    double noFee = _feeModel.calculateFee(noResult.totalCost);
    double totalFees = yesFee + noFee;

    double netProfit = grossProfit - totalFees;

    if (netProfit <= 0) return {};

    ArbitrageOpportunity opportunity;
    opportunity.quantity = executableQuantity;
    opportunity.requestedQuantity = quantity;
    opportunity.yesCost = yesCost;
    opportunity.noCost = noCost;
    opportunity.totalCost = totalCost;
    opportunity.yesFee = yesFee;
    opportunity.noFee = noFee;
    opportunity.totalFees = totalFees;
    opportunity.slippage = yesResult.slippage + noResult.slippage;
    opportunity.payout = payout;
    opportunity.grossProfit = grossProfit;
    opportunity.netProfit = netProfit;
    opportunity.returnOnCapital = totalCost > 0 ? netProfit / totalCost : 0.0;
    opportunity.market = market;

    return opporuntiy;
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