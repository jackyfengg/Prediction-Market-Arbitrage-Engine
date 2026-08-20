#include "ArbitrageEngine.hpp"

namespace {

// Converts a simulated execution into a full opportunity object.
ArbitrageOpportunity toOpportunity(const Market& market, const ExecutionResult& exec) {
    ArbitrageOpportunity opp;
    opp.market = market;

    opp.quantity = exec.executedQuantity;
    opp.requestedQuantity = exec.requestedQuantity;
    opp.yesCost = exec.yesCost;
    opp.noCost = exec.noCost;
    opp.totalCost = exec.capitalRequired;
    opp.yesFee = exec.yesFee;
    opp.noFee = exec.noFee;
    opp.totalFees = exec.fees;
    opp.slippage = exec.slippage;
    opp.payout = exec.payout;
    opp.grossProfit = exec.grossProfit;
    opp.netProfit = exec.netProfit;
    opp.returnOnCapital = exec.returnOnCapital;

    return opp;
}

} // namespace

ArbitrageEngine::ArbitrageEngine(const std::vector<Market>& markets, double quantity)
    : _markets(markets),
      _feeModel(feeRate),
      _detector(_feeModel),
      _simulator(_feeModel),
      _quantity(quantity)
{
    for (const Market& market : _markets) {
        _assetToMarket[market.yesAssetId] = market;
        _assetToMarket[market.noAssetId] = market;
    }
}

std::vector<std::string> ArbitrageEngine::assetIds() const {
    std::vector<std::string> ids;

    for (const Market& market : _markets) {
        ids.push_back(market.yesAssetId);
        ids.push_back(market.noAssetId);
    }

    return ids;
}

void ArbitrageEngine::initializeBook(const std::string& assetId, const nlohmann::json& book) {
    _books[assetId].applySnapshot(book);
}

void ArbitrageEngine::clearBooks() {
    _books.clear();
}

double ArbitrageEngine::combinedBestAsk(const Market& market) const {
    auto yesIt = _books.find(market.yesAssetId);
    auto noIt = _books.find(market.noAssetId);

    if (yesIt == _books.end() || noIt == _books.end()) {
        return 0.0;
    }

    double yesAsk = yesIt->second.getBestAsk();
    double noAsk = noIt->second.getBestAsk();

    if (yesAsk <= 0.0 || noAsk <= 0.0) {
        return 0.0;
    }

    return yesAsk + noAsk;
}

std::vector<ArbitrageOpportunity> ArbitrageEngine::processMessage(const nlohmann::json& json) {
    std::vector<ArbitrageOpportunity> opportunities;

    if (json.is_array()) {
        for (const auto& message : json) {
            std::vector<ArbitrageOpportunity> results = processMessage(message);

            opportunities.insert(
                opportunities.end(),
                results.begin(),
                results.end()
            );
        }

        return _ranker.rank(std::move(opportunities));
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
            _detector.checkBinaryArbitrage(
                market,
                _books,
                _quantity
            );

        if (opportunity.grossProfit > 0) {
            opportunities.push_back(opportunity);
        }

    } else if (eventType == "price_change") {
        std::unordered_set<std::string> affectedMarkets;

        for (const auto& change : json["price_changes"]) {
            std::string assetId =
                change["asset_id"].get<std::string>();

            auto bookIt = _books.find(assetId);

            if (bookIt == _books.end()) {
                continue;
            }

            bookIt->second.applyPriceChange(change);

            auto it = _assetToMarket.find(assetId);

            if (it == _assetToMarket.end()) {
                continue;
            }

            affectedMarkets.insert(
                it->second.yesAssetId
            );
        }

        for (const auto& marketId : affectedMarkets) {
            const Market& market = _assetToMarket.at(marketId);

            ArbitrageOpportunity opportunity = _detector.checkBinaryArbitrage(market, _books, _quantity);

            if (opportunity.grossProfit > 0) {
                opportunities.push_back(opportunity);
            }
        }
    }

    return _ranker.rank(std::move(opportunities));
}

ArbitrageOpportunity ArbitrageEngine::evaluateMarket(const Market& market) {
    ArbitrageOpportunity candidate = _detector.checkBinaryArbitrage(market, _books, _quantity);

    if (candidate.quantity <= 0) {
        return {};
    }

    ExecutionResult exec = _simulator.simulate(market, _books, candidate.requestedQuantity);

    if (exec.netProfit <= 0) {
        return {};
    }

    return toOpportunity(market, exec);
}