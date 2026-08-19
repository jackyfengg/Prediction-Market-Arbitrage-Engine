#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "MarketLoader.hpp"

namespace {

nlohmann::json marketObject(
    const std::string& yesId,
    const std::string& noId,
    bool closed = false,
    bool active = true,
    bool acceptingOrders = true,
    double liquidity = 1000.0,
    const std::string& question = "",
    const std::string& conditionId = "",
    const std::string& slug = "",
    const std::string& eventSlug = ""
) {
    nlohmann::json m;
    m["clobTokenIds"] = {yesId, noId};
    m["closed"] = closed;
    m["active"] = active;
    m["accepting_orders"] = acceptingOrders;
    m["liquidity"] = liquidity;
    m["question"] = question;
    m["conditionId"] = conditionId;
    m["slug"] = slug;

    if (!eventSlug.empty()) {
        m["events"] = nlohmann::json::array({{{"slug", eventSlug}}});
    }

    return m;
}

} // namespace

TEST(MarketLoaderTest, ParsesActiveMarketsAndFiltersUnusableOnes) {
    nlohmann::json response = nlohmann::json::array({
        marketObject("YES1", "NO1", false, true, true, 1000.0,
                     "Will it rain?", "0xabc123",
                     "will-it-rain", "weather-event"),                 // usable
        marketObject("YES2", "NO2", /*closed=*/true),                    // closed
        marketObject("YES3", "NO3", /*closed=*/false, /*active=*/false), // inactive
        marketObject("YES4", "NO4", /*closed=*/false, /*active=*/true,
                     /*acceptingOrders=*/false),                         // not accepting
        marketObject("YES5", "NO5", /*closed=*/false, /*active=*/true,
                     /*acceptingOrders=*/true, /*liquidity=*/0.0),       // no liquidity
        marketObject("", "NO6")                                          // missing YES token
    });

    auto markets = parseMarkets(response);

    ASSERT_EQ(markets.size(), 1);
    EXPECT_EQ(markets[0].yesAssetId, "YES1");
    EXPECT_EQ(markets[0].noAssetId, "NO1");
    EXPECT_EQ(markets[0].question, "Will it rain?");
    EXPECT_EQ(markets[0].conditionId, "0xabc123");
    EXPECT_EQ(markets[0].slug, "will-it-rain");
    EXPECT_EQ(markets[0].eventSlug, "weather-event");
}

TEST(MarketLoaderTest, ParsesMarketsNestedInEvents) {
    nlohmann::json response = nlohmann::json::array({
        {
            {"id", 1},
            {"slug", "event-one"},
            {"markets", nlohmann::json::array({
                marketObject("YES1", "NO1"),
                marketObject("YES2", "NO2")
            })}
        }
    });

    auto markets = parseMarkets(response);

    ASSERT_EQ(markets.size(), 2);
    EXPECT_EQ(markets[0].yesAssetId, "YES1");
    EXPECT_EQ(markets[1].noAssetId, "NO2");
}

TEST(MarketLoaderTest, SkipsObjectsWithoutTokenIds) {
    nlohmann::json response = nlohmann::json::array({
        {
            {"id", 1},
            {"question", "No clob tokens here"}
        }
    });

    auto markets = parseMarkets(response);

    EXPECT_TRUE(markets.empty());
}

TEST(MarketLoaderTest, ParsesRealGammaApiShapes) {
    // The Gamma API reports liquidity as a string, clobTokenIds as a
    // JSON-encoded string, and uses camelCase for acceptingOrders.
    nlohmann::json usable;
    usable["active"] = true;
    usable["closed"] = false;
    usable["acceptingOrders"] = true;
    usable["liquidity"] = "15411.1679";
    usable["clobTokenIds"] =
        "[\"85367286745806857961178482075931972831841231758328346969840810630055458089640\", "
        "\"40069008842150598748086988698459627032664680273804858199848489101623205239948\"]";

    nlohmann::json closed;
    closed["active"] = true;
    closed["closed"] = true;
    closed["acceptingOrders"] = true;
    closed["liquidity"] = "1000.0";
    closed["clobTokenIds"] =
        "[\"id3\", \"id4\"]";

    auto markets = parseMarkets(nlohmann::json::array({usable, closed}));

    ASSERT_EQ(markets.size(), 1);
    EXPECT_EQ(
        markets[0].yesAssetId,
        "85367286745806857961178482075931972831841231758328346969840810630055458089640");
    EXPECT_EQ(
        markets[0].noAssetId,
        "40069008842150598748086988698459627032664680273804858199848489101623205239948");
}

TEST(MarketLoaderTest, EmptyResponse) {
    EXPECT_TRUE(parseMarkets(nlohmann::json::array()).empty());
    EXPECT_TRUE(parseMarkets(nlohmann::json::object()).empty());
}
