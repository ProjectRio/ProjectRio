#include "MSB_HudStateLoader.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/LocalPlayers.h"
#include <fstream>
#include <string>
#include <picojson.h>


bool LoadStateFromHud(const std::string& path, MSBGameState& outState)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        ERROR_LOG_FMT(COMMON, "Failed to open HUD file: {}", path);
        return false;
    }

    std::string json_str((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());

    picojson::value v;
    std::string err = picojson::parse(v, json_str);

    if (!err.empty())
    {
        ERROR_LOG_FMT(COMMON, "Failed to parse HUD JSON from file: {} ({})", path, err);
        return false;
    }

    if (!v.is<picojson::object>())
    {
        ERROR_LOG_FMT(COMMON, "HUD JSON is not an object: {}", path);
        return false;
    }

    const picojson::object& j = v.get<picojson::object>();

        // === P1/P2 to HOME/AWAY MAPPING and VERIFICATION ===
    std::string localUsername = StripWhitespace(LocalPlayers::m_online_player.username);
    std::string awayPlayer = j.count("Away Player") ? StripWhitespace(j.at("Away Player").get<std::string>()) : "";
    std::string homePlayer = j.count("Home Player") ? StripWhitespace(j.at("Home Player").get<std::string>()) : "";

    if (awayPlayer == "No Player Selected" || homePlayer == "No Player Selected")
    {
        ERROR_LOG_FMT(COMMON, "HUD file has unselected player. Away='{}', Home='{}'", awayPlayer, homePlayer);
        return false;
    }

    // Find the opponent - the port player who isn't the local player
    std::string opponentUsername = "";
    auto portPlayers = LocalPlayers::GetPortPlayers();
    for (const auto& [port, player] : portPlayers)
    {
        std::string portUsername = StripWhitespace(player.username);
        if (portUsername != localUsername && portUsername != "No Player Selected")
        {
            opponentUsername = portUsername;
            break;
        }
    }

    if (opponentUsername.empty())
    {
        ERROR_LOG_FMT(COMMON, "Could not find opponent in local players config.");
        return false;
    }

    // Validate both players match the HUD file
    bool localIsAway = (localUsername == awayPlayer);
    bool localIsHome = (localUsername == homePlayer);
    bool opponentIsAway = (opponentUsername == awayPlayer);
    bool opponentIsHome = (opponentUsername == homePlayer);

    if (!((localIsAway && opponentIsHome) || (localIsHome && opponentIsAway)))
    {
        ERROR_LOG_FMT(COMMON, "Player mismatch. Local='{}', Opponent='{}', HUD Away='{}', HUD Home='{}'",
                    localUsername, opponentUsername, awayPlayer, homePlayer);
        return false;
    }

    bool localPlayerIsAway = localIsAway;

    MSBGameState state;

    // === PRE-GAME SETTINGS ===
    // === ROSTERS ===
    // Characters are stored in roster order in the HUD file, but your state
    // needs them in position order (P, C, 1B, 2B, 3B, SS, LF, CF, RF).
    // The "Fielding Position" field tells us what position each roster slot plays.
    // Position mapping: 0=P, 1=C, 2=1B, 3=2B, 4=3B, 5=SS, 6=LF, 7=CF, 8=RF

    // If local player is away, away=P1 and home=P2. Otherwise invert.

    for (int i = 0; i < 9; i++)
    {
        std::string p1Key = localPlayerIsAway ? "Away Roster " + std::to_string(i)
                                            : "Home Roster " + std::to_string(i);
        std::string p2Key = localPlayerIsAway ? "Home Roster " + std::to_string(i)
                                            : "Away Roster " + std::to_string(i);

        if (j.count(p1Key))
        {
            const picojson::object& roster = j.at(p1Key).get<picojson::object>();
            uint8_t charID = static_cast<uint8_t>(roster.at("CharID").get<double>());
            uint8_t position = static_cast<uint8_t>(roster.at("Fielding Position").get<double>());
            if (position < 9)
                state.charactersP1ByPosition[position] = charID;
            if (roster.count("Captain") && roster.at("Captain").get<double>() == 1)
                state.captainCharacterP1 = charID;
        }

        if (j.count(p2Key))
        {
            const picojson::object& roster = j.at(p2Key).get<picojson::object>();
            uint8_t charID = static_cast<uint8_t>(roster.at("CharID").get<double>());
            uint8_t position = static_cast<uint8_t>(roster.at("Fielding Position").get<double>());
            if (position < 9)
                state.charactersP2ByPosition[position] = charID;
            if (roster.count("Captain") && roster.at("Captain").get<double>() == 1)
                state.captainCharacterP2 = charID;
        }
    }

    if (j.count("Away Logo") && j.count("Home Logo"))
    {
        uint8_t awayLogo = static_cast<uint8_t>(j.at("Away Logo").get<double>());
        uint8_t homeLogo = static_cast<uint8_t>(j.at("Home Logo").get<double>());
        
        state.logoP1 = localIsAway ? awayLogo : homeLogo;
        state.logoP2 = localIsAway ? homeLogo : awayLogo;
    }
    
    if (j.count("StadiumID"))
        state.stadium = static_cast<uint8_t>(j.at("StadiumID").get<double>());

    // firstBatter: 0 = away bats first, 1 = home bats first.
    // We need to translate this to P1/P2 perspective.
    // If local player is away, firstBatter maps directly.
    // If local player is home, we invert it.
    if (j.count("First Batting Team"))
    {
        uint8_t firstBattingTeam = static_cast<uint8_t>(j.at("First Batting Team").get<double>()); // 0=Original P1, 1=Original P2
        if (localPlayerIsAway)
            state.firstBatter = firstBattingTeam;
        else
            state.firstBatter = (firstBattingTeam == 0) ? 1 : 0;
    }

    if (j.count("Star Skills On"))
        state.starSkills = static_cast<uint8_t>(j.at("Star Skills On").get<double>());

    if (j.count("Innings Selected"))
        state.inningsSelected = static_cast<uint8_t>(j.at("Innings Selected").get<double>());

    if (j.count("Mercy On"))
        state.mercy = static_cast<uint8_t>(j.at("Mercy On").get<double>());

    // === IN-GAME STATE ===

    if (j.count("Inning"))
        state.inning = static_cast<uint32_t>(j.at("Inning").get<double>());

    if (j.count("Half Inning"))
    {
        uint8_t halfInning = static_cast<uint8_t>(j.at("Half Inning").get<double>());

        state.halfInning = halfInning;

        state.battingTeam = static_cast<uint32_t>(halfInning);
        state.fieldingTeam = static_cast<uint32_t>(1 - halfInning);
    }    

    if (j.count("Away Score"))
        state.awayScore = static_cast<uint16_t>(j.at("Away Score").get<double>());

    if (j.count("Home Score"))
        state.homeScore = static_cast<uint16_t>(j.at("Home Score").get<double>());

    if (j.count("Away Inning Scores"))
    {
        const picojson::array& scores = j.at("Away Inning Scores").get<picojson::object>();
        for (int i = 0; i < static_cast<int>(scores.size()) && i < 18; i++)
            state.awayInningScores[i] = static_cast<uint16_t>(scores[i].get<double>());
    }

    if (j.count("Home Inning Scores"))
    {
        const picojson::array& scores = j.at("Home Inning Scores").get<picojson::object>();
        for (int i = 0; i < static_cast<int>(scores.size()) && i < 18; i++)
            state.homeInningScores[i] = static_cast<uint16_t>(scores[i].get<double>());
    }

    if (j.count("Strikes"))
        state.strikes = static_cast<uint32_t>(j.at("Strikes").get<double>());

    if (j.count("Balls"))
        state.balls = static_cast<uint32_t>(j.at("Balls").get<double>());

    if (j.count("Outs"))
        state.outs = static_cast<uint32_t>(j.at("Outs").get<double>());

    if (j.count("Away Stars"))
        state.awayTeamStars = static_cast<uint8_t>(j.at("Away Stars").get<double>());

    if (j.count("Home Stars"))
        state.homeTeamStars = static_cast<uint8_t>(j.at("Home Stars").get<double>());

    if (j.count("Star Chance"))
        state.isStarChance = static_cast<uint8_t>(j.at("Star Chance").get<double>());

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
                uint16_t batterRosterLoc = static_cast<uint16_t>(j.at("Batter Roster Loc").get<double>());
                uint16_t runnerRosterLocRaw = static_cast<uint16_t>(runner.at("Runner Roster Loc").get<double>());
                state.runnerRosterSpot[i] = (batterRosterLoc + runnerRosterLocRaw) % 9;
            }

            if (runner.count("Runner Char Id"))
                state.runnerCharacterID[i] = static_cast<uint16_t>(runner.at("Runner Char Id").get<double>());
        }
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
            
            std::string p1Key = localPlayerIsAway ? "Away Roster " + std::to_string(awayAdjustedIndex)
                                                : "Home Roster " + std::to_string(homeAdjustedIndex);
            std::string p2Key = localPlayerIsAway ? "Home Roster " + std::to_string(homeAdjustedIndex)
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

    // === DEBUG LOGGING OF LOADED STATE ===
    INFO_LOG_FMT(COMMON, "=== HUD State Loaded ===");

    // Pre-game
    INFO_LOG_FMT(COMMON, "Stadium: {}", state.stadium.has_value() ? std::to_string(state.stadium.value()) : "not set");
    INFO_LOG_FMT(COMMON, "First Batter: {}", state.firstBatter.has_value() ? std::to_string(state.firstBatter.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Star Skills: {}", state.starSkills.has_value() ? std::to_string(state.starSkills.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Innings Selected: {}", state.inningsSelected.has_value() ? std::to_string(state.inningsSelected.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Mercy: {}", state.mercy.has_value() ? std::to_string(state.mercy.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Logo P1: {}", state.logoP1.has_value() ? std::to_string(state.logoP1.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Logo P2: {}", state.logoP2.has_value() ? std::to_string(state.logoP2.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Captain P1: {}", state.captainCharacterP1.has_value() ? std::to_string(state.captainCharacterP1.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Captain P2: {}", state.captainCharacterP2.has_value() ? std::to_string(state.captainCharacterP2.value()) : "not set");

    // Rosters
    for (int i = 0; i < 9; i++)
        INFO_LOG_FMT(COMMON, "P1 Position {}: {}", i, state.charactersP1ByPosition[i].has_value() ? std::to_string(state.charactersP1ByPosition[i].value()) : "not set");
    for (int i = 0; i < 9; i++)
        INFO_LOG_FMT(COMMON, "P2 Position {}: {}", i, state.charactersP2ByPosition[i].has_value() ? std::to_string(state.charactersP2ByPosition[i].value()) : "not set");

    // In-game
    INFO_LOG_FMT(COMMON, "Inning: {}", state.inning.has_value() ? std::to_string(state.inning.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Half Inning: {}", state.halfInning.has_value() ? std::to_string(state.halfInning.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Batting Team: {}", state.battingTeam.has_value() ? std::to_string(state.battingTeam.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Fielding Team: {}", state.fieldingTeam.has_value() ? std::to_string(state.fieldingTeam.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Away Score: {}", state.awayScore.has_value() ? std::to_string(state.awayScore.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Home Score: {}", state.homeScore.has_value() ? std::to_string(state.homeScore.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Strikes: {}", state.strikes.has_value() ? std::to_string(state.strikes.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Balls: {}", state.balls.has_value() ? std::to_string(state.balls.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Outs: {}", state.outs.has_value() ? std::to_string(state.outs.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Away Stars: {}", state.awayTeamStars.has_value() ? std::to_string(state.awayTeamStars.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Home Stars: {}", state.homeTeamStars.has_value() ? std::to_string(state.homeTeamStars.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Star Chance: {}", state.isStarChance.has_value() ? std::to_string(state.isStarChance.value()) : "not set");

    // Inning scores
    for (int i = 0; i < 18; i++)
    {
        if (state.awayInningScores[i].has_value())
            INFO_LOG_FMT(COMMON, "Away Inning {} Score: {}", i, state.awayInningScores[i].value());
        if (state.homeInningScores[i].has_value())
            INFO_LOG_FMT(COMMON, "Home Inning {} Score: {}", i, state.homeInningScores[i].value());
    }

    // Batting order
    for (int i = 0; i < 9; i++)
    {
        if (state.awayPositionByBattingOrder[i].has_value())
            INFO_LOG_FMT(COMMON, "Away Batting Order {}: Position {}", i, state.awayPositionByBattingOrder[i].value());
        if (state.homePositionByBattingOrder[i].has_value())
            INFO_LOG_FMT(COMMON, "Home Batting Order {}: Position {}", i, state.homePositionByBattingOrder[i].value());
    }

    // Runners
    //const std::string runnerLogKeys[3] = {"Runner 1B", "Runner 2B", "Runner 3B"};
    for (int i = 0; i < 3; i++)
    {
        INFO_LOG_FMT(COMMON, "Runner {}: RosterSpot={}, CharID={}",
                    runnerLogKeys[i],
                    state.runnerRosterSpot[i].has_value() ? std::to_string(state.runnerRosterSpot[i].value()) : "not set",
                    state.runnerCharacterID[i].has_value() ? std::to_string(state.runnerCharacterID[i].value()) : "not set");
    }

    // Stamina
    for (int i = 0; i < 9; i++)
    {
        if (state.pitcherStaminaP1[i].has_value())
            INFO_LOG_FMT(COMMON, "P1 Stamina Roster {}: {}", i, state.pitcherStaminaP1[i].value());
        if (state.pitcherStaminaP2[i].has_value())
            INFO_LOG_FMT(COMMON, "P2 Stamina Roster {}: {}", i, state.pitcherStaminaP2[i].value());
    }

    INFO_LOG_FMT(COMMON, "=== End HUD State ===");

    outState = state;
    return true;
}