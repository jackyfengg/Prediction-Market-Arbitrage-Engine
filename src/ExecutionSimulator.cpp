#include "ExecutionSimulator.hpp"

#include <algorithm>

ExecutionSimulator::ExecutionSimulator(const FeeModel& feeModel)
    : _feeModel(feeModel)
{
}

ExecutionResult ExecutionSimulator::simulate(
    const Market& market,
    const std::unordered_map<std::string, OrderBook>& books,
    double requestedQuantity
) const {
    ExecutionResult result;
    result.requestedQuantity = requestedQuantity;

    auto yesIt = books.find(market.yesAssetId);
    auto noIt = books.find(market.noAssetId);

    if (yesIt == books.end() || noIt == books.end() || requestedQuantity <= 0) {
        return result;
    }

    OrderBook::CalculationResult yesResult =
        yesIt->second.calculateBuyCost(requestedQuantity);
    OrderBook::CalculationResult noResult =
        noIt->second.calculateBuyCost(requestedQuantity);

    // The number of complete pairs we can actually buy is limited by the
    // thinner side of the two books (e.g. requested 100, NO liquidity 60 -> 60).
    double executedQuantity = std::min(yesResult.quantity, noResult.quantity);

    if (executedQuantity == 0) {
        return result;
    }

    if (executedQuantity != yesResult.quantity) {
        yesResult = yesIt->second.calculateBuyCost(executedQuantity);
    }

    if (executedQuantity != noResult.quantity) {
        noResult = noIt->second.calculateBuyCost(executedQuantity);
    }

    result.executedQuantity = executedQuantity;
    result.fillRatio = executedQuantity / requestedQuantity;
    result.yesCost = yesResult.totalCost;
    result.noCost = noResult.totalCost;
    result.capitalRequired = result.yesCost + result.noCost;
    result.yesFee = _feeModel.calculateFee(result.yesCost);
    result.noFee = _feeModel.calculateFee(result.noCost);
    result.fees = result.yesFee + result.noFee;
    result.slippage = yesResult.slippage + noResult.slippage;
    result.payout = executedQuantity;
    result.grossProfit = result.payout - result.capitalRequired;
    result.netProfit = result.grossProfit - result.fees;
    result.returnOnCapital =
        result.capitalRequired > 0 ? result.netProfit / result.capitalRequired : 0.0;

    return result;
}
