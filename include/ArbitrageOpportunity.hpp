#pragma once

#include "Market.hpp"

struct ArbitrageOpportunity {
    double quantity = 0.0;
    double requestedQuantity = 0.0;
    double yesCost = 0.0;
    double noCost = 0.0;
    double totalCost = 0.0;
    double yesFee = 0.0;
    double noFee = 0.0;
    double totalFees = 0.0;
    double slippage = 0.0;
    double payout = 0.0;
    double grossProfit = 0.0;
    double netProfit = 0.0;
    double returnOnCapital = 0.0;

    Market market;
};