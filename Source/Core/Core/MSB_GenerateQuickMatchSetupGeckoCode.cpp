#include "MSB_GenerateQuickMatchSetupGeckoCode.h"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <optional>
#include <algorithm>
#include <cstdint>
#include "Common/Logging/Log.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/MSB_StatTracker.h"

bool menuInputRestrictionEnabled = true; // default to on, but for some debugging contexts can be set to false.

static Gecko::GeckoCode::Code ToGeckoCode(uint8_t geckoType, uint32_t address, uint32_t value)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());

    uint32_t firstWord = (static_cast<uint32_t>(geckoType) << 24) | (address & 0x00FFFFFF);

    oss << std::uppercase << std::hex << std::setfill('0')
        << std::setw(8) << firstWord
        << " "
        << std::setw(8) << value;

    Gecko::GeckoCode::Code code;
    code.address = firstWord;
    code.data = value;
    code.original_line = oss.str();

    INFO_LOG_FMT(COMMON, "Gecko code produced: {}", oss.str());
    return code;
}

static Gecko::GeckoCode::Code EndConditional()
{
    Gecko::GeckoCode::Code code;
    code.address = 0xE0000000;
    code.data    = 0x80008000;
    code.original_line = "E0000000 80008000";
    return code;
}

static Gecko::GeckoCode::Code CustomGeckoCode(
    uint32_t firstWord,
    uint32_t secondWord
)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());

    oss << std::uppercase << std::hex << std::setfill('0')
    << std::setw(8) << firstWord
    << " "
    << std::setw(8) << secondWord;

    Gecko::GeckoCode::Code code;
    code.address = firstWord;
    code.data    = secondWord;
    code.original_line = oss.str();

    INFO_LOG_FMT(COMMON, "Gecko code produced: {}", oss.str());
    return code;
}

bool MSB_Player::Validate(const std::string& context) const
{
    bool valid = true;
    const std::string p = context.empty() ? "" : context + ": ";

    if (charID.has_value() && charID.value() > 0x35)
    { ERROR_LOG_FMT(COMMON, "{}charID {} exceeds max (0x35=53)", p, charID.value()); valid = false; }
    if (position.has_value() && position.value() > 8)
    { ERROR_LOG_FMT(COMMON, "{}position {} out of range (0-8)", p, position.value()); valid = false; }
    if (battingOrderSlot.has_value() && battingOrderSlot.value() > 8)
    { ERROR_LOG_FMT(COMMON, "{}battingOrderSlot {} out of range (0-8)", p, battingOrderSlot.value()); valid = false; }
    if (battingHand.has_value() && battingHand.value() > 1)
    { ERROR_LOG_FMT(COMMON, "{}battingHand {} must be 0 (right) or 1 (left)", p, battingHand.value()); valid = false; }
    if (fieldingHand.has_value() && fieldingHand.value() > 1)
    { ERROR_LOG_FMT(COMMON, "{}fieldingHand {} must be 0 (right) or 1 (left)", p, fieldingHand.value()); valid = false; }
    if (superstar.has_value() && superstar.value() > 1)
    { ERROR_LOG_FMT(COMMON, "{}superstar {} must be 0 (off) or 1 (on)", p, superstar.value()); valid = false; }

    return valid;
}

MSB_Team::MSB_Team(std::array<MSB_Player, 9> inPlayers)
    : players(std::move(inPlayers))
{
    for (uint8_t i = 0; i < 9; i++)
    {
        if (players[i].IsSet())
            players[i].position = i;
    }
}

bool MSB_Team::SetPlayer(uint8_t pos, const MSB_Player& player)
{
    if (pos >= 9)
    {
        WARN_LOG_FMT(COMMON, "SetPlayer: position {} out of range (0-8)", pos);
        return false;
    }
    players[pos] = player;
    players[pos].position = pos;
    return true;
}

const MSB_Player* MSB_Team::GetPlayer(uint8_t pos) const
{
    if (pos >= 9)
        return nullptr;
    return &players[pos];
}

const MSB_Player* MSB_Team::GetPlayerByBattingSlot(uint8_t slot) const
{
    for (const MSB_Player& p : players)
    {
        if (p.battingOrderSlot.has_value() && p.battingOrderSlot.value() == slot)
            return &p;
    }
    return nullptr;
}

const MSB_Player* MSB_Team::GetCaptain() const
{
    if (!captainBattingSlot.has_value())
        return nullptr;
    return GetPlayerByBattingSlot(captainBattingSlot.value());
}

bool MSB_Team::IsFull() const
{
    for (const MSB_Player& p : players)
    {
        if (!p.IsSet())
            return false;
    }
    return true;
}

bool MSB_Team::SetCaptainBattingSlot(uint8_t slot)
{
    if (slot > 8)
    {
        WARN_LOG_FMT(COMMON, "SetCaptainBattingSlot: {} out of range (0-8)", slot);
        return false;
    }
    captainBattingSlot = slot;
    return true;
}

bool MSB_Team::SetLogo(uint32_t val)
{
    if (val > 0x2F)
    {
        WARN_LOG_FMT(COMMON, "SetLogo: {} exceeds max (0x2F=47)", val);
        return false;
    }
    logo = val;
    return true;
}

bool MSB_Team::SetTeamStars(uint8_t val)
{
    if (val > 5)
    {
        WARN_LOG_FMT(COMMON, "SetTeamStars: {} exceeds max (5)", val);
        return false;
    }
    teamStars = val;
    return true;
}

bool MSB_Team::Validate(const std::string& context) const
{
    bool valid = true;
    const std::string p = context.empty() ? "" : context + ": ";

    for (int i = 0; i < 9; i++)
    {
        if (players[i].IsSet())
            valid &= players[i].Validate(p + "pos" + std::to_string(i));
    }

    bool slotUsed[9] = {};
    for (int i = 0; i < 9; i++)
    {
        if (!players[i].battingOrderSlot.has_value())
            continue;
        uint8_t slot = players[i].battingOrderSlot.value();
        if (slot > 8)
            continue; // already caught by player validate
        if (slotUsed[slot])
        { ERROR_LOG_FMT(COMMON, "{}duplicate battingOrderSlot {}", p, slot); valid = false; }
        slotUsed[slot] = true;
    }

    if (captainBattingSlot.has_value())
    {
        if (captainBattingSlot.value() > 8)
        { ERROR_LOG_FMT(COMMON, "{}captainBattingSlot {} out of range (0-8)", p, captainBattingSlot.value()); valid = false; }
        else if (GetCaptain() == nullptr)
        { ERROR_LOG_FMT(COMMON, "{}captainBattingSlot {} does not match any player's battingOrderSlot", p, captainBattingSlot.value()); valid = false; }
    }

    if (logo.has_value() && logo.value() > 0x2F)
    { ERROR_LOG_FMT(COMMON, "{}logo {} exceeds max (0x2F=47)", p, logo.value()); valid = false; }
    if (teamStars.has_value() && teamStars.value() > 5)
    { ERROR_LOG_FMT(COMMON, "{}teamStars {} exceeds max (5)", p, teamStars.value()); valid = false; }

    return valid;
}

MSB_QuickMatchState::MSB_QuickMatchState(MSB_Team inP1, MSB_Team inP2, bool inP1IsAway)
    : p1(std::move(inP1)), p2(std::move(inP2)), p1IsAway(inP1IsAway)
{}

bool MSB_QuickMatchState::SetStadium(uint8_t val)
{
    if (val > 5) { WARN_LOG_FMT(COMMON, "SetStadium: {} out of range (0-5); Toy Field unsupported", val); return false; }
    stadium = val; return true;
}

bool MSB_QuickMatchState::SetFirstBatter(uint8_t val)
{
    if (val > 1) { WARN_LOG_FMT(COMMON, "SetFirstBatter: {} must be 0 (P1) or 1 (P2)", val); return false; }
    firstBatter = val; return true;
}

bool MSB_QuickMatchState::SetStarSkills(uint8_t val)
{
    if (val > 1) { WARN_LOG_FMT(COMMON, "SetStarSkills: {} must be 0 (off) or 1 (on)", val); return false; }
    starSkills = val; return true;
}

bool MSB_QuickMatchState::SetInningsSelected(uint8_t val)
{
    if (val == 0) { WARN_LOG_FMT(COMMON, "SetInningsSelected: must be at least 1"); return false; }
    if (val % 2 == 0)
        WARN_LOG_FMT(COMMON, "SetInningsSelected: {} is even; result will be {}", val, val - 1);
    inningsSelected = val; return true;
}

bool MSB_QuickMatchState::SetMercy(uint8_t val)
{
    if (val > 1) { WARN_LOG_FMT(COMMON, "SetMercy: {} must be 0 (off) or 1 (on)", val); return false; }
    mercy = val; return true;
}

bool MSB_QuickMatchState::SetInning(uint32_t val)
{
    if (val > 18) { WARN_LOG_FMT(COMMON, "SetInning: {} exceeds max (18)", val); return false; }
    inning = val; return true;
}

bool MSB_QuickMatchState::SetHalfInning(uint8_t val)
{
    if (val > 1) { WARN_LOG_FMT(COMMON, "SetHalfInning: {} must be 0 (top) or 1 (bottom)", val); return false; }
    halfInning = val; return true;
}

bool MSB_QuickMatchState::SetBattingTeam(uint32_t val)
{
    if (val > 1) { WARN_LOG_FMT(COMMON, "SetBattingTeam: {} must be 0 or 1", val); return false; }
    battingTeam = val; return true;
}

bool MSB_QuickMatchState::SetFieldingTeam(uint32_t val)
{
    if (val > 1) { WARN_LOG_FMT(COMMON, "SetFieldingTeam: {} must be 0 or 1", val); return false; }
    fieldingTeam = val; return true;
}

bool MSB_QuickMatchState::SetHomeScore(uint16_t val)
{
    homeScore = val; return true;
}

bool MSB_QuickMatchState::SetAwayScore(uint16_t val)
{
    awayScore = val; return true;
}

bool MSB_QuickMatchState::SetHomeInningScore(int i, uint16_t val)
{
    if (i < 0 || i >= 18) { WARN_LOG_FMT(COMMON, "SetHomeInningScore: index {} out of range (0-17)", i); return false; }
    homeInningScores[i] = val; return true;
}

bool MSB_QuickMatchState::SetAwayInningScore(int i, uint16_t val)
{
    if (i < 0 || i >= 18) { WARN_LOG_FMT(COMMON, "SetAwayInningScore: index {} out of range (0-17)", i); return false; }
    awayInningScores[i] = val; return true;
}

bool MSB_QuickMatchState::SetStrikes(uint32_t val)
{
    if (val > 2) { WARN_LOG_FMT(COMMON, "SetStrikes: {} out of range (0-2)", val); return false; }
    strikes = val; return true;
}

bool MSB_QuickMatchState::SetBalls(uint32_t val)
{
    if (val > 3) { WARN_LOG_FMT(COMMON, "SetBalls: {} out of range (0-3)", val); return false; }
    balls = val; return true;
}

bool MSB_QuickMatchState::SetOuts(uint32_t val)
{
    if (val > 2) { WARN_LOG_FMT(COMMON, "SetOuts: {} out of range (0-2)", val); return false; }
    outs = val; return true;
}

bool MSB_QuickMatchState::SetIsStarChance(uint8_t val)
{
    if (val > 1) { WARN_LOG_FMT(COMMON, "SetIsStarChance: {} must be 0 (off) or 1 (on)", val); return false; }
    isStarChance = val; return true;
}

bool MSB_QuickMatchState::SetRunner(int base, uint8_t battingSlot, std::optional<bool> useP1)
{
    if (base < 0 || base > 2)
    { WARN_LOG_FMT(COMMON, "SetRunner: base {} out of range (0=1B,1=2B,2=3B)", base); return false; }
    if (battingSlot > 8)
    { WARN_LOG_FMT(COMMON, "SetRunner: battingSlot {} out of range (0-8)", battingSlot); return false; }

    bool teamIsP1;
    if (useP1.has_value())
    {
        teamIsP1 = useP1.value();
    }
    else if (halfInning.has_value())
    {
        bool awayIsBatting = (halfInning.value() == 0);
        teamIsP1 = (awayIsBatting == p1IsAway);
    }
    else
    {
        WARN_LOG_FMT(COMMON, "SetRunner: halfInning not set and useP1 not specified; cannot determine batting team");
        return false;
    }

    const MSB_Team& team = teamIsP1 ? p1 : p2;
    const MSB_Player* player = team.GetPlayerByBattingSlot(battingSlot);
    if (player == nullptr || !player->charID.has_value() || !player->position.has_value())
    {
        WARN_LOG_FMT(COMMON, "SetRunner: no complete player found at battingSlot {} in {}", battingSlot, teamIsP1 ? "P1" : "P2");
        return false;
    }

    runnerRosterSpot[base]   = player->position.value();
    runnerCharacterID[base]  = player->charID.value();
    return true;
}

bool MSB_QuickMatchState::Validate() const
{
    bool valid = true;

    valid &= p1.Validate("P1");
    valid &= p2.Validate("P2");

    if (stadium.has_value() && stadium.value() > 5)
    { ERROR_LOG_FMT(COMMON, "stadium {} out of range (0-5); Toy Field unsupported", stadium.value()); valid = false; }
    if (firstBatter.has_value() && firstBatter.value() > 1)
    { ERROR_LOG_FMT(COMMON, "firstBatter {} must be 0 (P1) or 1 (P2)", firstBatter.value()); valid = false; }
    if (starSkills.has_value() && starSkills.value() > 1)
    { ERROR_LOG_FMT(COMMON, "starSkills {} must be 0 (off) or 1 (on)", starSkills.value()); valid = false; }
    if (inningsSelected.has_value())
    {
        if (inningsSelected.value() == 0)
        { ERROR_LOG_FMT(COMMON, "inningsSelected must be at least 1"); valid = false; }
        else if (inningsSelected.value() % 2 == 0)
            WARN_LOG_FMT(COMMON, "inningsSelected {} is even; result will be {}", inningsSelected.value(), inningsSelected.value() - 1);
    }
    if (mercy.has_value() && mercy.value() > 1)
    { ERROR_LOG_FMT(COMMON, "mercy {} must be 0 (off) or 1 (on)", mercy.value()); valid = false; }

    if (inning.has_value() && inning.value() > 18)
    { ERROR_LOG_FMT(COMMON, "inning {} exceeds max (18)", inning.value()); valid = false; }
    if (halfInning.has_value() && halfInning.value() > 1)
    { ERROR_LOG_FMT(COMMON, "halfInning {} must be 0 (top) or 1 (bottom)", halfInning.value()); valid = false; }
    if (battingTeam.has_value() && battingTeam.value() > 1)
    { ERROR_LOG_FMT(COMMON, "battingTeam {} must be 0 or 1", battingTeam.value()); valid = false; }
    if (fieldingTeam.has_value() && fieldingTeam.value() > 1)
    { ERROR_LOG_FMT(COMMON, "fieldingTeam {} must be 0 or 1", fieldingTeam.value()); valid = false; }

    if (strikes.has_value() && strikes.value() > 2)
    { ERROR_LOG_FMT(COMMON, "strikes {} out of range (0-2)", strikes.value()); valid = false; }
    if (balls.has_value() && balls.value() > 3)
    { ERROR_LOG_FMT(COMMON, "balls {} out of range (0-3)", balls.value()); valid = false; }
    if (outs.has_value() && outs.value() > 2)
    { ERROR_LOG_FMT(COMMON, "outs {} out of range (0-2)", outs.value()); valid = false; }
    if (isStarChance.has_value() && isStarChance.value() > 1)
    { ERROR_LOG_FMT(COMMON, "isStarChance {} must be 0 (off) or 1 (on)", isStarChance.value()); valid = false; }

    for (int i = 0; i < 3; i++)
    {
        if (runnerRosterSpot[i].has_value() && runnerRosterSpot[i].value() > 8)
        { ERROR_LOG_FMT(COMMON, "runnerRosterSpot[{}] {} out of range (0-8)", i, runnerRosterSpot[i].value()); valid = false; }
        if (runnerCharacterID[i].has_value() && runnerCharacterID[i].value() > 0x35)
        { ERROR_LOG_FMT(COMMON, "runnerCharacterID[{}] {} exceeds max (0x35=53)", i, runnerCharacterID[i].value()); valid = false; }
    }

    return valid;
}

void GenerateRosterGeckoCodes(
    const MSB_Team& team,
    uint32_t rosterBaseAddress,
    uint32_t stride,
    uint32_t spotFilledAddress,
    uint32_t okButtonActiveAddress,
    uint32_t cursorLocationAddress,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GenerateRosterGeckoCodes function");

    bool rosterProvided = false;
    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p = team.GetPlayer(static_cast<uint8_t>(i));
        if (p && p->IsSet()) { rosterProvided = true; break; }
    }

    if (rosterProvided)
    {
        // set spot filled indicators
        outCodes.push_back(ToGeckoCode(0x00, spotFilledAddress, 0x00080001));

        // make OK button selectable
        outCodes.push_back(ToGeckoCode(0x00, okButtonActiveAddress, 0x01));

        // put cursor on OK button
        outCodes.push_back(ToGeckoCode(0x04, cursorLocationAddress, 0x09));

        // fill rosters with character IDs
        for (int i = 0; i < 9; i++)
        {
            const MSB_Player* p = team.GetPlayer(static_cast<uint8_t>(i));
            if (p && p->charID.has_value())
                outCodes.push_back(ToGeckoCode(0x00, rosterBaseAddress + i * stride, p->charID.value()));
            else
                ERROR_LOG_FMT(COMMON, "Missing character at position {} address {:#010x}.", i, rosterBaseAddress + i * stride);
        }
    }
}

// score helper function to manage cases related to providing current and/or inning scores.
void GenerateTeamScoreGeckoCodes(
    const std::optional<uint16_t>& currentScore,
    const std::optional<uint16_t> inningScores[18],
    uint32_t currentScoreAddress,
    uint32_t inningScoresBaseAddress,
    uint32_t stride,  // default 2 bytes between innings
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GenerateTeamScoreGeckoCodes function");

    // Determine if any inning scores are provided
    bool inningScoresProvided = false;
    uint16_t inningScoresSum = 0;
    for (int i = 0; i < 18; ++i)
    {
        const auto& scoreOpt = inningScores[i];
        if (scoreOpt.has_value())
        {
            inningScoresProvided = true;
            inningScoresSum += scoreOpt.value();
        }
    }

    bool totalScoreProvided = currentScore.has_value();
    uint16_t totalScore = totalScoreProvided ? currentScore.value() : 0;

    // Case: both total score and inning scores provided
    if (totalScoreProvided && inningScoresProvided)
    {
        // Validate total vs sum
        if (totalScore != inningScoresSum)
        {
            // mismatch → set total to 99 to show error
            ERROR_LOG_FMT(COMMON, "Score mismatch: {} vs sum {}", totalScore, inningScoresSum);
            totalScore = 99;
        }

        // Generate total score Gecko code
        outCodes.push_back(ToGeckoCode(0x02, currentScoreAddress, totalScore));

        // Generate per-inning Gecko codes
        for (int i = 0; i < 18; ++i)
        {
            const auto& scoreOpt = inningScores[i];
            if (scoreOpt.has_value())
            {
                uint16_t val = scoreOpt.value();
                outCodes.push_back(ToGeckoCode(0x02, inningScoresBaseAddress + i*stride, val));
            }
        }
    }
    // Case: only total score provided
    else if (totalScoreProvided)
    {
        // Set first inning to total score
        outCodes.push_back(ToGeckoCode(0x02, inningScoresBaseAddress, totalScore));
        // Generate total score Gecko code
        outCodes.push_back(ToGeckoCode(0x02, currentScoreAddress, totalScore));
    }
    // Case: only inning scores provided
    else if (inningScoresProvided)
    {
        // Sum the innings for total score
        totalScore = inningScoresSum;

        // Generate total score Gecko code
        outCodes.push_back(ToGeckoCode(0x02, currentScoreAddress, totalScore));

        // Generate per-inning Gecko codes
        for (int i = 0; i < 18; ++i)
        {
            const auto& scoreOpt = inningScores[i];
            if (scoreOpt.has_value())
            {
                uint16_t val = scoreOpt.value();
                outCodes.push_back(ToGeckoCode(0x02, inningScoresBaseAddress + i*stride, val));
            }
        }
    }
    // Case: neither provided → do nothing
    else
    {
        // no Gecko codes generated
    }
}

void GenerateOrderAndPositionGeckoCodes(
    const MSB_Team& team,
    uint32_t structBase,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GenerateOrderAndPositionGeckoCodes function");

    bool positionsProvided = false;
    for (int i = 0; i < 9; i++)
    {
        if (team.GetPlayerByBattingSlot(static_cast<uint8_t>(i))) { positionsProvided = true; break; }
    }

    if (positionsProvided)
    {
        for (int i = 0; i < 9; i++)
        {
            const MSB_Player* p = team.GetPlayerByBattingSlot(static_cast<uint8_t>(i));
            if (!p || !p->position.has_value()) continue;

            uint32_t position = p->position.value();

            // i + 1 since first index is a copy of the pitchers spot in the order + position.
            uint32_t address =
                structBase +
                MSBQuickMatchCodeBuilder::ORDER_AND_POSITION_STRUCT_CHARACTER_STRIDE * (i + 1) +
                MSBQuickMatchCodeBuilder::ORDER_AND_POSITION_STRUCT_POSITION_STRIDE;

            outCodes.push_back(ToGeckoCode(0x04, address, position));

            // if pitcher, fill in their spot in the batting order
            if (position == 0)
            {
                outCodes.push_back(ToGeckoCode(0x04, structBase, static_cast<uint32_t>(i)));
                outCodes.push_back(ToGeckoCode(0x04, structBase + MSBQuickMatchCodeBuilder::ORDER_AND_POSITION_STRUCT_POSITION_STRIDE, 0x00000000));
            }
        }
    }
}

void GeneratePitcherStaminaGeckoCodes(
    const MSB_Team& team,
    uint32_t baseAddr,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GeneratePitcherStaminaGeckoCodes function");

    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p = team.GetPlayerByBattingSlot(static_cast<uint8_t>(i));
        if (p && p->pitcherStamina.has_value())
        {
            uint32_t address = baseAddr + MSBQuickMatchCodeBuilder::PITCHER_STAMINA_STRIDE * i;
            outCodes.push_back(ToGeckoCode(0x02, address, p->pitcherStamina.value()));
        }
    }
}

void GenerateBattingOrderGeckoCodes(
    const MSB_QuickMatchState& state,
    const bool p1IsAway,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GenerateBattingOrderGeckoCodes function");

    const MSB_Team& p1Team = state.GetP1();
    const MSB_Team& p2Team = state.GetP2();

    // add header
    outCodes.push_back(CustomGeckoCode(0xC2066A48, 0x00000016));
    // check if team equals P2 (1)
    outCodes.push_back(CustomGeckoCode(0x3AE10038, 0x2C080001));
    // branch to P2 code. Nop for alignment.
    outCodes.push_back(CustomGeckoCode(0x41820058, MSBQuickMatchCodeBuilder::NOP_INSTR));

    // P1 batting order
    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p = p1Team.GetPlayerByBattingSlot(static_cast<uint8_t>(i));
        uint8_t charID = p->charID.value();

        uint32_t firstWord = 0x39800000 | (charID & 0xFF);
        uint32_t secondWord = 0x99970000 | (i & 0xFF);

        outCodes.push_back(CustomGeckoCode(firstWord, secondWord));
    }

    // Put branch instruction to end. Nop for alignment.
    outCodes.push_back(CustomGeckoCode(0x48000050, MSBQuickMatchCodeBuilder::NOP_INSTR));

    // P2 batting order.
    // CharID and batting order are now on separate lines since there is an intermediate branch instruction.
    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p = p2Team.GetPlayerByBattingSlot(static_cast<uint8_t>(i));
        uint8_t charID = p->charID.value();

        uint32_t firstWord = 0x39800000 | (charID & 0xFF);
        uint32_t secondWord = 0x99970000 | (i & 0xFF);

        outCodes.push_back(CustomGeckoCode(firstWord, secondWord));
    }

    // finish the code
    outCodes.push_back(CustomGeckoCode(MSBQuickMatchCodeBuilder::NOP_INSTR, 0x00000000));
}

void GenerateHandednessGeckoCodes(
    const MSB_QuickMatchState& state,
    const bool p1IsAway,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GenerateHandednessGeckoCodes function");

   // Add C2 gecko code header
    outCodes.push_back(CustomGeckoCode(0xC2047E2C, 0x00000038));

    // load base address for handedness in r4 - so team 0 roster 0
    outCodes.push_back(CustomGeckoCode(0x3C808035, 0x38843C06));

    for (int team = 0; team < 2; team++)
    {
        const MSB_Team& t = (team == 0) ? state.GetP1() : state.GetP2();

        for (int rosterSpot = 0; rosterSpot < 9; rosterSpot++)
        {
            const MSB_Player* p = t.GetPlayerByBattingSlot(static_cast<uint8_t>(rosterSpot));

            uint8_t fieldingHand = p->fieldingHand.value();
            uint8_t battingHand  = p->battingHand.value();

            // load handedness into registers r3 and r6.
            uint32_t fieldingInt = 0x38600000 | (fieldingHand & 0xFF);
            uint32_t battingInt  = 0x38C00000 | (battingHand & 0xFF);
            outCodes.push_back(CustomGeckoCode(fieldingInt, battingInt));

            // store handedness into roster addresses
            outCodes.push_back(CustomGeckoCode(0x98640000, 0x98C40001));

            // go to next roster spot by adding stride to base address in r4. Also add nop for allignment.
            outCodes.push_back(CustomGeckoCode(0x388400A0, MSBQuickMatchCodeBuilder::NOP_INSTR));
        }
    }

    // finish the code by replacing last instruction
    outCodes.push_back(CustomGeckoCode(0x3C808033, 0x00000000));
}

void GenerateSuperstarGeckoCodes(
    const MSB_QuickMatchState& state,
    const bool p1IsAway,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GenerateSuperstarGeckoCodes function");

    int nSuperstarsP1 = 0;
    int nSuperstarsP2 = 0;
    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p1 = state.GetP1().GetPlayer(static_cast<uint8_t>(i));
        const MSB_Player* p2 = state.GetP2().GetPlayer(static_cast<uint8_t>(i));
        if (p1 && p1->superstar.has_value()) nSuperstarsP1 += p1->superstar.value();
        if (p2 && p2->superstar.has_value()) nSuperstarsP2 += p2->superstar.value();
    }

    // put the superstar indicators into a free spot in memory.
    for (int team = 0; team < 2; team++)
    {
        const MSB_Team& t = (team == 0) ? state.GetP1() : state.GetP2();
        int nSuperstars = (team == 0) ? nSuperstarsP1 : nSuperstarsP2;
        uint32_t baseAddress = (team == 0) ? MSBQuickMatchCodeBuilder::SUPERSTAR_BOOLS_P1_BASE : MSBQuickMatchCodeBuilder::SUPERSTAR_BOOLS_P2_BASE;

        if (nSuperstars == 9)
        {
            // if a team has all 9 players superstarred, use the shorthand version of the code.
            outCodes.push_back(CustomGeckoCode((0x08000000 | (baseAddress & 0x00FFFFFF)), 0x00000001));
            outCodes.push_back(CustomGeckoCode( (0x00080000 | (MSBQuickMatchCodeBuilder::SUPERSTAR_BOOLS_STRIDE & 0x0000FFFF)), 0x00000000));
        }
        else
        {
            for (int rosterSpot = 0; rosterSpot < 9; rosterSpot++)
            {
                const MSB_Player* p = t.GetPlayerByBattingSlot(static_cast<uint8_t>(rosterSpot));
                if (!p || !p->superstar.has_value()) continue;

                uint8_t superstarVal = p->superstar.value();

                if (superstarVal == 1)
                {
                    int address = baseAddress + rosterSpot * MSBQuickMatchCodeBuilder::SUPERSTAR_BOOLS_STRIDE;
                    outCodes.push_back(CustomGeckoCode((0x00000000 | (address & 0x00FFFFFF)), 0x00000001));
                }
            }
        }
    }

    // build the c2 code - see the RIO-ASM ripo for the source ASM.
    outCodes.push_back(CustomGeckoCode(0xC205A4F4, 0x00000014));
    outCodes.push_back(CustomGeckoCode(0x2C1B0002, 0x41810090));
    outCodes.push_back(CustomGeckoCode(0x3C60802F, 0x3863BF99));
    outCodes.push_back(CustomGeckoCode(0x7C63DA14, 0x8B030000));
    outCodes.push_back(CustomGeckoCode(0x2C180000, 0x41820060));
    outCodes.push_back(CustomGeckoCode(0x3FC08033, 0x3BDE6726));
    outCodes.push_back(CustomGeckoCode(0x7FDEDA14, 0x2C18000A));
    outCodes.push_back(CustomGeckoCode(0x41800018, 0x41820008));
    outCodes.push_back(CustomGeckoCode(0x4800005C, 0x3B400000));
    outCodes.push_back(CustomGeckoCode(0x9B5E0000, 0x48000038));
    outCodes.push_back(CustomGeckoCode(0x9B1E0000, 0x3C608035));
    outCodes.push_back(CustomGeckoCode(0x38633BE5, 0x1FDB0009));
    outCodes.push_back(CustomGeckoCode(0x7FDEC214, 0x3BDEFFFF));
    outCodes.push_back(CustomGeckoCode(0x1FDE00A0, 0x7C63F214));
    outCodes.push_back(CustomGeckoCode(0x3FC08033, 0x3BDE677E));
    outCodes.push_back(CustomGeckoCode(0x7FDEDA14, 0x8B830000));
    outCodes.push_back(CustomGeckoCode(0x9B9E0000, 0x3C60802F));
    outCodes.push_back(CustomGeckoCode(0x3863BF99, 0x7C63DA14));
    outCodes.push_back(CustomGeckoCode(0x3B180001, 0x9B030000));
    outCodes.push_back(CustomGeckoCode(0x48000004, 0x3C608033));
    outCodes.push_back(CustomGeckoCode(0x60000000, 0x00000000));
}

void GenerateBattingOrderScreenGeckoCodes(
    const MSB_QuickMatchState& state,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    bool basicInputsValidated = true;

    if (!state.GetFirstBatter().has_value() || !state.GetHalfInning().has_value())
        basicInputsValidated = false;

    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p1 = state.GetP1().GetPlayer(static_cast<uint8_t>(i));
        const MSB_Player* p2 = state.GetP2().GetPlayer(static_cast<uint8_t>(i));
        if (!p1 || !p1->charID.has_value() || !p1->battingOrderSlot.has_value() ||
            !p2 || !p2->charID.has_value() || !p2->battingOrderSlot.has_value())
        {
            basicInputsValidated = false;
            break;
        }
    }

    if (!basicInputsValidated)
    {
        ERROR_LOG_FMT(COMMON, "Not all inputs provided for batting order gecko codes. No codes produced.");
        return;
    }
    INFO_LOG_FMT(COMMON, "All inputs provided for batting order gecko codes. Generating codes.");

    // if all inputs are validated, can generate gecko codes.
    // convert positions to P1/P2
    bool p1IsAway;
    if (state.GetHalfInning().value() == 0)
        p1IsAway = state.GetFirstBatter().value() == 0;
    else
        p1IsAway = state.GetFirstBatter().value() == 1;
    INFO_LOG_FMT(COMMON, "P1 is away: {}", p1IsAway);

    GenerateBattingOrderGeckoCodes(state, p1IsAway, outCodes);

    // captain location in batting order
    const MSB_Player* captainP1 = state.GetP1().GetCaptain();
    const MSB_Player* captainP2 = state.GetP2().GetCaptain();
    if (captainP1 != nullptr && captainP2 != nullptr)
    {
        for (int team = 0; team < 2; team++)
        {
            const MSB_Player* captain = (team == 0) ? captainP1 : captainP2;
            uint32_t address = (team == 0) ? MSBQuickMatchCodeBuilder::CAPTAIN_BATTING_ORDER_LOCATION_P1_ADDR
                                           : MSBQuickMatchCodeBuilder::CAPTAIN_BATTING_ORDER_LOCATION_P2_ADDR;

            if (captain->battingOrderSlot.has_value())
                outCodes.push_back(ToGeckoCode(0x00, address, captain->battingOrderSlot.value()));
            else
                WARN_LOG_FMT(COMMON, "Captain has no battingOrderSlot. No gecko code produced for captain location.");
        }
    }

    // handedness
    bool handednessInputsValidated = true;
    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p1 = state.GetP1().GetPlayer(static_cast<uint8_t>(i));
        const MSB_Player* p2 = state.GetP2().GetPlayer(static_cast<uint8_t>(i));
        if (!p1 || !p1->fieldingHand.has_value() || !p1->battingHand.has_value() ||
            !p2 || !p2->fieldingHand.has_value() || !p2->battingHand.has_value())
        {
            handednessInputsValidated = false;
            ERROR_LOG_FMT(COMMON, "Not all handedness inputs provided. No codes produced.");
        }
    }
    if (handednessInputsValidated)
        GenerateHandednessGeckoCodes(state, p1IsAway, outCodes);

    // superstars
    bool superstarInputsValidated = true;
    for (int i = 0; i < 9; i++)
    {
        const MSB_Player* p1 = state.GetP1().GetPlayer(static_cast<uint8_t>(i));
        const MSB_Player* p2 = state.GetP2().GetPlayer(static_cast<uint8_t>(i));
        if (!p1 || !p1->superstar.has_value() || !p2 || !p2->superstar.has_value())
        {
            superstarInputsValidated = false;
            ERROR_LOG_FMT(COMMON, "Not all superstar inputs provided. No codes produced.");
        }
    }
    if (superstarInputsValidated)
        GenerateSuperstarGeckoCodes(state, p1IsAway, outCodes);

    // prevent movement if all inputs provided
    if (basicInputsValidated && handednessInputsValidated && superstarInputsValidated && menuInputRestrictionEnabled)
    {
        outCodes.push_back(ToGeckoCode(0x04, MSBQuickMatchCodeBuilder::TEAM_MANAGEMENT_UP_PRESS_INSTR_ADDR, 0x48000074));
        outCodes.push_back(ToGeckoCode(0x04, MSBQuickMatchCodeBuilder::TEAM_MANAGEMENT_DOWN_PRESS_INSTR_ADDR, 0x48000074));
    }
}

std::vector<Gecko::GeckoCode> MSBQuickMatchCodeBuilder::MSB_GenerateQuickMatchSetupGeckoCode(
    const MSB_QuickMatchState& state)
{
    std::vector<Gecko::GeckoCode::Code> codes;

    bool p1IsAway = state.GetP1IsAway();

    const MSB_Team& awayTeam = p1IsAway ? state.GetP1() : state.GetP2();
    const MSB_Team& homeTeam = p1IsAway ? state.GetP2() : state.GetP1();

    // Pre-game codes - only runs on the main menu when rel = 4.
    // These are mainly for addresses related to game settings.
    codes.push_back(ToGeckoCode(0x28, REL_ADDR, MAIN_MENU_REL));

        const MSB_Player* captainP1 = state.GetP1().GetCaptain();
        const MSB_Player* captainP2 = state.GetP2().GetCaptain();

        if (captainP1 && captainP1->charID.has_value())
            codes.push_back(ToGeckoCode(0x04, CAPTAIN_CHARACTER_P1_ADDR, captainP1->charID.value()));

        if (captainP2 && captainP2->charID.has_value())
            codes.push_back(ToGeckoCode(0x04, CAPTAIN_CHARACTER_P2_ADDR, captainP2->charID.value()));

        if (captainP1 != nullptr && captainP2 != nullptr && menuInputRestrictionEnabled)
        {
            // if both captains provided, prevent P1 from selecting the CPU captain when spamming A to avoid an invalid read error.
            codes.push_back(ToGeckoCode(0x04, CAPTAIN_SCREEN_PREVENT_CPU_CAPTAIN_ADDR, CAPTAIN_SCREEN_PREVENT_CPU_CAPTAIN_NEW_INSTR));
        }

        GenerateRosterGeckoCodes(
            state.GetP1(),
            CHARACTERS_P1_BASE,
            CHARACTER_STRIDE,
            CHARACTER_SELECT_P1_SPOT_FILLED_ADDR,
            CHARACTER_SELECT_P1_OK_ACTIVE_ADDR,
            CHARACTER_SELECT_P1_CURSOR_ADDR,
            codes);

        GenerateRosterGeckoCodes(
            state.GetP2(),
            CHARACTERS_P2_BASE,
            CHARACTER_STRIDE,
            CHARACTER_SELECT_P2_SPOT_FILLED_ADDR,
            CHARACTER_SELECT_P2_OK_ACTIVE_ADDR,
            CHARACTER_SELECT_P2_CURSOR_ADDR,
            codes);

        // if both rosters provided, prevent cursor movement by nop'ing function call.
        if (state.GetP1().GetPlayer(0)->IsSet() && state.GetP2().GetPlayer(0)->IsSet() && menuInputRestrictionEnabled)
            codes.push_back(ToGeckoCode(0x04, CHARACTER_SELECT_PREVENT_CURSOR_MOVEMENT_ADDR, NOP_INSTR));

        // generate batting order codes.
        GenerateBattingOrderScreenGeckoCodes(state, codes);

        if (state.GetStadium().has_value())
        {
            // 0=Mario, 1=Bowser, 2=Wario, 3=Yoshi, 4=Peach, 5=DK
            codes.push_back(ToGeckoCode(0x00, STADIUM_ADDR, state.GetStadium().value()));

            // prevent cursor movement on stadium select screen
            if (menuInputRestrictionEnabled)
            {
                codes.push_back(ToGeckoCode(0x02, STADIUM_CURSOR_RIGHT_INSTR_ADDR, 0x0000));
                codes.push_back(ToGeckoCode(0x02, STADIUM_CURSOR_LEFT_INSTR_ADDR, 0x0000));
            }
        }

        if (state.GetFirstBatter().has_value())
            codes.push_back(ToGeckoCode(0x00, FIRST_BATTER_ADDR, state.GetFirstBatter().value()));

        if (state.GetStarSkills().has_value())
            codes.push_back(ToGeckoCode(0x00, STAR_SKILLS_ADDR, state.GetStarSkills().value()));

        if (state.GetInningsSelected().has_value())
        {
            // the address being set is the cursor index.
            // this code does the inverse of the bit manipulation to get from the index to the innings.
            // currently has a limitation that the innings selected can only be odd.
            // even numbers will result in the innings selected to be (value - 1)
            uint8_t inningsMenuIndex = (state.GetInningsSelected().value() - 1) >> 1;
            codes.push_back(ToGeckoCode(0x00, INNINGS_SELECTED_ADDR, inningsMenuIndex));
        }

        if (state.GetMercy().has_value())
            codes.push_back(ToGeckoCode(0x00, MERCY_ADDR, state.GetMercy().value()));

        // if all game settings are specified, prevent cursor movement on that screen
        if (state.GetFirstBatter().has_value() &&
            state.GetStarSkills().has_value() &&
            state.GetInningsSelected().has_value() &&
            state.GetMercy().has_value() &&
            menuInputRestrictionEnabled)
        {
            codes.push_back(ToGeckoCode(0x02, GAME_SETTINGS_CURSOR_RIGHT_INSTR_ADDR, 0x0000));
            codes.push_back(ToGeckoCode(0x02, GAME_SETTINGS_CURSOR_LEFT_INSTR_ADDR, 0x0000));
        }

    codes.push_back(EndConditional()); // end main menu conditional


    // In-game codes - only runs on the in-game rel = 5 and game has not started.
    // These are for addresses that affect the game state, and can be set after the match loads.
    codes.push_back(ToGeckoCode(0x28, REL_ADDR, IN_GAME_REL));
        codes.push_back(ToGeckoCode(0x28, HAS_GAME_STARTED_ADDR, (GAME_STARTED_MASK << 16) | GAME_NOT_STARTED));

            // Innings
            if (state.GetInning().has_value())
                codes.push_back(ToGeckoCode(0x04, INNING_ADDR, state.GetInning().value()));

            if (state.GetHalfInning().has_value())
                codes.push_back(ToGeckoCode(0x00, HALF_INNING_ADDR, state.GetHalfInning().value()));

            if (state.GetBattingTeam().has_value())
                codes.push_back(ToGeckoCode(0x04, BATTING_TEAM_ADDR, state.GetBattingTeam().value()));

            if (state.GetFieldingTeam().has_value())
                codes.push_back(ToGeckoCode(0x04, FIELDING_TEAM_ADDR, state.GetFieldingTeam().value()));

            // Score functions
            {
                std::optional<uint16_t> awayInningScores[18];
                for (int i = 0; i < 18; i++) awayInningScores[i] = state.GetAwayInningScore(i);
                GenerateTeamScoreGeckoCodes(state.GetAwayScore(), awayInningScores, SCORE_AWAY_ADDR, SCORE_BYINNING_AWAY_BASE, SCORE_STRIDE, codes);

                std::optional<uint16_t> homeInningScores[18];
                for (int i = 0; i < 18; i++) homeInningScores[i] = state.GetHomeInningScore(i);
                GenerateTeamScoreGeckoCodes(state.GetHomeScore(), homeInningScores, SCORE_HOME_ADDR, SCORE_BYINNING_HOME_BASE, SCORE_STRIDE, codes);
            }

            // count
            if (state.GetStrikes().has_value())
                codes.push_back(ToGeckoCode(0x04, STRIKES_ADDR, state.GetStrikes().value()));

            if (state.GetBalls().has_value())
                codes.push_back(ToGeckoCode(0x04, BALLS_ADDR, state.GetBalls().value()));

            if (state.GetOuts().has_value())
            {
                codes.push_back(ToGeckoCode(0x04, OUTS_ADDR, state.GetOuts().value()));
                codes.push_back(ToGeckoCode(0x04, OUTS_STORED_ADDR, state.GetOuts().value())); // need to set both addrs
            }

            // team stars
            if (state.GetP1().GetTeamStars().has_value())
                codes.push_back(ToGeckoCode(0x00, TEAM_STARS_P1_ADDR, state.GetP1().GetTeamStars().value()));

            if (state.GetP2().GetTeamStars().has_value())
                codes.push_back(ToGeckoCode(0x00, TEAM_STARS_P2_ADDR, state.GetP2().GetTeamStars().value()));

            // star chance
            if (state.GetIsStarChance().has_value())
                codes.push_back(ToGeckoCode(0x00, IS_STAR_CHANCE_ADDR, state.GetIsStarChance().value()));

            if (awayTeam.GetLogo().has_value())
                codes.push_back(ToGeckoCode(0x04, LOGO_AWAY_ADDR, awayTeam.GetLogo().value()));

            if (homeTeam.GetLogo().has_value())
                codes.push_back(ToGeckoCode(0x04, LOGO_HOME_ADDR, homeTeam.GetLogo().value()));


            // batting order and position struct
            GenerateOrderAndPositionGeckoCodes(awayTeam, ORDER_AND_POSITION_STRUCT_AWAY_BASE, codes);
            GenerateOrderAndPositionGeckoCodes(homeTeam, ORDER_AND_POSITION_STRUCT_HOME_BASE, codes);

            // runners on base
            // will only work if the characterByPosition argument is also given.
            // take care to ensure the values given align with characterByPosition.
            for (int i = 0; i < 3; i++)
            {
                if (state.GetRunnerFieldingPosition(i).has_value() && state.GetRunnerCharacterID(i).has_value())
                {
                    codes.push_back(ToGeckoCode(0x02, RUNNER_ROSTER_LOCATION_BASE + RUNNER_STRIDE * i, state.GetRunnerFieldingPosition(i).value()));
                    codes.push_back(ToGeckoCode(0x02, RUNNER_CHARACTER_ID_BASE + RUNNER_STRIDE * i, state.GetRunnerCharacterID(i).value()));

                    // nop the instruction that clears the roster ID when the game starts
                    codes.push_back(ToGeckoCode(0x04, RUNNER_NOP_BASE + RUNNER_NOP_STRIDE * i, NOP_INSTR));
                }
            }

            // pitcher stamina. On a P1/P2 basis, and by batting order.
            GeneratePitcherStaminaGeckoCodes(state.GetP1(), PITCHER_STAMINA_P1_BASE, codes);
            GeneratePitcherStaminaGeckoCodes(state.GetP2(), PITCHER_STAMINA_P2_BASE, codes);

        codes.push_back(EndConditional()); // end game-not-started conditional

        // after match started codes - generally everything should be set before this, but there is some post processing that could be needed.
        codes.push_back(ToGeckoCode(0x28, HAS_GAME_STARTED_ADDR, (GAME_STARTED_MASK << 16) | GAME_STARTED));

            // if runners initialized, replace the nop'd instruction once the game starts
            for (int i = 0; i < 3; i++)
            {
                if (state.GetRunnerFieldingPosition(i).has_value() && state.GetRunnerCharacterID(i).has_value())
                {
                    codes.push_back(ToGeckoCode(0x04, RUNNER_NOP_BASE + RUNNER_NOP_STRIDE * i, RUNNER_REPLACEMENT_INSTRUCTIONS[i]));
                }
            }

            // for superstarred players, need to replace the c2 injection instruction. No condition needed since just replacing vanilla instruction
            codes.push_back(ToGeckoCode(0x04, SUPERSTAR_INJECTION_ADDR, SUPERSTAR_INJECTION_REPLACEMENT_INSTR));

            // for team management cursor, need to replace original instruction once in game.
            codes.push_back(ToGeckoCode(0x04, TEAM_MANAGEMENT_UP_PRESS_INSTR_ADDR, TEAM_MANAGEMENT_UP_PRESS_REPLACEMENT_INSTR));
            codes.push_back(ToGeckoCode(0x04, TEAM_MANAGEMENT_DOWN_PRESS_INSTR_ADDR, TEAM_MANAGEMENT_DOWN_PRESS_REPLACEMENT_INSTR));

            // for P1 prevention of picking a CPU captain, need to replace original instruction once in game.
            codes.push_back(ToGeckoCode(0x04, CAPTAIN_SCREEN_PREVENT_CPU_CAPTAIN_ADDR, CAPTAIN_SCREEN_PREVENT_CPU_CAPTAIN_REPLACEMENT_INSTR));

        codes.push_back(EndConditional()); // end game-has-started conditional
    codes.push_back(EndConditional()); // end in-game conditional

    // finish filling out gecko code structure
    Gecko::GeckoCode geckoCode;
    geckoCode.name = "Custom Match State";
    geckoCode.enabled = true;
    geckoCode.built_in_code = true;
    geckoCode.user_defined = false;
    geckoCode.codes = std::move(codes);

    return { geckoCode };
}
