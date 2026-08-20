#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "ArbitrageDetector.hpp"
#include "ExecutionSimulator.hpp"
#include "FeeModel.hpp"
#include "Market.hpp"
#include "OpportunityRanker.hpp"
#include "OrderBook.hpp"

class ArbitrageEngine{
public:
    ArbitrageEngine(const std::vector<Market>& markets, double quantity, double feeRate = 0.0);

    void initializeBook(const std::string& assetId, const nlohmann::json& book);

    void clearBooks();

    std::vector<ArbitrageOpportunity> processMessage(const nlohmann::json& json);

    double combinedBestAsk(const Market& market) const;

    const std::vector<Market>& markets() const {
        return _markets;
    }

    std::vector<std::string> assetIds() const;

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

