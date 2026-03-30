#pragma once

#include "Core/MSB_GenerateQuickMatchSetupGeckoCode.h"
#include <string>

bool LoadStateFromHud(const std::string& path, MSBQuickMatchGameState& outState);
int allowLoadFromHUD(const std::string& path);