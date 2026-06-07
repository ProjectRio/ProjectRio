#pragma once

#include "Core/MSB_GenerateQuickMatchSetupGeckoCode.h"
#include <string>

struct HUDValidationDetails
{
    std::string hudAwayPlayer;
    std::string hudHomePlayer;
    int hudTagSetId = -1;
    std::string activeTagSetName;
};

bool LoadStateFromHud(const std::string& path, MSBQuickMatchGameState& outState,
                      const std::string& p1Username, const std::string& p2Username);
int allowLoadFromHUD(const std::string& path,
                     const std::string& p1Username, const std::string& p2Username,
                     bool isNetplay = true,
                     HUDValidationDetails* outDetails = nullptr);