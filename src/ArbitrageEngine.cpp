#include "ArbitrageEngine.hpp"

#include <iostream>

ArbitrageEngine::ArbitrageEngine(
    const std::vector<Market>& markets,
    double quantity
)
    : _markets(markets),
      _quantity(quantity)
{
    for (const Market& market : _markets) {
        _assetToMarket[market.yesAssetId] = market;
        _assetToMarket[market.noAssetId] = market;
    }
}

std::vector<ArbitrageOpportunity> ArbitrageEngine::processMessage(const nlohmann::json& json) {

    std::vector<ArbitrageOpportunity> opportunities;

    if (json.is_array()) {
        for (const auto& message : json) {
            std::vector<ArbitrageOpportunity> results = processMessage(message);

            opportunities.insert(opportunities.end(), results.begin(), results.end());
        }

        return opportunities;
    }

    if (!json.is_object()) {
        return {};
    }

    std::string eventType = json.value("event_type", "");

    if (eventType == "book") {
        std::string assetId =
            json["asset_id"].get<std::string>();

        _books[assetId].applySnapshot(json);

        auto it = _assetToMarket.find(assetId);

        if (it == _assetToMarket.end()) {
            return {};
        }

        const Market& market = it->second;

        ArbitrageOpportunity opportunity =
            _detector.checkBinaryArbitrage(market, _books, _quantity);

        if (opportunity.grossProfit > 0) {
            opportunities.push_back(opportunity);
        }

    } else if (eventType == "price_change") {
    std::unordered_set<std::string> affectedMarkets;

    for (const auto& change : json["price_changes"]) {
        std::string assetId =
            change["asset_id"].get<std::string>();

        _books[assetId].applyPriceChange(change);

        auto it = _assetToMarket.find(assetId);

        if (it == _assetToMarket.end()) {
            continue;
        }

        affectedMarkets.insert(it->second.yesAssetId);
    }

    for (const auto& marketId : affectedMarkets) {
        const Market& market = _assetToMarket.at(marketId);

        ArbitrageOpportunity opportunity =
            _detector.checkBinaryArbitrage(
                market,
                _books,
                _quantity
            );

        if (opportunity.grossProfit > 0) {
            opportunities.push_back(opportunity);
            }
        }
    }
    
    return opportunities;
}