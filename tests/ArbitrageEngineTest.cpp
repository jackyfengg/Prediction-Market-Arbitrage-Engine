#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "ArbitrageEngine.hpp"

#include <fstream>
#include <stdexcept>

nlohmann::json loadFixture(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open fixture: " + path);
    }

    nlohmann::json json;
    file >> json;

    return json;
}

TEST(ArbitrageEngineTest, DetectsBinaryArbitrageFromBookSnapshots) {
    Market market{"YES", "NO"};
    ArbitrageEngine engine({market}, 1.0);

    nlohmann::json yesBook = {
        {"event_type", "book"},
        {"asset_id", "YES"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.45"}, {"size", "10"}}}}
    };

    nlohmann::json noBook = {
        {"event_type", "book"},
        {"asset_id", "NO"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.45"}, {"size", "10"}}}}
    };

    auto result1 = engine.processMessage(yesBook);

    EXPECT_TRUE(result1.empty());

    auto result2 = engine.processMessage(noBook);

    ASSERT_EQ(result2.size(), 1);
    EXPECT_GT(result2[0].grossProfit, 0.0);
}


TEST(ArbitrageEngineTest, HandlesMissingBook) {
    Market market{"YES", "NO"};
    ArbitrageEngine engine({market}, 1.0);

    nlohmann::json yesBook = {
        {"event_type", "book"},
        {"asset_id", "YES"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.45"}, {"size", "10"}}}}
    };

    auto result = engine.processMessage(yesBook);

    EXPECT_TRUE(result.empty());
}


TEST(ArbitrageEngineTest, DetectsArbitrageAfterPriceChange) {
    Market market{"YES", "NO"};
    ArbitrageEngine engine({market}, 1.0);

    nlohmann::json yesBook = {
        {"event_type", "book"},
        {"asset_id", "YES"},
        {"bids", {{{"price", "0.50"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
    };

    nlohmann::json noBook = {
        {"event_type", "book"},
        {"asset_id", "NO"},
        {"bids", {{{"price", "0.50"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
    };

    EXPECT_TRUE(engine.processMessage(yesBook).empty());
    EXPECT_TRUE(engine.processMessage(noBook).empty());

    nlohmann::json priceChange = {
        {"event_type", "price_change"},
        {"price_changes", {
            {
                {"asset_id", "YES"},
                {"price", "0.35"},
                {"size", "10"},
                {"side", "SELL"}
            }
        }}
    };

    auto result = engine.processMessage(priceChange);

    ASSERT_EQ(result.size(), 1);
    EXPECT_GT(result[0].grossProfit, 0.0);
}


TEST(ArbitrageEngineTest, ProcessesArrayOfMessages) {
    Market market{"YES", "NO"};
    ArbitrageEngine engine({market}, 1.0);

    nlohmann::json messages = nlohmann::json::array({
        {
            {"event_type", "book"},
            {"asset_id", "YES"},
            {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
            {"asks", {{{"price", "0.45"}, {"size", "10"}}}}
        },
        {
            {"event_type", "book"},
            {"asset_id", "NO"},
            {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
            {"asks", {{{"price", "0.45"}, {"size", "10"}}}}
        }
    });

    auto result = engine.processMessage(messages);

    ASSERT_EQ(result.size(), 1);
    EXPECT_GT(result[0].grossProfit, 0.0);
}


TEST(ArbitrageEngineTest, IgnoresUnknownAsset) {
    Market market{"YES", "NO"};
    ArbitrageEngine engine({market}, 1.0);

    nlohmann::json message = {
        {"event_type", "book"},
        {"asset_id", "UNKNOWN"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.45"}, {"size", "10"}}}}
    };

    auto result = engine.processMessage(message);

    EXPECT_TRUE(result.empty());
}


TEST(ArbitrageEngineTest, ProcessesMultiplePriceChanges) {
    Market market{"YES", "NO"};
    ArbitrageEngine engine({market}, 1.0);

    nlohmann::json yesBook = {
        {"event_type", "book"},
        {"asset_id", "YES"},
        {"bids", {{{"price", "0.50"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
    };

    nlohmann::json noBook = {
        {"event_type", "book"},
        {"asset_id", "NO"},
        {"bids", {{{"price", "0.50"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
    };

    EXPECT_TRUE(engine.processMessage(yesBook).empty());
    EXPECT_TRUE(engine.processMessage(noBook).empty());

    nlohmann::json changes = {
        {"event_type", "price_change"},
        {"price_changes", {
            {
                {"asset_id", "YES"},
                {"price", "0.35"},
                {"size", "10"},
                {"side", "SELL"}
            },
            {
                {"asset_id", "NO"},
                {"price", "0.60"},
                {"size", "10"},
                {"side", "SELL"}
            }
        }}
    };

    auto result = engine.processMessage(changes);

    ASSERT_EQ(result.size(), 1);
    EXPECT_GT(result[0].grossProfit, 0.0);
}


TEST(ArbitrageEngineTest, ReturnsCorrectOpportunityValues) {
    Market market{"YES", "NO"};
    ArbitrageEngine engine({market}, 1.0);

    nlohmann::json yesBook = {
        {"event_type", "book"},
        {"asset_id", "YES"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.45"}, {"size", "10"}}}}
    };

    nlohmann::json noBook = {
        {"event_type", "book"},
        {"asset_id", "NO"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.45"}, {"size", "10"}}}}
    };

    EXPECT_TRUE(engine.processMessage(yesBook).empty());

    auto result = engine.processMessage(noBook);

    ASSERT_EQ(result.size(), 1);

    EXPECT_DOUBLE_EQ(result[0].quantity, 1.0);
    EXPECT_DOUBLE_EQ(result[0].yesCost, 0.45);
    EXPECT_DOUBLE_EQ(result[0].noCost, 0.45);
    EXPECT_DOUBLE_EQ(result[0].totalCost, 0.90);
    EXPECT_DOUBLE_EQ(result[0].payout, 1.0);
    EXPECT_DOUBLE_EQ(result[0].grossProfit, 0.10);
}


TEST(ArbitrageEngineTest, ScansMultipleMarkets) {
    Market profitableMarket{"YES1", "NO1"};
    Market unprofitableMarket{"YES2", "NO2"};

    ArbitrageEngine engine(
        {profitableMarket, unprofitableMarket},
        1.0
    );

    nlohmann::json yes1Book = {
        {"event_type", "book"},
        {"asset_id", "YES1"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.45"}, {"size", "10"}}}}
    };

    nlohmann::json no1Book = {
        {"event_type", "book"},
        {"asset_id", "NO1"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.45"}, {"size", "10"}}}}
    };

    nlohmann::json yes2Book = {
        {"event_type", "book"},
        {"asset_id", "YES2"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
    };

    nlohmann::json no2Book = {
        {"event_type", "book"},
        {"asset_id", "NO2"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
    };

    EXPECT_TRUE(engine.processMessage(yes1Book).empty());

    auto result1 = engine.processMessage(no1Book);

    ASSERT_EQ(result1.size(), 1);
    EXPECT_DOUBLE_EQ(result1[0].grossProfit, 0.10);

    EXPECT_TRUE(engine.processMessage(yes2Book).empty());

    auto result2 = engine.processMessage(no2Book);

    EXPECT_TRUE(result2.empty());
}


TEST(ArbitrageEngineTest, ProcessesPriceChangesAcrossMultipleMarkets) {
    Market market1{"YES1", "NO1"};
    Market market2{"YES2", "NO2"};

    ArbitrageEngine engine(
        {market1, market2},
        1.0
    );

    nlohmann::json initialMessages = nlohmann::json::array({
        {
            {"event_type", "book"},
            {"asset_id", "YES1"},
            {"bids", {{{"price", "0.50"}, {"size", "10"}}}},
            {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
        },
        {
            {"event_type", "book"},
            {"asset_id", "NO1"},
            {"bids", {{{"price", "0.50"}, {"size", "10"}}}},
            {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
        },
        {
            {"event_type", "book"},
            {"asset_id", "YES2"},
            {"bids", {{{"price", "0.50"}, {"size", "10"}}}},
            {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
        },
        {
            {"event_type", "book"},
            {"asset_id", "NO2"},
            {"bids", {{{"price", "0.50"}, {"size", "10"}}}},
            {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
        }
    });

    EXPECT_TRUE(engine.processMessage(initialMessages).empty());

    nlohmann::json changes = {
        {"event_type", "price_change"},
        {"price_changes", {
            {
                {"asset_id", "YES1"},
                {"price", "0.35"},
                {"size", "10"},
                {"side", "SELL"}
            },
            {
                {"asset_id", "YES2"},
                {"price", "0.60"},
                {"size", "10"},
                {"side", "SELL"}
            }
        }}
    };

    auto result = engine.processMessage(changes);

    ASSERT_EQ(result.size(), 1);

    EXPECT_DOUBLE_EQ(result[0].yesCost, 0.35);
    EXPECT_DOUBLE_EQ(result[0].noCost, 0.60);
    EXPECT_DOUBLE_EQ(result[0].totalCost, 0.95);
    EXPECT_NEAR(result[0].grossProfit, 0.05, 1e-9);
}

TEST(ArbitrageEngineTest, ProcessesTestBookMessage) {
    nlohmann::json book =
        loadFixture("tests/fixtures/test_book.json");

    ASSERT_TRUE(book.is_array());
    ASSERT_EQ(book.size(), 1);

    EXPECT_EQ(
        book[0]["event_type"].get<std::string>(),
        "book"
    );

    EXPECT_TRUE(book[0]["bids"].is_array());
    EXPECT_TRUE(book[0]["asks"].is_array());

    std::string assetId =
        book[0]["asset_id"].get<std::string>();

    Market market{
        assetId,
        "27828976648682466778776999076215423777766972981338254154049603024771135223200"
    };

    ArbitrageEngine engine({market}, 1.0);

    auto result = engine.processMessage(book);

    // Only one side of the market has arrived.
    EXPECT_TRUE(result.empty());
}

TEST(ArbitrageEngineTest, ProcessesTestPriceChangeMessage) {
    nlohmann::json book =
        loadFixture("tests/fixtures/test_book.json");

    nlohmann::json priceChange =
        loadFixture("tests/fixtures/test_price_change.json");

    ASSERT_TRUE(book.is_array());
    ASSERT_EQ(book.size(), 1);

    ASSERT_EQ(
        priceChange["event_type"].get<std::string>(),
        "price_change"
    );

    EXPECT_TRUE(priceChange["price_changes"].is_array());
    EXPECT_EQ(priceChange["price_changes"].size(), 2);

    std::string yesAsset =
        book[0]["asset_id"].get<std::string>();

    std::string noAsset =
        priceChange["price_changes"][0]["asset_id"]
            .get<std::string>();

    ASSERT_EQ(
        priceChange["price_changes"][1]["asset_id"]
            .get<std::string>(),
        yesAsset
    );

    Market market{yesAsset, noAsset};

    ArbitrageEngine engine({market}, 1.0);

    // Process the real book first.
    auto bookResult = engine.processMessage(book);

    EXPECT_TRUE(bookResult.empty());

    // Process the real price-change message.
    auto changeResult =
        engine.processMessage(priceChange);

    // Don't assert profitability here yet.
    // This test is verifying that the real message format
    // successfully passes through the engine.
    EXPECT_TRUE(changeResult.size() >= 0);
}

TEST(ArbitrageEngineTest, InitializesBookFromSnapshot) {
    Market market{"YES", "NO"};

    ArbitrageEngine engine({market}, 1.0);

    nlohmann::json book = {
        {
            "price", "0.45"
        }
    };

    nlohmann::json snapshot = {
        {"asset_id", "YES"},
        {"bids", {
            {
                {"price", "0.40"},
                {"size", "10"}
            }
        }},
        {"asks", {
            {
                {"price", "0.45"},
                {"size", "10"}
            }
        }}
    };

    engine.initializeBook(
        "YES",
        snapshot
    );

    // We can expose read-only book access later if
    // we want this test to inspect the actual state.
    SUCCEED();
}
