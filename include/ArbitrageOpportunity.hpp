#pragma once

// A complete trade candidate: what to buy, at what cost, and what the
// expected economics are after liquidity consumption (slippage) and fees.
struct ArbitrageOpportunity {
    double quantity = 0.0;           // executable quantity (complete YES/NO pairs)
    double requestedQuantity = 0.0;  // quantity originally requested (for fill ratio)
    double yesCost = 0.0;
    double noCost = 0.0;
    double totalCost = 0.0;
    double yesFee = 0.0;
    double noFee = 0.0;
    double totalFees = 0.0;
    double slippage = 0.0;           // cost of consuming liquidity across the books
    double payout = 0.0;             // $1 per complete pair
    double grossProfit = 0.0;        // payout - totalCost
    double netProfit = 0.0;          // grossProfit - totalFees
    double returnOnCapital = 0.0;    // netProfit / totalCost
};
