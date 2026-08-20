#include "OpportunityRanker.hpp"

#include <algorithm>

std::vector<ArbitrageOpportunity> OpportunityRanker::rank(std::vector<ArbitrageOpportunity> opportunities) const {
    std::sort(
        opportunities.begin(),
        opportunities.end(),
        [](const ArbitrageOpportunity& a, const ArbitrageOpportunity& b) {
            if (a.netProfit > b.netProfit) return true;
            if (a.netProfit < b.netProfit) return false;
            if (a.returnOnCapital > b.returnOnCapital) return true;
            if (a.returnOnCapital < b.returnOnCapital) return false;
            return a.quantity > b.quantity;
        }
    );

    return opportunities;
}