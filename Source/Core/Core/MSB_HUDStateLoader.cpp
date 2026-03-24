#include "MSB_HudStateLoader.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;


bool LoadStateFromHud(const std::string& path, MSBGameState& outState)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        ERROR_LOG_FMT(COMMON, "Failed to open HUD file: {}", path);
        return false;
    }

    json j = json::parse(file, nullptr, false);
    if (j.is_discarded())
    {
        ERROR_LOG_FMT(COMMON, "Failed to parse HUD JSON from file: {}", path);
        return false;
    }

    MSBGameState state;

    // === GAME SETTINGS ===

    if (j.contains("StadiumID"))
        state.stadium = j["StadiumID"].get<uint8_t>();

    if (j.contains("Innings Selected"))
        state.inningsSelected = j["Innings Selected"].get<uint8_t>();

    if (j.contains("First Batting Team"))
        state.firstBatter = j["First Batting Team"].get<uint8_t>();

    if (j.contains("Star Skills On"))
        state.starSkills = j["Star Skills On"].get<uint8_t>();

    if (j.contains("Mercy On"))
        state.mercy = j["Mercy On"].get<uint8_t>();


    // === IN-GAME STATE ===

    if (j.contains("Inning"))
        state.inning = j["Inning"].get<uint32_t>();

    if (j.contains("Half Inning"))
    {
        uint8_t halfInning = j["Half Inning"].get<uint8_t>();

        state.halfInning = halfInning;

        state.battingTeam = halfInning;
        state.fieldingTeam = 1 - halfInning;
    }    

    if (j.contains("Away Score"))
        state.awayScore = j["Away Score"].get<uint16_t>();

    if (j.contains("Home Score"))
        state.homeScore = j["Home Score"].get<uint16_t>();

    if (j.contains("Balls"))
        state.balls = j["Balls"].get<uint32_t>();

    if (j.contains("Strikes"))
        state.strikes = j["Strikes"].get<uint32_t>();

    if (j.contains("Outs"))
        state.outs = j["Outs"].get<uint32_t>();

    if (j.contains("Away Stars"))
        state.awayTeamStars = j["Away Stars"].get<uint8_t>();

    if (j.contains("Home Stars"))
        state.homeTeamStars = j["Home Stars"].get<uint8_t>();

    if (j.contains("Star Chance"))
        state.isStarChance = j["Star Chance"].get<uint8_t>();

    // === INNING SCORES ===

    // if (j.contains("Away Inning Scores"))
    // {
    //     const auto& scores = j["Away Inning Scores"];
    //     for (int i = 0; i < static_cast<int>(scores.size()) && i < 18; i++)
    //         state.awayInningScores[i] = scores[i].get<uint16_t>();
    // }

    // if (j.contains("Home Inning Scores"))
    // {
    //     const auto& scores = j["Home Inning Scores"];
    //     for (int i = 0; i < static_cast<int>(scores.size()) && i < 18; i++)
    //         state.homeInningScores[i] = scores[i].get<uint16_t>();
    // }

    // // === P1/P2 to HOME/AWAY MAPPING ===
    // // Determine which player is P1 (the local player) and whether they are home or away.
    // // This is needed to correctly assign rosters and the first batting setting.

    // std::string localUsername = StripWhitespace(LocalPlayers::m_online_player.username);
    // std::string awayPlayer = j.contains("Away Player") ? StripWhitespace(j["Away Player"].get<std::string>()) : "";
    // std::string homePlayer = j.contains("Home Player") ? StripWhitespace(j["Home Player"].get<std::string>()) : "";

    // if (localUsername != awayPlayer && localUsername != homePlayer)
    // {
    //     ERROR_LOG_FMT(COMMON, "Local player '{}' not found in HUD file. Away='{}', Home='{}'",
    //                 localUsername, awayPlayer, homePlayer);
    //     return false;
    // }

    // bool localPlayerIsAway = (localUsername == awayPlayer);

    // // firstBatter: 0 = away bats first, 1 = home bats first.
    // // We need to translate this to P1/P2 perspective.
    // // If local player is away, firstBatter maps directly.
    // // If local player is home, we invert it.
    // if (j.contains("First Batting Team"))
    // {
    //     uint8_t firstBattingTeam = j["First Batting Team"].get<uint8_t>(); // 0=away, 1=home
    //     if (localPlayerIsAway)
    //         state.firstBatter = firstBattingTeam;
    //     else
    //         state.firstBatter = (firstBattingTeam == 0) ? 1 : 0;
    // }

    // // === ROSTERS ===
    // // Characters are stored in roster order in the HUD file, but your state
    // // needs them in position order (P, C, 1B, 2B, 3B, SS, LF, CF, RF).
    // // The "Fielding Position" field tells us what position each roster slot plays.
    // // Position mapping: 0=P, 1=C, 2=1B, 3=2B, 4=3B, 5=SS, 6=LF, 7=CF, 8=RF

    // // === ROSTERS ===
    // // If local player is away, away=P1 and home=P2. Otherwise invert.

    // for (int i = 0; i < 9; i++)
    // {
    //     std::string p1Key = localPlayerIsAway ? "Away Roster " + std::to_string(i)
    //                                         : "Home Roster " + std::to_string(i);
    //     std::string p2Key = localPlayerIsAway ? "Home Roster " + std::to_string(i)
    //                                         : "Away Roster " + std::to_string(i);

    //     if (j.contains(p1Key))
    //     {
    //         const auto& roster = j[p1Key];
    //         uint8_t charID = roster["CharID"].get<uint8_t>();
    //         uint8_t position = roster["Fielding Position"].get<uint8_t>();
    //         if (position < 9)
    //             state.charactersP1ByPosition[position] = charID;
    //         if (roster.contains("Captain") && roster["Captain"].get<int>() == 1)
    //             state.captainCharacterP1 = charID;
    //     }

    //     if (j.contains(p2Key))
    //     {
    //         const auto& roster = j[p2Key];
    //         uint8_t charID = roster["CharID"].get<uint8_t>();
    //         uint8_t position = roster["Fielding Position"].get<uint8_t>();
    //         if (position < 9)
    //             state.charactersP2ByPosition[position] = charID;
    //         if (roster.contains("Captain") && roster["Captain"].get<int>() == 1)
    //             state.captainCharacterP2 = charID;
    //     }
    // }

    // // === STAMINA ===
    // // Stamina is stored per-character in defensive stats.
    // // Your state expects it on a P1/P2 basis by roster ID.

    // for (int i = 0; i < 9; i++)
    // {
    //     std::string p1Key = localPlayerIsAway ? "Away Roster " + std::to_string(i)
    //                                         : "Home Roster " + std::to_string(i);
    //     std::string p2Key = localPlayerIsAway ? "Home Roster " + std::to_string(i)
    //                                         : "Away Roster " + std::to_string(i);

    //     if (j.contains(p1Key))
    //     {
    //         const auto& defensiveStats = j[p1Key]["Defensive Stats"];
    //         if (defensiveStats.contains("Stamina"))
    //             state.pitcherStaminaP1[i] = defensiveStats["Stamina"].get<uint16_t>();
    //     }

    //     if (j.contains(p2Key))
    //     {
    //         const auto& defensiveStats = j[p2Key]["Defensive Stats"];
    //         if (defensiveStats.contains("Stamina"))
    //             state.pitcherStaminaP2[i] = defensiveStats["Stamina"].get<uint16_t>();
    //     }
    // }

    INFO_LOG_FMT(COMMON, "HUD state loaded successfully from {}", path);
    outState = state;
    return true;
}