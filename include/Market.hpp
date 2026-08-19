#pragma once

#include <string>

struct Market {
    std::string yesAssetId;
    std::string noAssetId;

    // Human-readable description and on-chain identifier. Populated by the
    // market loader so opportunities can be traced back to a real market.
    std::string question;
    std::string conditionId;

    // URL slugs: the canonical market page is
    // https://polymarket.com/event/<eventSlug>/<slug>.
    // Empty for hardcoded/fallback markets.
    std::string slug;
    std::string eventSlug;
};
