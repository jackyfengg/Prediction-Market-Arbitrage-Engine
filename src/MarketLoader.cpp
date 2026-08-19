#include "MarketLoader.hpp"

#include <string>

#include "ClobRestClient.hpp"

namespace {

bool isTrue(const nlohmann::json& m, const std::string& key) {
    if (m.contains(key) && !m[key].is_null()) {
        return m.value(key, false);
    }

    return true;
}

double asDouble(const nlohmann::json& m, const std::string& key) {
    if (!m.contains(key) || m[key].is_null()) {
        return 0.0;
    }

    if (m[key].is_number()) {
        return m[key].get<double>();
    }

    if (m[key].is_string()) {
        return std::stod(m[key].get<std::string>());
    }

    return 0.0;
}

// clobTokenIds is an array in some responses and a JSON-encoded string
// (e.g. "[\"id1\", \"id2\"]") in others.
nlohmann::json tokenIdsOf(const nlohmann::json& m) {
    if (!m.contains("clobTokenIds")) {
        return nullptr;
    }

    const auto& value = m["clobTokenIds"];

    if (value.is_array()) {
        return value;
    }

    if (value.is_string()) {
        try {
            return nlohmann::json::parse(value.get<std::string>());
        }
        catch (const nlohmann::json::exception&) {
            return nullptr;
        }
    }

    return nullptr;
}

// Returns a Market if the object looks like a usable binary market,
// or an invalid market (empty asset IDs) otherwise.
Market marketFromMarketObject(const nlohmann::json& m) {
    Market market;

    if (!m.is_object()) {
        return market;
    }

    // Skip markets that are no longer tradeable.
    if (m.value("closed", false)) {
        return market;
    }

    if (!m.value("active", true)) {
        return market;
    }

    // Gamma API uses camelCase; some responses use snake_case.
    if (!isTrue(m, "acceptingOrders") || !isTrue(m, "accepting_orders")) {
        return market;
    }

    // Skip zero-liquidity markets when liquidity is reported.
    if (m.contains("liquidity") && asDouble(m, "liquidity") <= 0.0) {
        return market;
    }

    // clobTokenIds holds the YES/NO asset ids for a binary market.
    nlohmann::json tokenIds = tokenIdsOf(m);

    if (!tokenIds.is_array() || tokenIds.size() < 2) {
        return market;
    }

    std::string yesId = tokenIds[0].get<std::string>();
    std::string noId = tokenIds[1].get<std::string>();

    if (yesId.empty() || noId.empty()) {
        return market;
    }

    market.yesAssetId = yesId;
    market.noAssetId = noId;

    return market;
}

void collectMarkets(const nlohmann::json& json, std::vector<Market>& out) {
    if (json.is_object()) {
        // Some responses wrap the list under an "events" or "markets" key.
        if (json.contains("markets") && json["markets"].is_array()) {
            collectMarkets(json["markets"], out);
        } else if (json.contains("events") && json["events"].is_array()) {
            collectMarkets(json["events"], out);
        }
        return;
    }

    if (!json.is_array()) {
        return;
    }

    for (const auto& element : json) {
        if (element.is_object() && element.contains("markets") &&
            element["markets"].is_array()) {
            // Event object: dive into its nested markets.
            collectMarkets(element["markets"], out);
        } else {
            Market market = marketFromMarketObject(element);

            if (!market.yesAssetId.empty() && !market.noAssetId.empty()) {
                out.push_back(market);
            }
        }
    }
}

} // namespace

std::vector<Market> parseMarkets(const nlohmann::json& json) {
    std::vector<Market> markets;

    collectMarkets(json, markets);

    return markets;
}

MarketLoader::MarketLoader(ClobRestClient& client)
    : _client(client)
{
}

std::vector<Market> MarketLoader::loadMarkets(std::size_t limit) const {
    std::string target =
        "/markets?closed=false&active=true&limit=" +
        std::to_string(limit) +
        "&order=volume24hr&ascending=false";

    nlohmann::json response = _client.getJson(target);

    return parseMarkets(response);
}
