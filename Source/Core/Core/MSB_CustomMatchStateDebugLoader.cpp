#include "MSB_GenerateCustomMatchStateGeckoCode.h"
#include "MSB_CustomMatchStateDebugLoader.h"
#include <fstream>
#include <sstream>
#include <string>

// If using Dolphin logging
// #include "Common/Logging/Log.h"

MSBGameState LoadDebugState(const std::string& path)
{
    MSBGameState state;

    std::ifstream file(path);
    std::string line;

    while (std::getline(file, line))
    {
        // Skip empty lines or comments
        if (line.empty() || line[0] == '#')
            continue;

        // Skip malformed lines without '='
        if (line.find('=') == std::string::npos)
            continue;

        std::string key;
        std::string value;

        std::stringstream ss(line);
        std::getline(ss, key, '=');
        std::getline(ss, value);

        int v = std::stoi(value);

        if (key == "homeScore") state.homeScore = v;
        else if (key == "awayScore") state.awayScore = v;

        else if (key == "balls") state.balls = v;
        else if (key == "strikes") state.strikes = v;
        else if (key == "outs") state.outs = v;

        else if (key.rfind("homeInning", 0) == 0)
        {
            int inning = std::stoi(key.substr(10));
            if (inning >= 0 && inning < 18)
                state.homeInningScores[inning] = v;
        }

        else if (key.rfind("awayInning", 0) == 0)
        {
            int inning = std::stoi(key.substr(10));
            if (inning >= 0 && inning < 18)
                state.awayInningScores[inning] = v;
        }

        else
        {
            // Optional logging for unknown keys
            // WARN_LOG_FMT(CORE, "Unknown debug key: {}", key);
        }
    }

    return state;
}