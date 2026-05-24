#pragma once

#include "Core/MSB_GenerateQuickMatchSetupGeckoCode.h"
#include <string>

bool LoadStateFromHud(const std::string& path, MSBQuickMatchGameState& outState,
                      const std::string& p1Username, const std::string& p2Username);
int allowLoadFromHUD(const std::string& path,
                     const std::string& p1Username, const std::string& p2Username);