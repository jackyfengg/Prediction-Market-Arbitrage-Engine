#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "ArbitrageEngine.hpp"

namespace {

nlohmann::json bookSnapshot(
    const std::string& assetId,
    const std::string& askPrice
) {
    nlohmann::json book;
    book["event_type"] = "book";
    book["asset_id"] = assetId;
    book["bids"] = nlohmann::json::array({{{"price", "0.40"}, {"size", "100"}}});
    book["asks"] = nlohmann::json::array({{{"price", askPrice}, {"size", "100"}}});
    return book;
}

} // namespace

TEST(ArbitrageEnginePipelineTest, OpportunitiesAreRankedByNetProfitAcrossMarkets) {
    Market market1{"YES1", "NO1"};  // 0.40 + 0.40 -> net 20 per 100
    Market market2{"YES2", "NO2"};  // 0.45 + 0.45 -> net 10 per 100

    ArbitrageEngine engine({market1, market2}, 100.0);

    EXPECT_TRUE(engine.processMessage(bookSnapshot("YES1", "0.40")).empty());
    EXPECT_TRUE(engine.processMessage(bookSnapshot("YES2", "0.45")).empty());

    auto afterNo1 = engine.processMessage(bookSnapshot("NO1", "0.40"));
    ASSERT_EQ(afterNo1.size(), 1);
    EXPECT_DOUBLE_EQ(afterNo1[0].netProfit, 20.0);

    auto afterNo2 = engine.processMessage(bookSnapshot("NO2", "0.45"));
    ASSERT_EQ(afterNo2.size(), 1);
    EXPECT_DOUBLE_EQ(afterNo2[0].netProfit, 10.0);

    // Both markets are live; the returned list must be ranked by net profit.
    auto opportunities = engine.processMessage({
        {"event_type", "price_change"},
        {"price_changes", {
            {
                {"asset_id", "NO1"},
                {"price", "0.40"},
                {"size", "100"},
                {"side", "SELL"}
            },
            {
                {"asset_id", "NO2"},
                {"price", "0.45"},
                {"size", "100"},
                {"side", "SELL"}
            }
        }}
    });

    ASSERT_EQ(opportunities.size(), 2);

    EXPECT_DOUBLE_EQ(opportunities[0].netProfit, 20.0);
    EXPECT_DOUBLE_EQ(opportunities[0].quantity, 100.0);
    EXPECT_DOUBLE_EQ(opportunities[1].netProfit, 10.0);
}

TEST(ArbitrageEnginePipelineTest, FeesEliminateOpportunityEndToEnd) {
    Market market{"YES", "NO"};

    // Gross profit is 4.00, but a 5% fee on 96 notional is 4.80.
    ArbitrageEngine engine({market}, 100.0, /*feeRate=*/0.05);

    EXPECT_TRUE(engine.processMessage(bookSnapshot("YES", "0.48")).empty());

    auto result = engine.processMessage(bookSnapshot("NO", "0.48"));

    EXPECT_TRUE(result.empty());
}

TEST(ArbitrageEnginePipelineTest, CombinedBestAskMonitorsDistanceToArbitrage) {
    Market market{"YES", "NO"};

    ArbitrageEngine engine({market}, 100.0);

    // No books yet -> 0.
    EXPECT_DOUBLE_EQ(engine.combinedBestAsk(market), 0.0);

    engine.processMessage(bookSnapshot("YES", "0.40"));

    // Only one side loaded -> 0.
    EXPECT_DOUBLE_EQ(engine.combinedBestAsk(market), 0.0);

    engine.processMessage(bookSnapshot("NO", "0.45"));

    EXPECT_NEAR(engine.combinedBestAsk(market), 0.85, 1e-9);
}

TEST(ArbitrageEnginePipelineTest, SimulatedExecutionPopulatesFullEconomics) {
    Market market{"YES", "NO"};

    ArbitrageEngine engine({market}, 100.0, /*feeRate=*/0.01);

    EXPECT_TRUE(engine.processMessage(bookSnapshot("YES", "0.40")).empty());

    auto result = engine.processMessage(bookSnapshot("NO", "0.40"));

    ASSERT_EQ(result.size(), 1);

    const ArbitrageOpportunity& opp = result[0];

    EXPECT_DOUBLE_EQ(opp.quantity, 100.0);
    EXPECT_DOUBLE_EQ(opp.requestedQuantity, 100.0);
    EXPECT_DOUBLE_EQ(opp.yesCost, 40.0);
    EXPECT_DOUBLE_EQ(opp.noCost, 40.0);
    EXPECT_DOUBLE_EQ(opp.totalCost, 80.0);
    EXPECT_DOUBLE_EQ(opp.yesFee, 0.40);
    EXPECT_DOUBLE_EQ(opp.noFee, 0.40);
    EXPECT_DOUBLE_EQ(opp.totalFees, 0.80);
    EXPECT_DOUBLE_EQ(opp.slippage, 0.0);
    EXPECT_DOUBLE_EQ(opp.payout, 100.0);
    EXPECT_DOUBLE_EQ(opp.grossProfit, 20.0);
    EXPECT_DOUBLE_EQ(opp.netProfit, 19.20);
    EXPECT_NEAR(opp.returnOnCapital, 19.20 / 80.0, 1e-9);

    // The opportunity must carry enough identity to report what to buy.
    EXPECT_EQ(opp.market.yesAssetId, "YES");
    EXPECT_EQ(opp.market.noAssetId, "NO");
}
