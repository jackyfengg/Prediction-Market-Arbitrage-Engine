#pragma once

#include <vector>

#include "ArbitrageOpportunity.hpp"

class OpportunityRanker {
public:
    std::vector<ArbitrageOpportunity> rank(std::vector<ArbitrageOpportunity> opportunities) const;
};