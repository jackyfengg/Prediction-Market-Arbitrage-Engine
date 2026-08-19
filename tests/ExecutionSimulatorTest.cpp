#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "ExecutionSimulator.hpp"

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

TEST(ExecutionSimulatorTest, FullFill) {
    Market market{"YES", "NO"};

    OrderBook yes;
    yes.applySnapshot(makeBook({level("0.40", "100")}));

    OrderBook no;
    no.applySnapshot(makeBook({level("0.40", "100")}));

    ExecutionSimulator simulator;

    auto result = simulator.simulate(market, makeBooks(yes, no), 50);

    EXPECT_DOUBLE_EQ(result.requestedQuantity, 50);
    EXPECT_DOUBLE_EQ(result.executedQuantity, 50);
    EXPECT_DOUBLE_EQ(result.fillRatio, 1.0);
    EXPECT_DOUBLE_EQ(result.yesCost, 20);
    EXPECT_DOUBLE_EQ(result.noCost, 20);
    EXPECT_DOUBLE_EQ(result.capitalRequired, 40);
    EXPECT_DOUBLE_EQ(result.payout, 50);
    EXPECT_DOUBLE_EQ(result.grossProfit, 10);
    EXPECT_DOUBLE_EQ(result.netProfit, 10);
    EXPECT_NEAR(result.returnOnCapital, 0.25, 1e-9);
}

TEST(ExecutionSimulatorTest, PartialFillLimitedByThinnerSide) {
    Market market{"YES", "NO"};

    OrderBook yes;
    yes.applySnapshot(makeBook({level("0.40", "100")}));

    // NO side only has 60 available.
    OrderBook no;
    no.applySnapshot(makeBook({level("0.40", "60")}));

    ExecutionSimulator simulator;

    auto result = simulator.simulate(market, makeBooks(yes, no), 100);

    EXPECT_DOUBLE_EQ(result.requestedQuantity, 100);
    EXPECT_DOUBLE_EQ(result.executedQuantity, 60);
    EXPECT_DOUBLE_EQ(result.fillRatio, 0.6);
    EXPECT_DOUBLE_EQ(result.yesCost, 24);
    EXPECT_DOUBLE_EQ(result.noCost, 24);
    EXPECT_DOUBLE_EQ(result.capitalRequired, 48);
    EXPECT_DOUBLE_EQ(result.payout, 60);
    EXPECT_DOUBLE_EQ(result.grossProfit, 12);
}

TEST(ExecutionSimulatorTest, NoLiquidityProducesNoFill) {
    Market market{"YES", "NO"};

    OrderBook yes;
    yes.applySnapshot(makeBook({level("0.40", "100")}));

    OrderBook no;
    no.applySnapshot(makeBook({}));

    ExecutionSimulator simulator;

    auto result = simulator.simulate(market, makeBooks(yes, no), 100);

    EXPECT_DOUBLE_EQ(result.executedQuantity, 0);
    EXPECT_DOUBLE_EQ(result.fillRatio, 0);
    EXPECT_DOUBLE_EQ(result.netProfit, 0);
}

TEST(ExecutionSimulatorTest, FeesReduceNetProfit) {
    Market market{"YES", "NO"};

    OrderBook yes;
    yes.applySnapshot(makeBook({level("0.40", "100")}));

    OrderBook no;
    no.applySnapshot(makeBook({level("0.40", "100")}));

    ExecutionSimulator simulator(FeeModel(0.01));

    auto result = simulator.simulate(market, makeBooks(yes, no), 100);

    // capital = 80, gross = 20, fees = 1% of 80 = 0.80
    EXPECT_DOUBLE_EQ(result.fees, 0.80);
    EXPECT_DOUBLE_EQ(result.grossProfit, 20);
    EXPECT_DOUBLE_EQ(result.netProfit, 19.20);
}

TEST(ExecutionSimulatorTest, SlippageIncluded) {
    Market market{"YES", "NO"};

    OrderBook yes;
    yes.applySnapshot(makeBook({
        level("0.40", "50"),
        level("0.45", "50")
    }));

    OrderBook no;
    no.applySnapshot(makeBook({level("0.40", "100")}));

    ExecutionSimulator simulator;

    auto result = simulator.simulate(market, makeBooks(yes, no), 100);

    // YES: 50 @ 0.40 + 50 @ 0.45 = 42.5, avg 0.425, slippage (0.025 * 100) = 2.5
    EXPECT_DOUBLE_EQ(result.yesCost, 42.5);
    EXPECT_NEAR(result.slippage, 2.5, 1e-9);
}

TEST(ExecutionSimulatorTest, MissingBookProducesNoFill) {
    Market market{"YES", "NO"};

    OrderBook yes;
    yes.applySnapshot(makeBook({level("0.40", "100")}));

    ExecutionSimulator simulator;

    auto result = simulator.simulate(market, makeBooks(yes, OrderBook()), 100);

    EXPECT_DOUBLE_EQ(result.executedQuantity, 0);
}
