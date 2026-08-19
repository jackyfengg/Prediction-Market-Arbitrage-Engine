#include <gtest/gtest.h>

#include "OpportunityRanker.hpp"

namespace {

ArbitrageOpportunity makeOpportunity(
    double quantity,
    double netProfit,
    double totalCost
) {
    ArbitrageOpportunity opp;
    opp.quantity = quantity;
    opp.netProfit = netProfit;
    opp.totalCost = totalCost;
    opp.returnOnCapital = totalCost > 0 ? netProfit / totalCost : 0.0;
    return opp;
}

} // namespace

TEST(OpportunityRankerTest, RanksByNetProfit) {
    OpportunityRanker ranker;

    auto opportunities = ranker.rank({
        makeOpportunity(10, 2.0, 100.0),
        makeOpportunity(10, 5.0, 100.0)
    });

    ASSERT_EQ(opportunities.size(), 2);
    EXPECT_DOUBLE_EQ(opportunities[0].netProfit, 5.0);
    EXPECT_DOUBLE_EQ(opportunities[1].netProfit, 2.0);
}

TEST(OpportunityRankerTest, RanksByReturnOnCapitalAsTieBreak) {
    OpportunityRanker ranker;

    // Same net profit, but B needs far less capital.
    auto opportunities = ranker.rank({
        makeOpportunity(10, 2.0, 200.0),
        makeOpportunity(10, 2.0, 10.0)
    });

    ASSERT_EQ(opportunities.size(), 2);
    EXPECT_DOUBLE_EQ(opportunities[0].quantity, 10);
    EXPECT_DOUBLE_EQ(opportunities[0].totalCost, 10.0);
    EXPECT_DOUBLE_EQ(opportunities[1].totalCost, 200.0);
}

TEST(OpportunityRankerTest, SortsMultipleOpportunities) {
    OpportunityRanker ranker;

    auto opportunities = ranker.rank({
        makeOpportunity(100, 1.0, 50.0),   // roi 0.02
        makeOpportunity(50, 5.0, 100.0),   // roi 0.05
        makeOpportunity(20, 3.0, 10.0)     // roi 0.30
    });

    ASSERT_EQ(opportunities.size(), 3);
    EXPECT_DOUBLE_EQ(opportunities[0].netProfit, 5.0);
    EXPECT_DOUBLE_EQ(opportunities[1].netProfit, 3.0);
    EXPECT_DOUBLE_EQ(opportunities[2].netProfit, 1.0);
}

TEST(OpportunityRankerTest, EmptyList) {
    OpportunityRanker ranker;

    auto opportunities = ranker.rank({});

    EXPECT_TRUE(opportunities.empty());
}
