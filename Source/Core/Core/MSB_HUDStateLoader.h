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

// Where a HUD file sits in its game's timeline. Two HUDs with the same gameId can be ordered
// by eventOrder (lower = earlier in the game).
struct HUDEventInfo
{
    std::string gameId;
    std::string eventNum; // as written in the HUD, e.g. "12a"
    uint32_t eventOrder = 0;
};

bool ReadHUDFile(const std::string& path, std::string& outJson);
bool GetHUDEventInfo(const std::string& json, HUDEventInfo& outInfo);

bool LoadStateFromHud(const std::string& path, MSBQuickMatchGameState& outState,
                      const std::string& p1Username, const std::string& p2Username);
bool LoadStateFromHudJson(const std::string& json_str, MSBQuickMatchGameState& outState,
                          const std::string& p1Username, const std::string& p2Username);
int allowLoadFromHUD(const std::string& path,
                     const std::string& p1Username, const std::string& p2Username,
                     bool isNetplay = true,
                     HUDValidationDetails* outDetails = nullptr);
int allowLoadFromHUDJson(const std::string& json_str,
                         const std::string& p1Username, const std::string& p2Username,
                         bool isNetplay = true,
                         HUDValidationDetails* outDetails = nullptr);
