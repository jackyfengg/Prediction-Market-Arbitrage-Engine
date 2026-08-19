#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ArbitrageDetector.hpp"
#include "ExecutionSimulator.hpp"
#include "FeeModel.hpp"
#include "Market.hpp"
#include "OpportunityRanker.hpp"
#include "OrderBook.hpp"

// Coordinates books, markets, incoming events, detection, execution
// simulation, and ranking. Kept lean: it does not own fee math, ranking
// logic, or networking.
class ArbitrageEngine {
public:
    ArbitrageEngine(
        const std::vector<Market>& markets,
        double quantity,
        double feeRate = 0.0
    );

    void initializeBook(const std::string& assetId, const nlohmann::json& book);

    std::vector<ArbitrageOpportunity> processMessage(const nlohmann::json& json);

    const std::vector<Market>& markets() const { return _markets; }

    // All subscribed asset ids (YES + NO across every market).
    std::vector<std::string> assetIds() const;

    // Runs detector -> execution simulator for one market. Returns an empty
    // opportunity when there is nothing worth executing.
    ArbitrageOpportunity evaluateMarket(const Market& market);

private:
    std::vector<Market> _markets;
    std::unordered_map<std::string, OrderBook> _books;
    std::unordered_map<std::string, Market> _assetToMarket;

    FeeModel _feeModel;
    ArbitrageDetector _detector;
    ExecutionSimulator _simulator;
    OpportunityRanker _ranker;

    double _quantity;
};
