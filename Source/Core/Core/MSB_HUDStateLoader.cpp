#include "Core/MSB_HUDStateLoader.h"
#include "Core/MSB_GenerateQuickMatchSetupGeckoCode.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/TagSet.h"
#include "Core/Core.h"
#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <picojson.h>

namespace
{
// HUD text comes from disk and from other netplay players, so none of it can be trusted.
// picojson's get<T>() aborts on a type mismatch and at() throws on a missing key, so HUD fields
// are only read through the TryGet helpers below: they check the key and its type, log the
// field's name and return false, and the caller returns its usual failure result.

// Far deeper than anything the stat tracker writes.
constexpr int HUD_MAX_JSON_DEPTH = 100;
// Highest character in the game (see cCharIdToCharName).
constexpr uint8_t HUD_MAX_CHAR_ID = 0x35;

// picojson::parse can also take Rio down on its own: a number too big for a double ("1e999")
// reaches the abort in value(double), and it recurses once per level of nesting with no limit.
class HUDParseContext : public picojson::default_parse_context
{
public:
    HUDParseContext(picojson::value* out, int depth, bool* hasBadNumber)
        : picojson::default_parse_context(out), m_depth(depth), m_hasBadNumber(hasBadNumber)
    {
    }

    bool set_number(double f)
    {
        // picojson ignores this function's result, so a flag has to carry the failure out.
        if (!std::isfinite(f))
        {
            *m_hasBadNumber = true;
            return true;
        }
        return picojson::default_parse_context::set_number(f);
    }

    template <typename Iter>
    bool parse_array_item(picojson::input<Iter>& in, size_t)
    {
        if (m_depth >= HUD_MAX_JSON_DEPTH)
            return false;
        picojson::array& a = out_->get<picojson::array>();
        a.push_back(picojson::value());
        HUDParseContext ctx(&a.back(), m_depth + 1, m_hasBadNumber);
        return picojson::_parse(ctx, in);
    }

    template <typename Iter>
    bool parse_object_item(picojson::input<Iter>& in, const std::string& key)
    {
        if (m_depth >= HUD_MAX_JSON_DEPTH)
            return false;
        picojson::object& o = out_->get<picojson::object>();
        HUDParseContext ctx(&o[key], m_depth + 1, m_hasBadNumber);
        return picojson::_parse(ctx, in);
    }

private:
    int m_depth;
    bool* m_hasBadNumber;
};

// Returns the root object of the HUD, which lives in v, or nullptr if there isn't one.
const picojson::object* ParseHUDObject(const std::string& json, picojson::value& v)
{
    bool hasBadNumber = false;
    HUDParseContext ctx(&v, 0, &hasBadNumber);
    std::string err;
    picojson::_parse(ctx, json.begin(), json.end(), &err);

    if (!err.empty())
    {
        ERROR_LOG_FMT(COMMON, "Failed to parse HUD JSON ({})", err);
        return nullptr;
    }

    if (hasBadNumber)
    {
        ERROR_LOG_FMT(COMMON, "HUD JSON has a number that is out of range");
        return nullptr;
    }

    if (!v.is<picojson::object>())
    {
        ERROR_LOG_FMT(COMMON, "HUD JSON is not an object");
        return nullptr;
    }

    return &v.get<picojson::object>();
}

bool TryGetString(const picojson::object& obj, const std::string& key, std::string* out)
{
    const auto it = obj.find(key);
    if (it == obj.end() || !it->second.is<std::string>())
    {
        ERROR_LOG_FMT(COMMON, "HUD field '{}' is missing or not a string.", key);
        return false;
    }
    *out = it->second.get<std::string>();
    return true;
}

bool TryGetNumber(const picojson::object& obj, const std::string& key, double* out)
{
    const auto it = obj.find(key);
    if (it == obj.end() || !it->second.is<double>())
    {
        ERROR_LOG_FMT(COMMON, "HUD field '{}' is missing or not a number.", key);
        return false;
    }
    *out = it->second.get<double>();
    return true;
}

bool TryGetObject(const picojson::object& obj, const std::string& key, const picojson::object** out)
{
    const auto it = obj.find(key);
    if (it == obj.end() || !it->second.is<picojson::object>())
    {
        ERROR_LOG_FMT(COMMON, "HUD field '{}' is missing or not an object.", key);
        return false;
    }
    *out = &it->second.get<picojson::object>();
    return true;
}

bool TryGetArray(const picojson::object& obj, const std::string& key, const picojson::array** out)
{
    const auto it = obj.find(key);
    if (it == obj.end() || !it->second.is<picojson::array>())
    {
        ERROR_LOG_FMT(COMMON, "HUD field '{}' is missing or not an array.", key);
        return false;
    }
    *out = &it->second.get<picojson::array>();
    return true;
}

// Casting a double that doesn't fit the target type is undefined behaviour, so check it first.
// Fractions truncate, the same as the cast.
template <typename T>
bool TryConvertToInt(double number, const std::string& name, T* out)
{
    const double truncated = std::trunc(number);
    if (truncated < static_cast<double>(std::numeric_limits<T>::lowest()) ||
        truncated > static_cast<double>(std::numeric_limits<T>::max()))
    {
        ERROR_LOG_FMT(COMMON, "HUD field '{}' is out of range ({}).", name, number);
        return false;
    }
    *out = static_cast<T>(truncated);
    return true;
}

template <typename T>
bool TryGetInt(const picojson::object& obj, const std::string& key, T* out)
{
    double number = 0;
    return TryGetNumber(obj, key, &number) && TryConvertToInt(number, key, out);
}

// For fields a HUD may leave out: a missing key is fine and leaves out unset, a bad value is not.
template <typename T>
bool TryGetOptionalInt(const picojson::object& obj, const std::string& key, std::optional<T>* out)
{
    if (!obj.count(key))
        return true;
    T value = 0;
    if (!TryGetInt(obj, key, &value))
        return false;
    *out = value;
    return true;
}

template <typename T>
bool TryGetArrayInt(const picojson::array& array, size_t index, const std::string& name, T* out)
{
    if (!array[index].is<double>())
    {
        ERROR_LOG_FMT(COMMON, "HUD field '{}' has an entry that is not a number.", name);
        return false;
    }
    return TryConvertToInt(array[index].get<double>(), name, out);
}

// "Away Player" and "Home Player" may be left out, which reads as an empty name.
bool TryGetPlayerName(const picojson::object& obj, const std::string& key, std::string* out)
{
    out->clear();
    if (!obj.count(key))
        return true;
    std::string name;
    if (!TryGetString(obj, key, &name))
        return false;
    *out = std::string(StripWhitespace(name));
    return true;
}
} // namespace


bool ReadHUDFile(const std::string& path, std::string& outJson)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        ERROR_LOG_FMT(COMMON, "Failed to open HUD file: {}", path);
        return false;
    }

    outJson.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return true;
}

bool GetHUDEventInfo(const std::string& json, HUDEventInfo& outInfo)
{
    picojson::value v;
    const picojson::object* root = ParseHUDObject(json, v);
    if (!root)
        return false;

    const picojson::object& j = *root;
    std::string eventNum;
    if (!TryGetString(j, "Event Num", &eventNum))
        return false;

    // "Event Num" is the event index plus which of the event's two HUD writes this is:
    // "a" = before the pitch, "b" = after the play.
    size_t digits = 0;
    uint32_t event = 0;
    while (digits < eventNum.size() && eventNum[digits] >= '0' && eventNum[digits] <= '9')
    {
        event = event * 10 + static_cast<uint32_t>(eventNum[digits] - '0');
        digits++;
    }
    if (digits == 0)
        return false;

    outInfo.eventNum = eventNum;
    outInfo.eventOrder = event * 2 + ((eventNum.find('b', digits) != std::string::npos) ? 1 : 0);
    // Hex from 2.3.0. Older game ids have the writer's locale (digit grouping), so keep the digits only.
    outInfo.gameId.clear();
    std::string gameId;
    if (j.count("GameID") && TryGetString(j, "GameID", &gameId))
    {
        for (const char c : gameId)
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
                outInfo.gameId += c;
    }
    return true;
}

bool LoadStateFromHud(const std::string& path, MSBQuickMatchGameState& outState,
                      const std::string& p1Username, const std::string& p2Username)
{
    std::string json_str;
    if (!ReadHUDFile(path, json_str))
        return false;

    INFO_LOG_FMT(COMMON, "Found HUD file: {}", path);
    return LoadStateFromHudJson(json_str, outState, p1Username, p2Username);
}

bool LoadStateFromHudJson(const std::string& json_str, MSBQuickMatchGameState& outState,
                          const std::string& p1Username, const std::string& p2Username,
                          std::optional<uint8_t> p2Port)
{
    picojson::value v;
    const picojson::object* root = ParseHUDObject(json_str, v);
    if (!root)
        return false;

    const picojson::object& j = *root;

    // === P1/P2 to HOME/AWAY MAPPING and VERIFICATION ===
    if (p1Username.empty())
    {
        ERROR_LOG_FMT(COMMON, "Could not find player 1 in netplay data.");
        return false;
    }
    INFO_LOG_FMT(COMMON, "P1 player: {}", p1Username);
    INFO_LOG_FMT(COMMON, "P2 player: {}", p2Username.empty() ? "none (solo)" : p2Username);

    std::string awayPlayer;
    std::string homePlayer;
    if (!TryGetPlayerName(j, "Away Player", &awayPlayer) ||
        !TryGetPlayerName(j, "Home Player", &homePlayer))
    {
        return false;
    }
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

    // Validate the opponent slot (the one P1 doesn't occupy) every time. A CPU opponent is recorded
    // in the HUD as the literal "CPU", while locally it appears as an empty p2Username (Player 2 =
    // "No Player Selected"); treat those as equivalent so a CPU game validates against a CPU game,
    // but still reject a CPU-vs-human mismatch in either direction.
    const std::string hudOpponent = p1IsAway ? homePlayer : awayPlayer;
    const bool hudOpponentIsCpu = (hudOpponent == "CPU" || hudOpponent == "No Player Selected");
    const bool localOpponentIsCpu = p2Username.empty();

    if (hudOpponentIsCpu || localOpponentIsCpu)
    {
        if (hudOpponentIsCpu != localOpponentIsCpu)
        {
            ERROR_LOG_FMT(COMMON,
                "Player mismatch (CPU). P1='{}', Local Opponent='{}', HUD Away='{}', HUD Home='{}'",
                p1Username, p2Username.empty() ? "CPU" : p2Username, awayPlayer, homePlayer);
            return false;
        }
    }
    else if (hudOpponent != p2Username)
    {
        ERROR_LOG_FMT(COMMON, "Player mismatch. P1='{}', Opponent='{}', HUD Away='{}', HUD Home='{}'",
                    p1Username, p2Username, awayPlayer, homePlayer);
        return false;
    }

    // Restrict menu inputs only when a human opponent is present; a CPU game leaves P1 free to
    // control the captain screen.
    menuInputRestrictionEnabled = !localOpponentIsCpu;

    INFO_LOG_FMT(COMMON, "Starting parsing HUD to fill out state");


    MSBQuickMatchGameState state;

    // Record who is playing for the boot code: a CPU opponent means a 1-player
    // boot. Netplay passes the second human's controller port from the pad map;
    // when unset (local Player 2) it defaults to port 2 inside the generator.
    state.isCpuMatch = localOpponentIsCpu ? 1 : 0;
    state.p2Port = p2Port;

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
            const picojson::object* roster = nullptr;
            if (!TryGetObject(j, p1Key, &roster))
                return false;
            if (!roster->count("CharID") || !roster->count("Fielding Position") ||
                !roster->count("Batting Hand") || !roster->count("Fielding Hand") || !roster->count("Superstar"))
            {
                ERROR_LOG_FMT(COMMON, "Roster {} missing required fields.", p1Key);
                return false;
            }
            uint8_t charID = 0;
            uint8_t position = 0;
            if (!TryGetInt(*roster, "CharID", &charID) || !TryGetInt(*roster, "Fielding Position", &position))
                return false;
            // draft-slot order: "Away/Home Roster i" is slot i as the game stores it
            state.rosterCharP1BySlot[i] = charID;
            state.positionByRosterSlotP1[i] = position;
            if (position < 9)
            {
                // The generator's batting-order hook writes these into game code without a clamp.
                if (charID > HUD_MAX_CHAR_ID)
                {
                    ERROR_LOG_FMT(COMMON, "Roster {} has invalid CharID {}.", p1Key, charID);
                    return false;
                }
                state.charactersP1ByPosition[position] = charID;
                uint8_t battingHand = 0;
                uint8_t fieldingHand = 0;
                uint8_t superstar = 0;
                if (!TryGetInt(*roster, "Batting Hand", &battingHand) ||
                    !TryGetInt(*roster, "Fielding Hand", &fieldingHand) ||
                    !TryGetInt(*roster, "Superstar", &superstar))
                {
                    return false;
                }
                state.battingHandP1ByPosition[position] = battingHand;
                state.fieldingHandP1ByPosition[position] = fieldingHand;
                state.superstarP1ByPosition[position] = superstar;
            }

            if (roster->count("Captain"))
            {
                double captain = 0;
                if (!TryGetNumber(*roster, "Captain", &captain))
                    return false;
                if (captain == 1)
                {
                    state.captainCharacterP1 = charID;
                    state.captainPositionP1 = position;
                }
            }
        }

        if (j.count(p2Key))
        {
            const picojson::object* roster = nullptr;
            if (!TryGetObject(j, p2Key, &roster))
                return false;
            if (!roster->count("CharID") || !roster->count("Fielding Position") ||
                !roster->count("Batting Hand") || !roster->count("Fielding Hand") || !roster->count("Superstar"))
            {
                ERROR_LOG_FMT(COMMON, "Roster {} missing required fields.", p2Key);
                return false;
            }
            uint8_t charID = 0;
            uint8_t position = 0;
            if (!TryGetInt(*roster, "CharID", &charID) || !TryGetInt(*roster, "Fielding Position", &position))
                return false;
            state.rosterCharP2BySlot[i] = charID;
            state.positionByRosterSlotP2[i] = position;
            if (position < 9)
            {
                // The generator's batting-order hook writes these into game code without a clamp.
                if (charID > HUD_MAX_CHAR_ID)
                {
                    ERROR_LOG_FMT(COMMON, "Roster {} has invalid CharID {}.", p2Key, charID);
                    return false;
                }
                state.charactersP2ByPosition[position] = charID;
                uint8_t battingHand = 0;
                uint8_t fieldingHand = 0;
                uint8_t superstar = 0;
                if (!TryGetInt(*roster, "Batting Hand", &battingHand) ||
                    !TryGetInt(*roster, "Fielding Hand", &fieldingHand) ||
                    !TryGetInt(*roster, "Superstar", &superstar))
                {
                    return false;
                }
                state.battingHandP2ByPosition[position] = battingHand;
                state.fieldingHandP2ByPosition[position] = fieldingHand;
                state.superstarP2ByPosition[position] = superstar;
            }

            if (roster->count("Captain"))
            {
                double captain = 0;
                if (!TryGetNumber(*roster, "Captain", &captain))
                    return false;
                if (captain == 1)
                {
                    state.captainCharacterP2 = charID;
                    state.captainPositionP2 = position;
                }
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
        uint32_t awayLogo = 0;
        uint32_t homeLogo = 0;
        if (!TryGetInt(j, "Away Logo", &awayLogo) || !TryGetInt(j, "Home Logo", &homeLogo))
            return false;
        state.logoP1 = p1IsAway ? awayLogo : homeLogo;
        state.logoP2 = p1IsAway ? homeLogo : awayLogo;
    }
    INFO_LOG_FMT(COMMON, "Logo P1: {}", state.logoP1.has_value() ? std::to_string(state.logoP1.value()) : "not set");
    INFO_LOG_FMT(COMMON, "Logo P2: {}", state.logoP2.has_value() ? std::to_string(state.logoP2.value()) : "not set");

    if (j.count("StadiumID"))
    {
        // need to convert stadium ID in HUD to menu index.
        uint8_t stadiumID = 0;
        if (!TryGetInt(j, "StadiumID", &stadiumID))
            return false;
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

    if (!TryGetOptionalInt(j, "Star Skills On", &state.starSkills))
        return false;
    INFO_LOG_FMT(COMMON, "Star Skills: {}", state.starSkills.has_value() ? std::to_string(state.starSkills.value()) : "not set");

    if (!TryGetOptionalInt(j, "Innings Selected", &state.inningsSelected))
        return false;
    INFO_LOG_FMT(COMMON, "Innings Selected: {}", state.inningsSelected.has_value() ? std::to_string(state.inningsSelected.value()) : "not set");

    if (!TryGetOptionalInt(j, "Mercy On", &state.mercy))
        return false;
    INFO_LOG_FMT(COMMON, "Mercy: {}", state.mercy.has_value() ? std::to_string(state.mercy.value()) : "not set");

    // === IN-GAME STATE ===

    if (!TryGetOptionalInt(j, "Inning", &state.inning))
        return false;
    INFO_LOG_FMT(COMMON, "Inning: {}", state.inning.has_value() ? std::to_string(state.inning.value()) : "not set");

    if (j.count("Half Inning"))
    {
        uint8_t halfInning = 0;
        if (!TryGetInt(j, "Half Inning", &halfInning))
            return false;

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

    if (!TryGetOptionalInt(j, "Away Score", &state.awayScore))
        return false;
    INFO_LOG_FMT(COMMON, "Away Score: {}", state.awayScore.has_value() ? std::to_string(state.awayScore.value()) : "not set");

    if (!TryGetOptionalInt(j, "Home Score", &state.homeScore))
        return false;
    INFO_LOG_FMT(COMMON, "Home Score: {}", state.homeScore.has_value() ? std::to_string(state.homeScore.value()) : "not set");

    if (j.count("Away Inning Scores"))
    {
        const picojson::array* scores = nullptr;
        if (!TryGetArray(j, "Away Inning Scores", &scores))
            return false;
        for (int i = 0; i < static_cast<int>(scores->size()) && i < 18; i++)
        {
            uint16_t score = 0;
            if (!TryGetArrayInt(*scores, i, "Away Inning Scores", &score))
                return false;
            state.awayInningScores[i] = score;
        }
    }

    if (j.count("Home Inning Scores"))
    {
        const picojson::array* scores = nullptr;
        if (!TryGetArray(j, "Home Inning Scores", &scores))
            return false;
        for (int i = 0; i < static_cast<int>(scores->size()) && i < 18; i++)
        {
            uint16_t score = 0;
            if (!TryGetArrayInt(*scores, i, "Home Inning Scores", &score))
                return false;
            state.homeInningScores[i] = score;
        }
    }
    for (int i = 0; i < 18; i++)
    {
        if (state.awayInningScores[i].has_value())
            INFO_LOG_FMT(COMMON, "Away Inning {} Score: {}", i, state.awayInningScores[i].value());
        if (state.homeInningScores[i].has_value())
            INFO_LOG_FMT(COMMON, "Home Inning {} Score: {}", i, state.homeInningScores[i].value());
    }

    if (!TryGetOptionalInt(j, "Strikes", &state.strikes))
        return false;
    INFO_LOG_FMT(COMMON, "Strikes: {}", state.strikes.has_value() ? std::to_string(state.strikes.value()) : "not set");

    if (!TryGetOptionalInt(j, "Balls", &state.balls))
        return false;
    INFO_LOG_FMT(COMMON, "Balls: {}", state.balls.has_value() ? std::to_string(state.balls.value()) : "not set");

    if (!TryGetOptionalInt(j, "Outs", &state.outs))
        return false;
    INFO_LOG_FMT(COMMON, "Outs: {}", state.outs.has_value() ? std::to_string(state.outs.value()) : "not set");

    if (j.count("Away Stars"))
    {
        uint8_t awayStars = 0;
        if (!TryGetInt(j, "Away Stars", &awayStars))
            return false;
        if (p1IsAway)
            state.p1TeamStars = awayStars;
        else
            state.p2TeamStars = awayStars;
    }
    INFO_LOG_FMT(COMMON, "Away Stars assigned to P{}: {}",
        p1IsAway ? "1" : "2",
        p1IsAway ?
            (state.p1TeamStars.has_value() ? std::to_string(state.p1TeamStars.value()) : "not set") :
            (state.p2TeamStars.has_value() ? std::to_string(state.p2TeamStars.value()) : "not set"));

    if (j.count("Home Stars"))
    {
        uint8_t homeStars = 0;
        if (!TryGetInt(j, "Home Stars", &homeStars))
            return false;
        if (p1IsAway)
            state.p2TeamStars = homeStars;
        else
            state.p1TeamStars = homeStars;
    }
    INFO_LOG_FMT(COMMON, "Home Stars assigned to P{}: {}",
        p1IsAway ? "2" : "1",
        p1IsAway ?
            (state.p2TeamStars.has_value() ? std::to_string(state.p2TeamStars.value()) : "not set") :
            (state.p1TeamStars.has_value() ? std::to_string(state.p1TeamStars.value()) : "not set"));

    if (!TryGetOptionalInt(j, "Star Chance", &state.isStarChance))
        return false;
    INFO_LOG_FMT(COMMON, "Star Chance: {}", state.isStarChance.has_value() ? std::to_string(state.isStarChance.value()) : "not set");

    // === POSITIONS BY BATTING ORDER ===
    if (j.count("Away Batter Roster Loc") && j.count("Home Batter Roster Loc"))
    {
        // int16_t so the "i + startingBatter" below can't overflow.
        int16_t awayStartingBatter = 0;
        int16_t homeStartingBatter = 0;
        if (!TryGetInt(j, "Away Batter Roster Loc", &awayStartingBatter) ||
            !TryGetInt(j, "Home Batter Roster Loc", &homeStartingBatter))
        {
            return false;
        }

        // Keep the current batter's natural-order slot so the generator can
        // un-rotate the batting order and set the current batter correctly.
        state.awayBatterRosterLoc = static_cast<uint8_t>(((awayStartingBatter % 9) + 9) % 9);
        state.homeBatterRosterLoc = static_cast<uint8_t>(((homeStartingBatter % 9) + 9) % 9);

        for (int i = 0; i < 9; i++)
        {
            std::string awayKey = "Away Roster " + std::to_string((i + awayStartingBatter) % 9);
            std::string homeKey = "Home Roster " + std::to_string((i + homeStartingBatter) % 9);

            if (j.count(awayKey))
            {
                const picojson::object* roster = nullptr;
                uint32_t position = 0;
                if (!TryGetObject(j, awayKey, &roster) || !TryGetInt(*roster, "Fielding Position", &position))
                    return false;
                if (position < 9)
                    state.awayPositionByBattingOrder[i] = position;
            }

            if (j.count(homeKey))
            {
                const picojson::object* roster = nullptr;
                uint32_t position = 0;
                if (!TryGetObject(j, homeKey, &roster) || !TryGetInt(*roster, "Fielding Position", &position))
                    return false;
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
            const picojson::object* runner = nullptr;
            if (!TryGetObject(j, key, &runner))
                return false;

            // Absolute natural batting slot of the runner (0 = leadoff), which
            // is the index into the batting team's in-memory roster the boot
            // code writes directly.
            if (!TryGetOptionalInt(*runner, "Runner Roster Loc", &state.runnerRosterSpot[i]))
                return false;

            if (!TryGetOptionalInt(*runner, "Runner Char Id", &state.runnerCharacterID[i]))
                return false;
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
    // Stamina is stored per character in "Defensive Stats", indexed by the HUD's roster slot (the
    // game's own index for it). Keep that index; the generator maps it onto the booted order.
    for (int i = 0; i < 9; i++)
    {
        std::string p1Key = (p1IsAway ? "Away Roster " : "Home Roster ") + std::to_string(i);
        std::string p2Key = (p1IsAway ? "Home Roster " : "Away Roster ") + std::to_string(i);

        if (j.count(p1Key))
        {
            const picojson::object* roster = nullptr;
            if (!TryGetObject(j, p1Key, &roster))
                return false;
            if (roster->count("Defensive Stats"))
            {
                const picojson::object* defensiveStats = nullptr;
                if (!TryGetObject(*roster, "Defensive Stats", &defensiveStats))
                    return false;
                if (!TryGetOptionalInt(*defensiveStats, "Stamina", &state.pitcherStaminaP1[i]))
                    return false;
            }
        }

        if (j.count(p2Key))
        {
            const picojson::object* roster = nullptr;
            if (!TryGetObject(j, p2Key, &roster))
                return false;
            if (roster->count("Defensive Stats"))
            {
                const picojson::object* defensiveStats = nullptr;
                if (!TryGetObject(*roster, "Defensive Stats", &defensiveStats))
                    return false;
                if (!TryGetOptionalInt(*defensiveStats, "Stamina", &state.pitcherStaminaP2[i]))
                    return false;
            }
        }
    }
    // The HUD's situation (count, score, inning, "Pitcher Stamina") is captured when the event
    // starts, before the pitch, but "Defensive Stats" are read when the HUD is written, which for a
    // post-play ("b") HUD is after the play has already drained stamina (runs scored, star pitch).
    // Only the current pitcher's stamina changes during a play, so put that slot back to the
    // pre-play value so it matches the situation the reset restores. (Same fix as PR #150 for the
    // old menu-walk generator, indexed here by HUD roster slot.)
    if (j.count("Pitcher Stamina") && j.count("Pitcher Roster Loc") && state.halfInning.has_value())
    {
        const bool homeIsPitching = (state.halfInning.value() == 0);
        const bool p1IsPitching = (homeIsPitching != p1IsAway);
        int pitcherSlot = 0;
        uint16_t prePlayStamina = 0;
        if (!TryGetInt(j, "Pitcher Roster Loc", &pitcherSlot) || !TryGetInt(j, "Pitcher Stamina", &prePlayStamina))
            return false;
        std::optional<uint16_t>* staminaBySlot = p1IsPitching ? state.pitcherStaminaP1 : state.pitcherStaminaP2;
        if (pitcherSlot >= 0 && pitcherSlot < 9)
        {
            staminaBySlot[pitcherSlot] = prePlayStamina;
            INFO_LOG_FMT(COMMON, "P{} pitcher (roster slot {}) set to pre-play stamina {}",
                         p1IsPitching ? 1 : 2, pitcherSlot, prePlayStamina);
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
                     const std::string& p1Username, const std::string& p2Username,
                     bool isNetplay,
                     HUDValidationDetails* outDetails)
{
    std::string json_str;
    if (!ReadHUDFile(path, json_str))
        return 2;

    return allowLoadFromHUDJson(json_str, p1Username, p2Username, isNetplay, outDetails);
}

int allowLoadFromHUDJson(const std::string& json_str,
                         const std::string& p1Username, const std::string& p2Username,
                         bool isNetplay,
                         HUDValidationDetails* outDetails)
{
    INFO_LOG_FMT(COMMON, "Starting to check if HUD state load is allowed");

    // ===== check HUD can be parsed. =====
    picojson::value v;
    const picojson::object* root = ParseHUDObject(json_str, v);
    if (!root)
        return 2;

    const picojson::object& j = *root;

    INFO_LOG_FMT(COMMON, "File check passed, checking gamemode.");

    // ===== check lobby gamemode matches HUD =====
    // Get the active tagset from the current netplay session
    std::optional<Tag::TagSet> activeTagSet = Core::GetActiveTagSet(isNetplay);

    // Read the HUD's TagSetID. The HUD writer always emits this field, using -1
    // as the sentinel for "no game mode", so check the value rather than key presence.
    int hudTagSetId = -1;
    if (j.count("TagSetID") && !TryGetInt(j, "TagSetID", &hudTagSetId))
        return 2;
    bool hudHasTagSet = (hudTagSetId != -1);

    if (hudHasTagSet)
    {
        if (activeTagSet.has_value())
        {
            // Both HUD and lobby have a tagset - they must match
            if (hudTagSetId != activeTagSet.value().id)
            {
                ERROR_LOG_FMT(COMMON, "TagSet mismatch. Lobby TagSet ID={}, HUD TagSet ID={}",
                            activeTagSet.value().id, hudTagSetId);
                if (outDetails)
                {
                    outDetails->hudTagSetId = hudTagSetId;
                    outDetails->activeTagSetName = activeTagSet.value().name;
                }
                return 3;
            }
            INFO_LOG_FMT(COMMON, "TagSet match confirmed: ID={}", hudTagSetId);
        }
        else
        {
            // HUD has a tagset but nothing is active — always a mismatch.
            ERROR_LOG_FMT(COMMON, "HUD has TagSet ID={} but no game mode is active.", hudTagSetId);
            if (outDetails)
            {
                outDetails->hudTagSetId = hudTagSetId;
                outDetails->activeTagSetName = "";
            }
            return 3;
        }
    }
    else if (activeTagSet.has_value())
    {
        // Lobby has a tagset but HUD does not record one
        ERROR_LOG_FMT(COMMON, "Lobby has TagSet ID={} but HUD has no game mode.",
                    activeTagSet.value().id);
        if (outDetails)
        {
            outDetails->hudTagSetId = -1;
            outDetails->activeTagSetName = activeTagSet.value().name;
        }
        return 3;
    }
    // else: neither HUD nor lobby has a game mode — no mismatch.

    INFO_LOG_FMT(COMMON, "Gamemode check passed, checking players.");

    // ===== check players match HUD =====
    if (p1Username.empty())
    {
        ERROR_LOG_FMT(COMMON, "Could not find player 1 in netplay data.");
        return 4;
    }
    INFO_LOG_FMT(COMMON, "P1 player: {}", p1Username);
    INFO_LOG_FMT(COMMON, "P2 player: {}", p2Username.empty() ? "none (solo)" : p2Username);

    std::string awayPlayer;
    std::string homePlayer;
    if (!TryGetPlayerName(j, "Away Player", &awayPlayer) ||
        !TryGetPlayerName(j, "Home Player", &homePlayer))
    {
        return 2;
    }

    if (outDetails)
    {
        outDetails->hudAwayPlayer = awayPlayer;
        outDetails->hudHomePlayer = homePlayer;
    }

    // Validate players match the HUD file
    bool p1IsAway = (p1Username == awayPlayer);
    bool p1IsHome = (p1Username == homePlayer);

    if (!p1IsAway && !p1IsHome)
    {
        ERROR_LOG_FMT(COMMON, "P1 player '{}' not found in HUD file. Away='{}', Home='{}'",
                    p1Username, awayPlayer, homePlayer);
        return 4;
    }

    // Validate the opponent slot (the one P1 doesn't occupy) every time. A CPU opponent is recorded
    // in the HUD as the literal "CPU", while locally it appears as an empty p2Username (Player 2 =
    // "No Player Selected"); treat those as equivalent so a CPU game validates against a CPU game,
    // but still reject a CPU-vs-human mismatch in either direction.
    const std::string hudOpponent = p1IsAway ? homePlayer : awayPlayer;
    const bool hudOpponentIsCpu = (hudOpponent == "CPU" || hudOpponent == "No Player Selected");
    const bool localOpponentIsCpu = p2Username.empty();

    if (hudOpponentIsCpu || localOpponentIsCpu)
    {
        if (hudOpponentIsCpu != localOpponentIsCpu)
        {
            ERROR_LOG_FMT(COMMON,
                "Player mismatch (CPU). P1='{}', Local Opponent='{}', HUD Away='{}', HUD Home='{}'",
                p1Username, p2Username.empty() ? "CPU" : p2Username, awayPlayer, homePlayer);
            return 4;
        }
    }
    else if (hudOpponent != p2Username)
    {
        ERROR_LOG_FMT(COMMON, "Player mismatch. P1='{}', Opponent='{}', HUD Away='{}', HUD Home='{}'",
                    p1Username, p2Username, awayPlayer, homePlayer);
        return 4;
    }

    // Netplay reports its HUD as valid off this result and the server relays the one it picks, so
    // a HUD that can't load has to fail here or it would beat the other player's valid HUD.
    MSBQuickMatchGameState unusedState;
    if (!LoadStateFromHudJson(json_str, unusedState, p1Username, p2Username))
        return 2;

    INFO_LOG_FMT(COMMON, "All checks passed. HUD can load.");

    return 0;
}