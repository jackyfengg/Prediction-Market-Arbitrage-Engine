#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "ArbitrageDetector.hpp"

namespace {

nlohmann::json makeBook(const std::vector<nlohmann::json>& asks) {
    nlohmann::json book;
    book["bids"] = nlohmann::json::array();
    book["asks"] = asks;
    return book;
}

nlohmann::json level(const std::string& price, const std::string& size) {
    return {{"price", price}, {"size", size}};
}

std::unordered_map<std::string, OrderBook> makeBooks(
    const OrderBook& yes,
    const OrderBook& no
) {
    return {{"YES", yes}, {"NO", no}};
}

} // namespace

TEST(ArbitrageDetectorEconomicsTest, NetProfitReflectsFees) {
    Market market{"YES", "NO"};

    OrderBook yes;
    yes.applySnapshot(makeBook({level("0.40", "100")}));

    OrderBook no;
    no.applySnapshot(makeBook({level("0.40", "100")}));

    ArbitrageDetector detector(FeeModel(0.01));

    auto result = detector.checkBinaryArbitrage(market, makeBooks(yes, no), 100);

    EXPECT_DOUBLE_EQ(result.quantity, 100);
    EXPECT_DOUBLE_EQ(result.totalCost, 80);
    EXPECT_DOUBLE_EQ(result.totalFees, 0.80);
    EXPECT_DOUBLE_EQ(result.grossProfit, 20);
    EXPECT_DOUBLE_EQ(result.netProfit, 19.20);
    EXPECT_NEAR(result.returnOnCapital, 19.20 / 80.0, 1e-9);
}

TEST(ArbitrageDetectorEconomicsTest, FeesCanEliminateApparentArbitrage) {
    Market market{"YES", "NO"};

    // Gross profit is positive (0.48 + 0.48 = 0.96 < 1.00)...
    OrderBook yes;
    yes.applySnapshot(makeBook({level("0.48", "100")}));

    OrderBook no;
    no.applySnapshot(makeBook({level("0.48", "100")}));

    // ...but a 5% fee on 96 notional (4.80) exceeds the 4.00 gross profit.
    ArbitrageDetector detector(FeeModel(0.05));

    auto result = detector.checkBinaryArbitrage(market, makeBooks(yes, no), 100);

    EXPECT_DOUBLE_EQ(result.quantity, 0);
    EXPECT_DOUBLE_EQ(result.netProfit, 0);

    // Without fees the same market is profitable.
    ArbitrageDetector noFeeDetector;
    auto profitable = noFeeDetector.checkBinaryArbitrage(
        market, makeBooks(yes, no), 100);

    EXPECT_DOUBLE_EQ(profitable.quantity, 100);
    EXPECT_DOUBLE_EQ(profitable.grossProfit, 4.0);
}

TEST(ArbitrageDetectorEconomicsTest, SlippageReportedOnOpportunity) {
    Market market{"YES", "NO"};

    OrderBook yes;
    yes.applySnapshot(makeBook({
        level("0.40", "50"),
        level("0.45", "50")
    }));

    OrderBook no;
    no.applySnapshot(makeBook({level("0.40", "100")}));

    ArbitrageDetector detector;

    auto result = detector.checkBinaryArbitrage(market, makeBooks(yes, no), 100);

    // YES side: avg 0.425 vs best 0.40 -> (0.025 * 100) = 2.5 slippage.
    EXPECT_DOUBLE_EQ(result.quantity, 100);
    EXPECT_DOUBLE_EQ(result.yesCost, 42.5);
    EXPECT_NEAR(result.slippage, 2.5, 1e-9);
}

TEST(ArbitrageDetectorEconomicsTest, ScanFiltersOutFeeEliminatedMarkets) {
    Market profitable{"YES1", "NO1"};
    Market feeEliminated{"YES2", "NO2"};

    OrderBook yes1;
    yes1.applySnapshot(makeBook({level("0.40", "100")}));
    OrderBook no1;
    no1.applySnapshot(makeBook({level("0.40", "100")}));

    OrderBook yes2;
    yes2.applySnapshot(makeBook({level("0.48", "100")}));
    OrderBook no2;
    no2.applySnapshot(makeBook({level("0.48", "100")}));

    std::unordered_map<std::string, OrderBook> books{
        {"YES1", yes1}, {"NO1", no1},
        {"YES2", yes2}, {"NO2", no2}
    };

    ArbitrageDetector detector(FeeModel(0.05));

    auto opportunities = detector.scan(
        {profitable, feeEliminated}, books, 100);

    ASSERT_EQ(opportunities.size(), 1);
    EXPECT_DOUBLE_EQ(opportunities[0].yesCost, 40);
}
