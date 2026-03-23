#include "MSB_GenerateCustomMatchStateGeckoCode.h"
#include "MSB_CustomMatchStateDebugLoader.h"
#include <fstream>
#include <sstream>
#include <string>
#include "Common/Logging/Log.h"

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

        INFO_LOG_FMT(COMMON, "Loading key={}, value={}", key, value);

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

        else if (key.rfind("awayPosition", 0) == 0)
        {
            int order = std::stoi(key.substr(12));
            if (order >= 0 && order < 10)
                state.awayPositionByBattingOrder[order] = v;
        }

        else if (key.rfind("homePosition", 0) == 0)
        {
            int order = std::stoi(key.substr(12));
            if (order >= 0 && order < 10)
                state.awayPositionByBattingOrder[order] = v;
        }

        else if (key.rfind("runnerRosterID", 0) == 0)
        {
            int runner = std::stoi(key.substr(14));
            if (runner >= 0 && runner < 3)
                state.runnerRosterSpot[runner] = v;
        }

        else if (key.rfind("runnerCharacterID", 0) == 0)
        {
            int runner = std::stoi(key.substr(17));
            if (runner >= 0 && runner < 3)
                state.runnerCharacterID[runner] = v;
        }

        else if (key.rfind("p1Stamina", 0) == 0)
        {
            int pitcher = std::stoi(key.substr(9));
            if (pitcher >= 0 && pitcher < 9)
                state.pitcherStaminaP1[pitcher] = v;
        }

        else if (key.rfind("p2Stamina", 0) == 0)
        {
            int pitcher = std::stoi(key.substr(9));
            if (pitcher >= 0 && pitcher < 9)
                state.pitcherStaminaP2[pitcher] = v;
        }

        else
        {
            WARN_LOG_FMT(COMMON, "Failed to find key match. key={}, value={}", key, value);
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
// p1Character0=0
// p1Character1=1
// p1Character2=2
// p1Character3=3
// p1Character4=4
// p1Character5=5
// p1Character6=6
// p1Character7=7
// p1Character8=8
// p2Character0=10
// p2Character1=11
// p2Character2=12
// p2Character3=13
// p2Character4=14
// p2Character5=15
// p2Character6=16
// p2Character7=16
// p2Character8=17
// captainCharacterP1=3
// captainCharacterP2=2
// runnerRosterID0=0
// runnerCharacterID0=0
