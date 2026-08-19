#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "OrderBook.hpp"

namespace {

nlohmann::json makeBook(
    const std::vector<nlohmann::json>& bids,
    const std::vector<nlohmann::json>& asks
) {
    nlohmann::json book;
    book["bids"] = bids;
    book["asks"] = asks;
    return book;
}

nlohmann::json level(const std::string& price, const std::string& size) {
    return {{"price", price}, {"size", size}};
}

} // namespace

TEST(OrderBookTest, BuyCostReportsBestAndAveragePrice) {
    OrderBook book;
    book.applySnapshot(makeBook(
        {},
        {
            level("0.40", "2"),
            level("0.41", "3"),
            level("0.43", "5")
        }
    ));

    // Buying 10 contracts: 2 @ 0.40 + 3 @ 0.41 + 5 @ 0.43 = 4.18
    auto result = book.calculateBuyCost(10);

    EXPECT_DOUBLE_EQ(result.quantity, 10);
    EXPECT_DOUBLE_EQ(result.totalCost, 4.18);
    EXPECT_NEAR(result.averagePrice, 0.418, 1e-9);
    EXPECT_DOUBLE_EQ(result.bestPrice, 0.40);

    // (0.418 - 0.40) * 10 = 0.18 of slippage from walking the book
    EXPECT_NEAR(result.slippage, 0.18, 1e-9);
}

TEST(OrderBookTest, BuySlippageIsZeroAtTopOfBook) {
    OrderBook book;
    book.applySnapshot(makeBook(
        {},
        {level("0.40", "100")}
    ));

    auto result = book.calculateBuyCost(10);

    EXPECT_DOUBLE_EQ(result.quantity, 10);
    EXPECT_DOUBLE_EQ(result.averagePrice, 0.40);
    EXPECT_DOUBLE_EQ(result.slippage, 0.0);
}

TEST(OrderBookTest, PartialFillReportsFilledQuantityAndSlippage) {
    OrderBook book;
    book.applySnapshot(makeBook(
        {},
        {
            level("0.40", "5"),
            level("0.42", "5")
        }
    ));

    // Only 10 available but 20 requested.
    auto result = book.calculateBuyCost(20);

    EXPECT_DOUBLE_EQ(result.quantity, 10);
    EXPECT_DOUBLE_EQ(result.totalCost, 4.10);
    EXPECT_NEAR(result.averagePrice, 0.41, 1e-9);
    EXPECT_DOUBLE_EQ(result.bestPrice, 0.40);
    EXPECT_NEAR(result.slippage, 0.10, 1e-9);
}

TEST(OrderBookTest, BuyCostOnEmptyBook) {
    OrderBook book;
    book.applySnapshot(makeBook({}, {}));

    auto result = book.calculateBuyCost(10);

    EXPECT_DOUBLE_EQ(result.quantity, 0);
    EXPECT_DOUBLE_EQ(result.totalCost, 0);
    EXPECT_DOUBLE_EQ(result.averagePrice, 0);
    EXPECT_DOUBLE_EQ(result.bestPrice, 0);
    EXPECT_DOUBLE_EQ(result.slippage, 0);
}

TEST(OrderBookTest, SellRevenueReportsSlippage) {
    OrderBook book;
    book.applySnapshot(makeBook(
        {
            level("0.43", "5"),
            level("0.41", "3"),
            level("0.40", "2")
        },
        {}
    ));

    // Selling 10: 5 @ 0.43 + 3 @ 0.41 + 2 @ 0.40 = 4.18
    auto result = book.calculateSellRevenue(10);

    EXPECT_DOUBLE_EQ(result.quantity, 10);
    EXPECT_DOUBLE_EQ(result.totalCost, 4.18);
    EXPECT_NEAR(result.averagePrice, 0.418, 1e-9);
    EXPECT_DOUBLE_EQ(result.bestPrice, 0.43);

    // (0.43 - 0.418) * 10 = 0.12
    EXPECT_NEAR(result.slippage, 0.12, 1e-9);
}

TEST(OrderBookTest, PriceChangeErasesLevelWhenSizeIsZero) {
    OrderBook book;
    book.applySnapshot(makeBook(
        {},
        {
            level("0.40", "5"),
            level("0.42", "5")
        }
    ));

    book.applyPriceChange({
        {"price", "0.40"},
        {"size", "0"},
        {"side", "SELL"}
    });

    auto result = book.calculateBuyCost(20);

    EXPECT_DOUBLE_EQ(result.quantity, 5);
    EXPECT_DOUBLE_EQ(result.bestPrice, 0.42);
}
