#include "Core/MSB_HUDStateLoader.h"
#include "Core/MSB_GenerateQuickMatchSetupGeckoCode.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/TagSet.h"
#include "Core/Core.h"
#include <fstream>
#include <string>
#include <picojson.h>



bool LoadStateFromHud(const std::string& path, MSB_QuickMatchState& outState,
                      const std::string& p1Username, const std::string& p2Username)
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

    bool p1IsAway = (p1Username == awayPlayer);
    bool p1IsHome = (p1Username == homePlayer);

    if (!p1IsAway && !p1IsHome)
    {
        ERROR_LOG_FMT(COMMON, "P1 player '{}' not found in HUD file. Away='{}', Home='{}'",
                    p1Username, awayPlayer, homePlayer);
        return false;
    }

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
    }
    else
    {
        INFO_LOG_FMT(COMMON, "Solo game detected, skipping opponent validation.");
    }

    INFO_LOG_FMT(COMMON, "Starting parsing HUD to fill out MSB_QuickMatchState");

    MSB_QuickMatchState state;
    state.SetP1IsAway(p1IsAway);

    // === PRE-GAME SETTINGS ===

    if (j.count("StadiumID"))
    {
        uint8_t stadiumID = static_cast<uint8_t>(j.at("StadiumID").get<double>());
        uint8_t stadiumIndex;
        if (stadiumID == 1)      stadiumIndex = 5; // Bowser Stadium
        else if (stadiumID == 4) stadiumIndex = 1; // Peach Garden
        else if (stadiumID == 5) stadiumIndex = 4; // DK Jungle
        else                     stadiumIndex = stadiumID;

        state.SetStadium(stadiumIndex);
    }
    INFO_LOG_FMT(COMMON, "Stadium: {}",
        state.GetStadium().has_value() ? std::to_string(state.GetStadium().value()) : "not set");

    if (j.count("Star Skills On"))
        state.SetStarSkills(static_cast<uint8_t>(j.at("Star Skills On").get<double>()));
    INFO_LOG_FMT(COMMON, "Star Skills: {}",
        state.GetStarSkills().has_value() ? std::to_string(state.GetStarSkills().value()) : "not set");

    if (j.count("Innings Selected"))
        state.SetInningsSelected(static_cast<uint8_t>(j.at("Innings Selected").get<double>()));
    INFO_LOG_FMT(COMMON, "Innings Selected: {}",
        state.GetInningsSelected().has_value() ? std::to_string(state.GetInningsSelected().value()) : "not set");

    if (j.count("Mercy On"))
        state.SetMercy(static_cast<uint8_t>(j.at("Mercy On").get<double>()));
    INFO_LOG_FMT(COMMON, "Mercy: {}",
        state.GetMercy().has_value() ? std::to_string(state.GetMercy().value()) : "not set");

    // === IN-GAME STATE ===

    if (j.count("Inning"))
        state.SetInning(static_cast<uint32_t>(j.at("Inning").get<double>()));
    INFO_LOG_FMT(COMMON, "Inning: {}",
        state.GetInning().has_value() ? std::to_string(state.GetInning().value()) : "not set");

    if (j.count("Half Inning"))
    {
        uint8_t halfInning = static_cast<uint8_t>(j.at("Half Inning").get<double>());
        state.SetHalfInning(halfInning);
        state.SetBattingTeam(static_cast<uint32_t>(halfInning));
        state.SetFieldingTeam(static_cast<uint32_t>(1 - halfInning));
        state.SetFirstBatter(p1IsAway ? halfInning : 1 - halfInning);
    }
    INFO_LOG_FMT(COMMON, "Half Inning: {}",
        state.GetHalfInning().has_value() ? std::to_string(state.GetHalfInning().value()) : "not set");
    INFO_LOG_FMT(COMMON, "Batting Team: {}",
        state.GetBattingTeam().has_value() ? std::to_string(state.GetBattingTeam().value()) : "not set");
    INFO_LOG_FMT(COMMON, "Fielding Team: {}",
        state.GetFieldingTeam().has_value() ? std::to_string(state.GetFieldingTeam().value()) : "not set");
    INFO_LOG_FMT(COMMON, "First Batter: {}",
        state.GetFirstBatter().has_value() ? std::to_string(state.GetFirstBatter().value()) : "not set");

    if (j.count("Away Score"))
        state.SetAwayScore(static_cast<uint16_t>(j.at("Away Score").get<double>()));
    INFO_LOG_FMT(COMMON, "Away Score: {}",
        state.GetAwayScore().has_value() ? std::to_string(state.GetAwayScore().value()) : "not set");

    if (j.count("Home Score"))
        state.SetHomeScore(static_cast<uint16_t>(j.at("Home Score").get<double>()));
    INFO_LOG_FMT(COMMON, "Home Score: {}",
        state.GetHomeScore().has_value() ? std::to_string(state.GetHomeScore().value()) : "not set");

    if (j.count("Away Inning Scores"))
    {
        const picojson::array& scores = j.at("Away Inning Scores").get<picojson::array>();
        for (int i = 0; i < static_cast<int>(scores.size()) && i < 18; i++)
            state.SetAwayInningScore(i, static_cast<uint16_t>(scores[i].get<double>()));
    }

    if (j.count("Home Inning Scores"))
    {
        const picojson::array& scores = j.at("Home Inning Scores").get<picojson::array>();
        for (int i = 0; i < static_cast<int>(scores.size()) && i < 18; i++)
            state.SetHomeInningScore(i, static_cast<uint16_t>(scores[i].get<double>()));
    }

    for (int i = 0; i < 18; i++)
    {
        if (state.GetAwayInningScore(i).has_value())
            INFO_LOG_FMT(COMMON, "Away Inning {} Score: {}", i, state.GetAwayInningScore(i).value());
        if (state.GetHomeInningScore(i).has_value())
            INFO_LOG_FMT(COMMON, "Home Inning {} Score: {}", i, state.GetHomeInningScore(i).value());
    }

    if (j.count("Strikes"))
        state.SetStrikes(static_cast<uint32_t>(j.at("Strikes").get<double>()));
    INFO_LOG_FMT(COMMON, "Strikes: {}",
        state.GetStrikes().has_value() ? std::to_string(state.GetStrikes().value()) : "not set");

    if (j.count("Balls"))
        state.SetBalls(static_cast<uint32_t>(j.at("Balls").get<double>()));
    INFO_LOG_FMT(COMMON, "Balls: {}",
        state.GetBalls().has_value() ? std::to_string(state.GetBalls().value()) : "not set");

    if (j.count("Outs"))
        state.SetOuts(static_cast<uint32_t>(j.at("Outs").get<double>()));
    INFO_LOG_FMT(COMMON, "Outs: {}",
        state.GetOuts().has_value() ? std::to_string(state.GetOuts().value()) : "not set");

    if (j.count("Star Chance"))
        state.SetIsStarChance(static_cast<uint8_t>(j.at("Star Chance").get<double>()));
    INFO_LOG_FMT(COMMON, "Star Chance: {}",
        state.GetIsStarChance().has_value() ? std::to_string(state.GetIsStarChance().value()) : "not set");

    // === ROSTERS, CAPTAINS, LOGOS, TEAM STARS, RUNNERS ===

    int awayStartingBatter = j.count("Away Batter Roster Loc") ?
        static_cast<int>(j.at("Away Batter Roster Loc").get<double>()) : 0;
    int homeStartingBatter = j.count("Home Batter Roster Loc") ?
        static_cast<int>(j.at("Home Batter Roster Loc").get<double>()) : 0;

    int p1StartingBatter = p1IsAway ? awayStartingBatter : homeStartingBatter;
    int p2StartingBatter = p1IsAway ? homeStartingBatter : awayStartingBatter;

    MSB_Team p1Team, p2Team;
    std::optional<uint8_t> captainBattingSlotP1, captainBattingSlotP2;

    for (int i = 0; i < 9; i++)
    {
        std::string p1Key = p1IsAway ? "Away Roster " + std::to_string(i)
                                     : "Home Roster " + std::to_string(i);
        std::string p2Key = p1IsAway ? "Home Roster " + std::to_string(i)
                                     : "Away Roster " + std::to_string(i);

        if (j.count(p1Key))
        {
            const picojson::object& roster = j.at(p1Key).get<picojson::object>();
            uint8_t charID       = static_cast<uint8_t>(roster.at("CharID").get<double>());
            uint8_t position     = static_cast<uint8_t>(roster.at("Fielding Position").get<double>());
            uint8_t battingHand  = static_cast<uint8_t>(roster.at("Batting Hand").get<double>());
            uint8_t fieldingHand = static_cast<uint8_t>(roster.at("Fielding Hand").get<double>());
            uint8_t superstar    = static_cast<uint8_t>(roster.at("Superstar").get<double>());
            uint8_t battingSlot  = static_cast<uint8_t>((i - p1StartingBatter + 9) % 9);
            uint16_t stamina = 10;
            if (roster.count("Defensive Stats"))
            {
                const picojson::object& def = roster.at("Defensive Stats").get<picojson::object>();
                if (def.count("Stamina"))
                    stamina = static_cast<uint16_t>(def.at("Stamina").get<double>());
            }
            if (position < 9)
            {
                MSB_Player player(charID, position, battingSlot, battingHand, fieldingHand, superstar, stamina);
                p1Team.SetPlayer(position, player);
            }
            if (roster.count("Captain") && roster.at("Captain").get<double>() == 1)
                captainBattingSlotP1 = battingSlot;
        }

        if (j.count(p2Key))
        {
            const picojson::object& roster = j.at(p2Key).get<picojson::object>();
            uint8_t charID       = static_cast<uint8_t>(roster.at("CharID").get<double>());
            uint8_t position     = static_cast<uint8_t>(roster.at("Fielding Position").get<double>());
            uint8_t battingHand  = static_cast<uint8_t>(roster.at("Batting Hand").get<double>());
            uint8_t fieldingHand = static_cast<uint8_t>(roster.at("Fielding Hand").get<double>());
            uint8_t superstar    = static_cast<uint8_t>(roster.at("Superstar").get<double>());
            uint8_t battingSlot  = static_cast<uint8_t>((i - p2StartingBatter + 9) % 9);
            uint16_t stamina = 10;
            if (roster.count("Defensive Stats"))
            {
                const picojson::object& def = roster.at("Defensive Stats").get<picojson::object>();
                if (def.count("Stamina"))
                    stamina = static_cast<uint16_t>(def.at("Stamina").get<double>());
            }
            if (position < 9)
            {
                MSB_Player player(charID, position, battingSlot, battingHand, fieldingHand, superstar, stamina);
                p2Team.SetPlayer(position, player);
            }
            if (roster.count("Captain") && roster.at("Captain").get<double>() == 1)
                captainBattingSlotP2 = battingSlot;
        }
    }

    if (captainBattingSlotP1.has_value())
        p1Team.SetCaptainBattingSlot(captainBattingSlotP1.value());
    if (captainBattingSlotP2.has_value())
        p2Team.SetCaptainBattingSlot(captainBattingSlotP2.value());

    if (j.count("Away Logo"))
    {
        uint32_t awayLogo = static_cast<uint32_t>(j.at("Away Logo").get<double>());
        if (p1IsAway) p1Team.SetLogo(awayLogo);
        else          p2Team.SetLogo(awayLogo);
    }
    if (j.count("Home Logo"))
    {
        uint32_t homeLogo = static_cast<uint32_t>(j.at("Home Logo").get<double>());
        if (p1IsAway) p2Team.SetLogo(homeLogo);
        else          p1Team.SetLogo(homeLogo);
    }

    if (j.count("Away Stars"))
    {
        uint8_t awayStars = static_cast<uint8_t>(j.at("Away Stars").get<double>());
        if (p1IsAway) p1Team.SetTeamStars(awayStars);
        else          p2Team.SetTeamStars(awayStars);
    }
    if (j.count("Home Stars"))
    {
        uint8_t homeStars = static_cast<uint8_t>(j.at("Home Stars").get<double>());
        if (p1IsAway) p2Team.SetTeamStars(homeStars);
        else          p1Team.SetTeamStars(homeStars);
    }

    state.SetP1(p1Team);
    state.SetP2(p2Team);

    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* pl = state.GetP1().GetPlayer(static_cast<uint8_t>(i));
        if (pl && pl->IsSet())
            INFO_LOG_FMT(COMMON,
                "P1 Position {}: CharID={}, BatSlot={}, BatHand={}, FieldHand={}, SS={}, Stamina={}",
                i,
                pl->charID.has_value()          ? std::to_string(pl->charID.value())          : "-",
                pl->battingOrderSlot.has_value() ? std::to_string(pl->battingOrderSlot.value()): "-",
                pl->battingHand.has_value()      ? std::to_string(pl->battingHand.value())     : "-",
                pl->fieldingHand.has_value()     ? std::to_string(pl->fieldingHand.value())    : "-",
                pl->superstar.has_value()        ? std::to_string(pl->superstar.value())       : "-",
                pl->pitcherStamina.has_value()   ? std::to_string(pl->pitcherStamina.value())  : "-");
    }
    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* pl = state.GetP2().GetPlayer(static_cast<uint8_t>(i));
        if (pl && pl->IsSet())
            INFO_LOG_FMT(COMMON,
                "P2 Position {}: CharID={}, BatSlot={}, BatHand={}, FieldHand={}, SS={}, Stamina={}",
                i,
                pl->charID.has_value()          ? std::to_string(pl->charID.value())          : "-",
                pl->battingOrderSlot.has_value() ? std::to_string(pl->battingOrderSlot.value()): "-",
                pl->battingHand.has_value()      ? std::to_string(pl->battingHand.value())     : "-",
                pl->fieldingHand.has_value()     ? std::to_string(pl->fieldingHand.value())    : "-",
                pl->superstar.has_value()        ? std::to_string(pl->superstar.value())       : "-",
                pl->pitcherStamina.has_value()   ? std::to_string(pl->pitcherStamina.value())  : "-");
    }
    INFO_LOG_FMT(COMMON, "P1 Captain BattingSlot: {}",
        state.GetP1().GetCaptainBattingSlot().has_value()
            ? std::to_string(state.GetP1().GetCaptainBattingSlot().value()) : "not set");
    INFO_LOG_FMT(COMMON, "P2 Captain BattingSlot: {}",
        state.GetP2().GetCaptainBattingSlot().has_value()
            ? std::to_string(state.GetP2().GetCaptainBattingSlot().value()) : "not set");
    INFO_LOG_FMT(COMMON, "P1 Logo: {}",
        state.GetP1().GetLogo().has_value() ? std::to_string(state.GetP1().GetLogo().value()) : "not set");
    INFO_LOG_FMT(COMMON, "P2 Logo: {}",
        state.GetP2().GetLogo().has_value() ? std::to_string(state.GetP2().GetLogo().value()) : "not set");
    INFO_LOG_FMT(COMMON, "P1 TeamStars: {}",
        state.GetP1().GetTeamStars().has_value() ? std::to_string(state.GetP1().GetTeamStars().value()) : "not set");
    INFO_LOG_FMT(COMMON, "P2 TeamStars: {}",
        state.GetP2().GetTeamStars().has_value() ? std::to_string(state.GetP2().GetTeamStars().value()) : "not set");

    // === RUNNERS ===
    const std::string runnerKeys[3] = {"Runner 1B", "Runner 2B", "Runner 3B"};
    int battingTeamStartingBatter = 0;
    if (state.GetHalfInning().has_value())
    {
        bool awayIsBatting = (state.GetHalfInning().value() == 0);
        battingTeamStartingBatter = awayIsBatting ? awayStartingBatter : homeStartingBatter;
    }

    for (int i = 0; i < 3; i++)
    {
        const std::string& key = runnerKeys[i];
        if (!j.count(key)) continue;
        const picojson::object& runner = j.at(key).get<picojson::object>();
        if (runner.count("Runner Roster Loc"))
        {
            uint16_t runnerRosterLocRaw = static_cast<uint16_t>(runner.at("Runner Roster Loc").get<double>());
            uint8_t battingSlot = static_cast<uint8_t>((runnerRosterLocRaw - battingTeamStartingBatter + 9) % 9);
            state.SetRunner(i, battingSlot);
        }
    }
    for (int i = 0; i < 3; i++)
    {
        INFO_LOG_FMT(COMMON, "Runner {}: FieldingPos={}, CharID={}",
            runnerKeys[i],
            state.GetRunnerFieldingPosition(i).has_value()
                ? std::to_string(state.GetRunnerFieldingPosition(i).value()) : "not set",
            state.GetRunnerCharacterID(i).has_value()
                ? std::to_string(state.GetRunnerCharacterID(i).value()) : "not set");
    }

    INFO_LOG_FMT(COMMON, "=== MSB_QuickMatchState loaded ===");

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