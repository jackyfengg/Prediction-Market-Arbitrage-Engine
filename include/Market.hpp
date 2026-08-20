#pragma once

#include <string>

struct Market {
    std::string yesAssetId;
    std::string noAssetId;

    // Description
    std::string question;
    std::string conditionId;

    std::string slug;
    std::string eventSlug;

};