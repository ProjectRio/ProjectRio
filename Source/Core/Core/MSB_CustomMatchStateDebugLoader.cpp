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

        if (key == "captainCharacterP1") state.captainCharacterP1 = v;
        else if (key == "captainCharacterP2") state.captainCharacterP2 = v;

        else if (key.rfind("p1Character", 0) == 0)
        {
            int characterNumber = std::stoi(key.substr(11));
            if (characterNumber >= 0 && characterNumber < 10)
                state.charactersP1ByPosition[characterNumber] = v;
        }

        else if (key.rfind("p2Character", 0) == 0)
        {
            int characterNumber = std::stoi(key.substr(11));
            if (characterNumber >= 0 && characterNumber < 10)
                state.charactersP2ByPosition[characterNumber] = v;
        }
        
        else if (key == "logoP1") state.logoP1 = v;
        else if (key == "logoP2") state.logoP2 = v;

        else if (key == "stadium") state.stadium = v;

        else if (key == "firstBatter") state.firstBatter = v;
        else if (key == "starSkills") state.starSkills = v;
        else if (key == "inningsSelected") state.inningsSelected = v;
        else if (key == "mercy") state.mercy = v;

        else if (key == "inning") state.inning = v;
        else if (key == "halfInning") state.halfInning = v;

        else if (key == "awayScore") state.awayScore = v;
        else if (key == "homeScore") state.homeScore = v;

        else if (key == "strikes") state.strikes = v;
        else if (key == "balls") state.balls = v;
        else if (key == "outs") state.outs = v;

        else if (key == "awayTeamStars") state.awayTeamStars = v;
        else if (key == "homeTeamStars") state.homeTeamStars = v;

        else if (key == "isStarChance") state.isStarChance = v;

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

// User/Debug/msb_state.txt
// stadium=1
// starSkills=1
// inningsSelected=9
// mercy=1
// logoP1=5
// logoP2=30
// inning=4
// halfInning=1
// awayScore=4
// homeScore=6
// homeInning0=1
// homeInning1=3
// homeInning2=2
// strikes=1
// balls=2
// outs=1
// awayTeamStars=4
// homeTeamStars=2
