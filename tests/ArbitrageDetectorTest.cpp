#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "ArbitrageDetector.hpp"

TEST(ArbitrageDetectorTest, ProfitableArbitrage) {
    Market market{"YES", "NO"};

    OrderBook yesBook;
    yesBook.applySnapshot({
        {"bids", nlohmann::json::array()},
        {"asks", {
            {{"price", "0.40"}, {"size", "100"}}
        }}
    });

    OrderBook noBook;
    noBook.applySnapshot({
        {"bids", nlohmann::json::array()},
        {"asks", {
            {{"price", "0.40"}, {"size", "100"}}
        }}
    });

    std::unordered_map<std::string, OrderBook> books{
        {"YES", yesBook},
        {"NO", noBook}
    };

    ArbitrageDetector detector;

    auto result = detector.checkBinaryArbitrage(market, books, 100);

    EXPECT_DOUBLE_EQ(result.quantity, 100);
    EXPECT_DOUBLE_EQ(result.yesCost, 40);
    EXPECT_DOUBLE_EQ(result.noCost, 40);
    EXPECT_DOUBLE_EQ(result.totalCost, 80);
    EXPECT_DOUBLE_EQ(result.payout, 100);
    EXPECT_DOUBLE_EQ(result.grossProfit, 20);
}

TEST(ArbitrageDetectorTest, UsesSmallerExecutableQuantity) {
    Market market{"YES", "NO"};

    OrderBook yesBook;
    yesBook.applySnapshot({
        {"bids", nlohmann::json::array()},
        {"asks", {
            {{"price", "0.40"}, {"size", "100"}}
        }}
    });

    OrderBook noBook;
    noBook.applySnapshot({
        {"bids", nlohmann::json::array()},
        {"asks", {
            {{"price", "0.40"}, {"size", "20"}}
        }}
    });

    std::unordered_map<std::string, OrderBook> books{
        {"YES", yesBook},
        {"NO", noBook}
    };

    ArbitrageDetector detector;

    auto result = detector.checkBinaryArbitrage(market, books, 100);

    EXPECT_DOUBLE_EQ(result.quantity, 20);
    EXPECT_DOUBLE_EQ(result.yesCost, 8);
    EXPECT_DOUBLE_EQ(result.noCost, 8);
    EXPECT_DOUBLE_EQ(result.totalCost, 16);
    EXPECT_DOUBLE_EQ(result.payout, 20);
    EXPECT_DOUBLE_EQ(result.grossProfit, 4);
}

TEST(ArbitrageDetectorTest, NoArbitrageWhenCostEqualsPayout) {
    Market market{"YES", "NO"};

    OrderBook yesBook;
    yesBook.applySnapshot({
        {"bids", nlohmann::json::array()},
        {"asks", {
            {{"price", "0.50"}, {"size", "100"}}
        }}
    });

    OrderBook noBook;
    noBook.applySnapshot({
        {"bids", nlohmann::json::array()},
        {"asks", {
            {{"price", "0.50"}, {"size", "100"}}
        }}
    });

    std::unordered_map<std::string, OrderBook> books{
        {"YES", yesBook},
        {"NO", noBook}
    };

    ArbitrageDetector detector;

    auto result = detector.checkBinaryArbitrage(market, books, 100);

    EXPECT_DOUBLE_EQ(result.quantity, 0);
    EXPECT_DOUBLE_EQ(result.grossProfit, 0);
}

TEST(ArbitrageDetectorTest, NoArbitrageWhenOneSideHasNoLiquidity) {
    Market market{"YES", "NO"};

    OrderBook yesBook;
    yesBook.applySnapshot({
        {"bids", nlohmann::json::array()},
        {"asks", {
            {{"price", "0.40"}, {"size", "100"}}
        }}
    });

    OrderBook noBook;
    noBook.applySnapshot({
        {"bids", nlohmann::json::array()},
        {"asks", nlohmann::json::array()}
    });

    std::unordered_map<std::string, OrderBook> books{
        {"YES", yesBook},
        {"NO", noBook}
    };

    ArbitrageDetector detector;

    auto result = detector.checkBinaryArbitrage(market, books, 100);

    EXPECT_DOUBLE_EQ(result.quantity, 0);
    EXPECT_DOUBLE_EQ(result.grossProfit, 0);
}

TEST(ArbitrageDetectorTest, HandlesMultipleAskLevels) {
    Market market{"YES", "NO"};

    OrderBook yesBook;
    yesBook.applySnapshot({
        {"bids", nlohmann::json::array()},
        {"asks", {
            {{"price", "0.40"}, {"size", "50"}},
            {{"price", "0.45"}, {"size", "50"}}
        }}
    });

    OrderBook noBook;
    noBook.applySnapshot({
        {"bids", nlohmann::json::array()},
        {"asks", {
            {{"price", "0.40"}, {"size", "100"}}
        }}
    });

    std::unordered_map<std::string, OrderBook> books{
        {"YES", yesBook},
        {"NO", noBook}
    };

    ArbitrageDetector detector;

    auto result = detector.checkBinaryArbitrage(market, books, 100);

    EXPECT_DOUBLE_EQ(result.quantity, 100);
    EXPECT_DOUBLE_EQ(result.yesCost, 42.5);
    EXPECT_DOUBLE_EQ(result.noCost, 40);
    EXPECT_DOUBLE_EQ(result.totalCost, 82.5);
    EXPECT_DOUBLE_EQ(result.payout, 100);
    EXPECT_DOUBLE_EQ(result.grossProfit, 17.5);
}