#pragma once

#include <string>
#include <nlohmann/json.hpp>

class ClobRestClient {
public:
    explicit ClobRestClient(std::string host);

    nlohmann::json getOrderBook(const std::string& assetId) const;

private:
    std::string _host;
};