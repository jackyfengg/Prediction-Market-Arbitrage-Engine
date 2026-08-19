#pragma once

#include <vector>

#include "ArbitrageOpportunity.hpp"

// Orders a set of opportunities so the "best" one comes first.
// Ranking is by net profit, then return on capital, then executable quantity.
class OpportunityRanker {
public:
    std::vector<ArbitrageOpportunity> rank(
        std::vector<ArbitrageOpportunity> opportunities
    ) const;
};
