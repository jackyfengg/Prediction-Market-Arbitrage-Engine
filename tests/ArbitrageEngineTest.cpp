#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "ArbitrageEngine.hpp"

TEST(ArbitrageEngineTest, DetectsBinaryArbitrageFromBookSnapshots) {
    Market market;
    market.yesAssetId = "YES";
    market.noAssetId = "NO";

    std::vector<Market> markets = { market };

    ArbitrageEngine engine(markets, 1.0);

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
    EXPECT_GT(result2[0].grossProfit, 0);
}

TEST(ArbitrageEngineTest, DetectsArbitrageAfterPriceChange) {
    Market market{"YES", "NO"};
    ArbitrageEngine engine({market}, 1.0);

    nlohmann::json yesBook = {
        {"event_type", "book"},
        {"asset_id", "YES"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
        {"asks", {{{"price", "0.60"}, {"size", "10"}}}}
    };

    nlohmann::json noBook = {
        {"event_type", "book"},
        {"asset_id", "NO"},
        {"bids", {{{"price", "0.40"}, {"size", "10"}}}},
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