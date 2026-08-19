#pragma once

#include <unordered_map>

#include "FeeModel.hpp"
#include "Market.hpp"
#include "OrderBook.hpp"

// The realistic result of attempting to execute a trade: how much actually
// fills, how much capital is required, and the P&L after fees and slippage.
struct ExecutionResult {
    double requestedQuantity = 0.0;
    double executedQuantity = 0.0;
    double fillRatio = 0.0;          // executed / requested
    double yesCost = 0.0;
    double noCost = 0.0;
    double capitalRequired = 0.0;    // yesCost + noCost
    double yesFee = 0.0;
    double noFee = 0.0;
    double fees = 0.0;
    double slippage = 0.0;
    double payout = 0.0;
    double grossProfit = 0.0;
    double netProfit = 0.0;
    double returnOnCapital = 0.0;
};

// Answers the question: "If I actually attempted this trade against the
// current books, what would happen?" Distinguishes theoretical arbitrage
// (the detector) from executable arbitrage (this class).
class ExecutionSimulator {
public:
    explicit ExecutionSimulator(const FeeModel& feeModel = FeeModel());

    ExecutionResult simulate(
        const Market& market,
        const std::unordered_map<std::string, OrderBook>& books,
        double requestedQuantity
    ) const;

private:
    FeeModel _feeModel;
};
