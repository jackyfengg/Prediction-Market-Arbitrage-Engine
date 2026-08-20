#pragma once

#include <vector>

#include "ArbitrageOpportunity.hpp"

class OpportuntiyRanker {
public:
    std::vector<ArbitrageOpportunity> rank(std::vector<ArbitrageOpportunity> opportunites) const;
};