#pragma once

#include <string>

#include <nlohmann/json.hpp>

// Thin HTTP layer: only speaks to the REST API and returns JSON.
// No business logic (filtering, market discovery) lives here.
class ClobRestClient {
public:
    explicit ClobRestClient(std::string host);

    // Performs a GET against the given path and returns the parsed JSON body.
    nlohmann::json getJson(const std::string& target) const;

    // Order book snapshot for a single token: /book?token_id=<assetId>
    nlohmann::json getOrderBook(const std::string& assetId) const;

private:
    std::string _host;
};
