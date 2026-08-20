#pragma once

#include <unordered_map>

#include "FeeModel.hpp"
#include "Market.hpp"
#include "OrderBook.hpp"

struct ExecutionResult {
    double requestedQuantity = 0.0;
    double executedQuantity = 0.0;
    double fillRatio = 0.0;          
    double yesCost = 0.0;
    double noCost = 0.0;
    double capitalRequired = 0.0;    
    double yesFee = 0.0;
    double noFee = 0.0;
    double fees = 0.0;
    double slippage = 0.0;
    double payout = 0.0;
    double grossProfit = 0.0;
    double netProfit = 0.0;
    double returnOnCapital = 0.0;
};

class ExecutionSimulator {
public:
    explicit ExecutionSimulator(const FeeModel& feeModel = FeeModel());

    ExecutionResult simulate(const Market& market, const std::unordered_map<std::string, OrderBook>& books, double requestedQuantity) const;

private:
    FeeModel _feeModel;
};