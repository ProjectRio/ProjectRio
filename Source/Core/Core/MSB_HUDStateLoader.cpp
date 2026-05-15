#include "Core/MSB_HUDStateLoader.h"
#include "Core/MSB_GenerateQuickMatchSetupGeckoCode.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/TagSet.h"
#include "Core/Core.h"
#include <fstream>
#include <string>
#include <picojson.h>


bool LoadStateFromHud(const std::string& path, MSBQuickMatchGameState& outState,
                      const std::string& p1Username, const std::string& p2Username)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        ERROR_LOG_FMT(COMMON, "Failed to open HUD file: {}", path);
        return false;
    }

    INFO_LOG_FMT(COMMON, "Found HUD file: {}", path);

    std::string json_str((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());

    picojson::value v;
    std::string err = picojson::parse(v, json_str);

    if (!err.empty())
    {
        ERROR_LOG_FMT(COMMON, "Failed to parse HUD JSON from file: {} ({})", path, err);
        return false;
    }

    INFO_LOG_FMT(COMMON, "Successfully parsed HUD JSON from file: {}", path);

    if (!v.is<picojson::object>())
    {
        ERROR_LOG_FMT(COMMON, "HUD JSON is not an object: {}", path);
        return false;
    }

    INFO_LOG_FMT(COMMON, "HUD is a JSON object.: {}", path);

    const picojson::object& j = v.get<picojson::object>();

    // === P1/P2 to HOME/AWAY MAPPING and VERIFICATION ===
    if (p1Username.empty())
    {
        ERROR_LOG_FMT(COMMON, "Could not find player 1 in netplay data.");
        return false;
    }
    INFO_LOG_FMT(COMMON, "P1 player: {}", p1Username);
    INFO_LOG_FMT(COMMON, "P2 player: {}", p2Username.empty() ? "none (solo)" : p2Username);

    std::string awayPlayer = j.count("Away Player") ?
        std::string(StripWhitespace(j.at("Away Player").get<std::string>())) : "";
    std::string homePlayer = j.count("Home Player") ?
        std::string(StripWhitespace(j.at("Home Player").get<std::string>())) : "";
    INFO_LOG_FMT(COMMON, "Home and away players set: Home {}, Away {}.", homePlayer, awayPlayer);

    // Validate players match the HUD file
    bool p1IsAway = (p1Username == awayPlayer);
    bool p1IsHome = (p1Username == homePlayer);

    if (!p1IsAway && !p1IsHome)
    {
        ERROR_LOG_FMT(COMMON, "P1 player '{}' not found in HUD file. Away='{}', Home='{}'",
                    p1Username, awayPlayer, homePlayer);
        return false;
    }

    // If opponent is known, validate they match the other slot
    if (!p2Username.empty())
    {
        bool opponentIsAway = (p2Username == awayPlayer);
        bool opponentIsHome = (p2Username == homePlayer);

        if (!((p1IsAway && opponentIsHome) || (p1IsHome && opponentIsAway)))
        {
            ERROR_LOG_FMT(COMMON, "Player mismatch. P1='{}', Opponent='{}', HUD Away='{}', HUD Home='{}'",
                        p1Username, p2Username, awayPlayer, homePlayer);
            return false;
        }

        menuInputRestrictionEnabled = true; // if both players present, restrict menu inputs to prevent accidental desync. 
    }
    // Solo game - just verify P1 player is in the file, P2 slot can be "No Player Selected"
    else
    {
        menuInputRestrictionEnabled = false; // if solo, don't restrict menu inputs since P1 needs some control on the captain screen. Used for debugging.
        INFO_LOG_FMT(COMMON, "Solo game detected, skipping opponent validation.");
    }

    INFO_LOG_FMT(COMMON, "Starting parsing HUD to fill out state");


    MSBQuickMatchGameState state;

    // === PRE-GAME SETTINGS ===
    // === ROSTERS ===
    // Characters are stored in roster order in the HUD file, but your state
    // needs them in position order (P, C, 1B, 2B, 3B, SS, LF, CF, RF).
    // The "Fielding Position" field tells us what position each roster slot plays.
    // Position mapping: 0=P, 1=C, 2=1B, 3=2B, 4=3B, 5=SS, 6=LF, 7=CF, 8=RF

    // If host player is away, away=P1 and home=P2. Otherwise invert.

    for (int i = 0; i < 9; i++)
    {
        std::string p1Key = p1IsAway ? "Away Roster " + std::to_string(i)
                                            : "Home Roster " + std::to_string(i);
        std::string p2Key = p1IsAway ? "Home Roster " + std::to_string(i)
                                            : "Away Roster " + std::to_string(i);

        if (j.count(p1Key))
        {
            const picojson::object& roster = j.at(p1Key).get<picojson::object>();
            uint8_t charID = static_cast<uint8_t>(roster.at("CharID").get<double>());
            uint8_t position = static_cast<uint8_t>(roster.at("Fielding Position").get<double>());
            if (position < 9)
            {
                state.charactersP1ByPosition[position] = charID;
                state.battingHandP1ByPosition[position] = static_cast<uint8_t>(roster.at("Batting Hand").get<double>());
                state.fieldingHandP1ByPosition[position] = static_cast<uint8_t>(roster.at("Fielding Hand").get<double>());
                state.superstarP1ByPosition[position] = static_cast<uint8_t>(roster.at("Superstar").get<double>());
            }

            if (roster.count("Captain") && roster.at("Captain").get<double>() == 1)
            {
                state.captainCharacterP1 = charID;
                state.captainPositionP1 = position;
            }
        }

        if (j.count(p2Key))
        {
            const picojson::object& roster = j.at(p2Key).get<picojson::object>();
            uint8_t charID = static_cast<uint8_t>(roster.at("CharID").get<double>());
            uint8_t position = static_cast<uint8_t>(roster.at("Fielding Position").get<double>());
            if (position < 9)
            {
                state.charactersP2ByPosition[position] = charID;
                state.battingHandP2ByPosition[position] = static_cast<uint8_t>(roster.at("Batting Hand").get<double>());
                state.fieldingHandP2ByPosition[position] = static_cast<uint8_t>(roster.at("Fielding Hand").get<double>());
                state.superstarP2ByPosition[position] = static_cast<uint8_t>(roster.at("Superstar").get<double>());
            }

            if (roster.count("Captain") && roster.at("Captain").get<double>() == 1)
            {
                state.captainCharacterP2 = charID;
                state.captainPositionP2 = position;
            }
        }
    }
    for (int i = 0; i < 9; i++)
    {
        INFO_LOG_FMT(
            COMMON,
            "P1 Position {}: CharID {}, Batting Hand {}, Fielding Hand {}, Superstar {}",
            i,
            state.charactersP1ByPosition[i].has_value() ? std::to_string(state.charactersP1ByPosition[i].value()) : "not set",
            state.battingHandP1ByPosition[i].has_value() ? std::to_string(state.battingHandP1ByPosition[i].value()) : "not set",
            state.fieldingHandP1ByPosition[i].has_value() ? std::to_string(state.fieldingHandP1ByPosition[i].value()) : "not set",
            state.superstarP1ByPosition[i].has_value() ? std::to_string(state.superstarP1ByPosition[i].value()) : "not set");
    }
    for (int i = 0; i < 9; i++)
    {
        INFO_LOG_FMT(
            COMMON,
            "P2 Position {}: CharID {}, Batting Hand {}, Fielding Hand {}, Superstar {}",
            i,
            state.charactersP2ByPosition[i].has_value() ? std::to_string(state.charactersP2ByPosition[i].value()) : "not set",
            state.battingHandP2ByPosition[i].has_value() ? std::to_string(state.battingHandP2ByPosition[i].value()) : "not set",
            state.fieldingHandP2ByPosition[i].has_value() ? std::to_string(state.fieldingHandP2ByPosition[i].value()) : "not set",
            state.superstarP2ByPosition[i].has_value() ? std::to_string(state.superstarP2ByPosition[i].value()) : "not set");
    }
    INFO_LOG_FMT(COMMON, "Captain P1: {}, Position: {}", 
        state.captainCharacterP1.has_value() ? std::to_string(state.captainCharacterP1.value()) : "not set", 
        state.captainPositionP1.has_value() ? std::to_string(state.captainPositionP1.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Captain P2: {}, Position: {}", 
        state.captainCharacterP2.has_value() ? std::to_string(state.captainCharacterP2.value()) : "not set", 
        state.captainPositionP2.has_value() ? std::to_string(state.captainPositionP2.value()) : "not set");


    if (j.count("Away Logo") && j.count("Home Logo"))
    {
        state.logoAway = static_cast<uint32_t>(j.at("Away Logo").get<double>());
        state.logoHome = static_cast<uint32_t>(j.at("Home Logo").get<double>());
    }
    INFO_LOG_FMT(COMMON, "Logo Away: {}", state.logoAway.has_value() ? std::to_string(state.logoAway.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Logo Home: {}", state.logoHome.has_value() ? std::to_string(state.logoHome.value()) : "not set");

    if (j.count("StadiumID"))
    {
        // need to convert stadium ID in HUD to menu index.
        uint8_t stadiumID = static_cast<uint8_t>(j.at("StadiumID").get<double>());
        uint8_t stadiumIndex;
        if (stadiumID == 1) stadiumIndex = 5; // Bowser Stadium
        else if (stadiumID == 4) stadiumIndex = 1; // Peach Garden
        else if (stadiumID == 5) stadiumIndex = 4; // DK Jungle
        else stadiumIndex = stadiumID; // Rest match directly.

        state.stadium = stadiumIndex;
    }
    INFO_LOG_FMT(COMMON, "Stadium: {}", state.stadium.has_value() ? std::to_string(state.stadium.value()) : "not set");

    // handled with half inning value since the game uses this to load offence/defence, not home or away.
    // if (j.count("First Batting Team"))
    // {
    //     uint8_t firstBattingTeam = static_cast<uint8_t>(j.at("First Batting Team").get<double>()); // 0=Original P1, 1=Original P2
    //     if (p1IsAway)
    //         state.firstBatter = firstBattingTeam;
    //     else
    //         state.firstBatter = (firstBattingTeam == 0) ? 1 : 0;
    // }
    // INFO_LOG_FMT(COMMON, "First Batter: {}", state.firstBatter.has_value() ? std::to_string(state.firstBatter.value()) : "not set");

    if (j.count("Star Skills On"))
        state.starSkills = static_cast<uint8_t>(j.at("Star Skills On").get<double>());
    INFO_LOG_FMT(COMMON, "Star Skills: {}", state.starSkills.has_value() ? std::to_string(state.starSkills.value()) : "not set");

    if (j.count("Innings Selected"))
        state.inningsSelected = static_cast<uint8_t>(j.at("Innings Selected").get<double>());
    INFO_LOG_FMT(COMMON, "Innings Selected: {}", state.inningsSelected.has_value() ? std::to_string(state.inningsSelected.value()) : "not set");

    if (j.count("Mercy On"))
        state.mercy = static_cast<uint8_t>(j.at("Mercy On").get<double>());
    INFO_LOG_FMT(COMMON, "Mercy: {}", state.mercy.has_value() ? std::to_string(state.mercy.value()) : "not set");

    // === IN-GAME STATE ===

    if (j.count("Inning"))
        state.inning = static_cast<uint32_t>(j.at("Inning").get<double>());
    INFO_LOG_FMT(COMMON, "Inning: {}", state.inning.has_value() ? std::to_string(state.inning.value()) : "not set");

    if (j.count("Half Inning"))
    {
        uint8_t halfInning = static_cast<uint8_t>(j.at("Half Inning").get<double>());

        state.halfInning = halfInning;

        state.battingTeam = static_cast<uint32_t>(halfInning);
        state.fieldingTeam = static_cast<uint32_t>(1 - halfInning);

        if (p1IsAway) state.firstBatter = halfInning;
        else state.firstBatter = 1 - halfInning;
    }
    INFO_LOG_FMT(COMMON, "Half Inning: {}", state.halfInning.has_value() ? std::to_string(state.halfInning.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Batting Team: {}", state.battingTeam.has_value() ? std::to_string(state.battingTeam.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Fielding Team: {}", state.fieldingTeam.has_value() ? std::to_string(state.fieldingTeam.value()) : "not set");
    INFO_LOG_FMT(COMMON, "First Batter: {}", state.firstBatter.has_value() ? std::to_string(state.firstBatter.value()) : "not set");

    if (j.count("Away Score"))
        state.awayScore = static_cast<uint16_t>(j.at("Away Score").get<double>());
    INFO_LOG_FMT(COMMON, "Away Score: {}", state.awayScore.has_value() ? std::to_string(state.awayScore.value()) : "not set");

    if (j.count("Home Score"))
        state.homeScore = static_cast<uint16_t>(j.at("Home Score").get<double>());
    INFO_LOG_FMT(COMMON, "Home Score: {}", state.homeScore.has_value() ? std::to_string(state.homeScore.value()) : "not set");

    if (j.count("Away Inning Scores"))
    {
        const picojson::array& scores = j.at("Away Inning Scores").get<picojson::array>();
        for (int i = 0; i < static_cast<int>(scores.size()) && i < 18; i++)
            state.awayInningScores[i] = static_cast<uint16_t>(scores[i].get<double>());
    }

    if (j.count("Home Inning Scores"))
    {
        const picojson::array& scores = j.at("Home Inning Scores").get<picojson::array>();
        for (int i = 0; i < static_cast<int>(scores.size()) && i < 18; i++)
            state.homeInningScores[i] = static_cast<uint16_t>(scores[i].get<double>());
    }
    for (int i = 0; i < 18; i++)
    {
        if (state.awayInningScores[i].has_value())
            INFO_LOG_FMT(COMMON, "Away Inning {} Score: {}", i, state.awayInningScores[i].value());
        if (state.homeInningScores[i].has_value())
            INFO_LOG_FMT(COMMON, "Home Inning {} Score: {}", i, state.homeInningScores[i].value());
    }

    if (j.count("Strikes"))
        state.strikes = static_cast<uint32_t>(j.at("Strikes").get<double>());
    INFO_LOG_FMT(COMMON, "Strikes: {}", state.strikes.has_value() ? std::to_string(state.strikes.value()) : "not set");

    if (j.count("Balls"))
        state.balls = static_cast<uint32_t>(j.at("Balls").get<double>());
    INFO_LOG_FMT(COMMON, "Balls: {}", state.balls.has_value() ? std::to_string(state.balls.value()) : "not set");

    if (j.count("Outs"))
        state.outs = static_cast<uint32_t>(j.at("Outs").get<double>());
    INFO_LOG_FMT(COMMON, "Outs: {}", state.outs.has_value() ? std::to_string(state.outs.value()) : "not set");

    if (j.count("Away Stars"))
    {
        uint8_t awayStars = static_cast<uint8_t>(j.at("Away Stars").get<double>());
        if (p1IsAway)
            state.p1TeamStars = awayStars;
        else
            state.p2TeamStars = awayStars;
    }
    INFO_LOG_FMT(COMMON, "Away Stars assigned to P{}: {}", 
        p1IsAway ? 
            (state.p1TeamStars.has_value() ? std::to_string(state.p1TeamStars.value()) : "not set") : 
            (state.p2TeamStars.has_value() ? std::to_string(state.p2TeamStars.value()) : "not set"),
        p1IsAway ? "1" : "2");

    if (j.count("Home Stars"))
    {
        uint8_t homeStars = static_cast<uint8_t>(j.at("Home Stars").get<double>());
        if (p1IsAway)
            state.p2TeamStars = homeStars;
        else
            state.p1TeamStars = homeStars;
    }
    INFO_LOG_FMT(COMMON, "Home Stars assigned to P{}: {}", 
        p1IsAway ? 
            (state.p2TeamStars.has_value() ? std::to_string(state.p2TeamStars.value()) : "not set") : 
            (state.p1TeamStars.has_value() ? std::to_string(state.p1TeamStars.value()) : "not set"),
        p1IsAway ? "2" : "1");

    if (j.count("Star Chance"))
        state.isStarChance = static_cast<uint8_t>(j.at("Star Chance").get<double>());
    INFO_LOG_FMT(COMMON, "Star Chance: {}", state.isStarChance.has_value() ? std::to_string(state.isStarChance.value()) : "not set");

    // === POSITIONS BY BATTING ORDER ===
    if (j.count("Away Batter Roster Loc") && j.count("Home Batter Roster Loc"))
    {
        int awayStartingBatter = static_cast<int>(j.at("Away Batter Roster Loc").get<double>());
        int homeStartingBatter = static_cast<int>(j.at("Home Batter Roster Loc").get<double>());

        for (int i = 0; i < 9; i++)
        {
            std::string awayKey = "Away Roster " + std::to_string((i + awayStartingBatter) % 9);
            std::string homeKey = "Home Roster " + std::to_string((i + homeStartingBatter) % 9);

            if (j.count(awayKey))
            {
                const picojson::object& roster = j.at(awayKey).get<picojson::object>();
                uint32_t position = static_cast<uint32_t>(roster.at("Fielding Position").get<double>());
                if (position < 9)
                    state.awayPositionByBattingOrder[i] = position;
            }

            if (j.count(homeKey))
            {
                const picojson::object& roster = j.at(homeKey).get<picojson::object>();
                uint32_t position = static_cast<uint32_t>(roster.at("Fielding Position").get<double>());
                if (position < 9)
                    state.homePositionByBattingOrder[i] = position;
            }
        }
    }
    for (int i = 0; i < 9; i++)
    {
        if (state.awayPositionByBattingOrder[i].has_value())
            INFO_LOG_FMT(COMMON, "Away Batting Order {}: Position {}", i, state.awayPositionByBattingOrder[i].value());
        if (state.homePositionByBattingOrder[i].has_value())
            INFO_LOG_FMT(COMMON, "Home Batting Order {}: Position {}", i, state.homePositionByBattingOrder[i].value());
    }

    // === RUNNERS ===
    // Runners are indexed 1-3 for each base (1B, 2B, 3B)
    // runnerRosterSpot and runnerCharacterID are 0-indexed arrays (0=1B, 1=2B, 2=3B)

    const std::string runnerKeys[3] = {"Runner 1B", "Runner 2B", "Runner 3B"};

    for (int i = 0; i < 3; i++)
    {
        const std::string& key = runnerKeys[i];
        if (j.count(key))
        {
            const picojson::object& runner = j.at(key).get<picojson::object>();

            if (runner.count("Runner Roster Loc"))
            {
                uint16_t batterRosterLoc = static_cast<uint16_t>(j.at("Batter Roster Loc").get<double>()); // 1
                uint16_t runnerRosterLocRaw = static_cast<uint16_t>(runner.at("Runner Roster Loc").get<double>()); // 0
                state.runnerRosterSpot[i] = (runnerRosterLocRaw - batterRosterLoc + 9) % 9;
            }

            if (runner.count("Runner Char Id"))
                state.runnerCharacterID[i] = static_cast<uint16_t>(runner.at("Runner Char Id").get<double>());
        }
    }
    for (int i = 0; i < 3; i++)
    {
        INFO_LOG_FMT(COMMON, "Runner {}: RosterSpot={}, CharID={}",
                    runnerKeys[i],
                    state.runnerRosterSpot[i].has_value() ? std::to_string(state.runnerRosterSpot[i].value()) : "not set",
                    state.runnerCharacterID[i].has_value() ? std::to_string(state.runnerCharacterID[i].value()) : "not set");
    }

    // === STAMINA ===
    // Stamina is stored per-character in defensive stats.
    // Your state expects it on a P1/P2 basis by roster ID.

    if (j.count("Away Batter Roster Loc") && j.count("Home Batter Roster Loc"))
    {
        int awayStartingBatter = static_cast<int>(j.at("Away Batter Roster Loc").get<double>());
        int homeStartingBatter = static_cast<int>(j.at("Home Batter Roster Loc").get<double>());

        for (int i = 0; i < 9; i++)
        {
            int awayAdjustedIndex = (i + awayStartingBatter) % 9;
            int homeAdjustedIndex = (i + homeStartingBatter) % 9;

            std::string p1Key = p1IsAway ? "Away Roster " + std::to_string(awayAdjustedIndex)
                                                : "Home Roster " + std::to_string(homeAdjustedIndex);
            std::string p2Key = p1IsAway ? "Home Roster " + std::to_string(homeAdjustedIndex)
                                                : "Away Roster " + std::to_string(awayAdjustedIndex);

            if (j.count(p1Key))
            {
                const picojson::object& roster = j.at(p1Key).get<picojson::object>();
                const picojson::object& defensiveStats = roster.at("Defensive Stats").get<picojson::object>();
                if (defensiveStats.count("Stamina"))
                    state.pitcherStaminaP1[i] = static_cast<uint16_t>(defensiveStats.at("Stamina").get<double>());
            }

            if (j.count(p2Key))
            {
                const picojson::object& roster = j.at(p2Key).get<picojson::object>();
                const picojson::object& defensiveStats = roster.at("Defensive Stats").get<picojson::object>();
                if (defensiveStats.count("Stamina"))
                    state.pitcherStaminaP2[i] = static_cast<uint16_t>(defensiveStats.at("Stamina").get<double>());
            }
        }
    }
    for (int i = 0; i < 9; i++)
    {
        if (state.pitcherStaminaP1[i].has_value())
            INFO_LOG_FMT(COMMON, "P1 Stamina Roster {}: {}", i, state.pitcherStaminaP1[i].value());
        if (state.pitcherStaminaP2[i].has_value())
            INFO_LOG_FMT(COMMON, "P2 Stamina Roster {}: {}", i, state.pitcherStaminaP2[i].value());
    }

    // === DEBUG LOGGING OF LOADED STATE ===
    INFO_LOG_FMT(COMMON, "=== HUD State Loaded ===");

    outState = state;
    return true;
}

int allowLoadFromHUD(const std::string& path,
                     const std::string& p1Username, const std::string& p2Username)
{
    INFO_LOG_FMT(COMMON, "Starting to check if HUD state load is allowed");

    // ===== check HUD file exists and can be parsed. =====
    std::ifstream file(path);
    if (!file.is_open())
    {
        ERROR_LOG_FMT(COMMON, "Failed to open HUD file: {}", path);
        return 2;
    }

    std::string json_str((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());

    picojson::value v;
    std::string err = picojson::parse(v, json_str);

    if (!err.empty())
    {
        ERROR_LOG_FMT(COMMON, "Failed to parse HUD JSON from file: {} ({})", path, err);
        return 2;
    }

    if (!v.is<picojson::object>())
    {
        ERROR_LOG_FMT(COMMON, "HUD JSON is not an object: {}", path);
        return 2;
    }

    const picojson::object& j = v.get<picojson::object>();

    INFO_LOG_FMT(COMMON, "File check passed, checking gamemode.");

    // ===== check lobby gamemode matches HUD =====
    // Get the active tagset from the current netplay session
    std::optional<Tag::TagSet> activeTagSet = Core::GetActiveTagSet(true);

    // Check if HUD file has a TagSetID
    if (j.count("TagSetID"))
    {
        int hudTagSetId = static_cast<int>(j.at("TagSetID").get<double>());

        if (activeTagSet.has_value())
        {
            // Both HUD and lobby have a tagset - they must match
            if (hudTagSetId != activeTagSet.value().id)
            {
                ERROR_LOG_FMT(COMMON, "TagSet mismatch. Lobby TagSet ID={}, HUD TagSet ID={}",
                            activeTagSet.value().id, hudTagSetId);
                return 3;
            }
            INFO_LOG_FMT(COMMON, "TagSet match confirmed: ID={}", hudTagSetId);
        }
        else
        {
            // HUD has a tagset but lobby does not
             ERROR_LOG_FMT(COMMON, "HUD has TagSet ID={} but no game mode is active in lobby.", hudTagSetId);
            // return 3; Actually allowing this situation so no game mode can be used for anything to allow for better customization.
        }
    }
    else if (activeTagSet.has_value())
    {
        // Lobby has a tagset but HUD does not record one
        ERROR_LOG_FMT(COMMON, "Lobby has TagSet ID={} but HUD file has no TagSetID field.",
                    activeTagSet.value().id);
        return 3;
    }

    INFO_LOG_FMT(COMMON, "Gamemode check passed, checking players.");

    // ===== check players match HUD =====
    if (p1Username.empty())
    {
        ERROR_LOG_FMT(COMMON, "Could not find player 1 in netplay data.");
        return 4;
    }
    INFO_LOG_FMT(COMMON, "P1 player: {}", p1Username);
    INFO_LOG_FMT(COMMON, "P2 player: {}", p2Username.empty() ? "none (solo)" : p2Username);

    std::string awayPlayer = j.count("Away Player") ?
        std::string(StripWhitespace(j.at("Away Player").get<std::string>())) : "";
    std::string homePlayer = j.count("Home Player") ?
        std::string(StripWhitespace(j.at("Home Player").get<std::string>())) : "";

    // Validate players match the HUD file
    bool p1IsAway = (p1Username == awayPlayer);
    bool p1IsHome = (p1Username == homePlayer);

    if (!p1IsAway && !p1IsHome)
    {
        ERROR_LOG_FMT(COMMON, "P1 player '{}' not found in HUD file. Away='{}', Home='{}'",
                    p1Username, awayPlayer, homePlayer);
        return 4;
    }

    // If opponent is known, validate they match the other slot
    if (!p2Username.empty())
    {
        bool opponentIsAway = (p2Username == awayPlayer);
        bool opponentIsHome = (p2Username == homePlayer);

        if (!((p1IsAway && opponentIsHome) || (p1IsHome && opponentIsAway)))
        {
            ERROR_LOG_FMT(COMMON, "Player mismatch. P1='{}', Opponent='{}', HUD Away='{}', HUD Home='{}'",
                        p1Username, p2Username, awayPlayer, homePlayer);
            return 4;
        }
    }

    INFO_LOG_FMT(COMMON, "All checks passed. HUD can load.");

    return 0;
}