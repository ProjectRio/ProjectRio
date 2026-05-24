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

void GenerateRosterGeckoCodes(
    const std::optional<uint8_t> charactersByPosition[9],
    uint32_t rosterBaseAddress,
    uint32_t stride,
    uint32_t spotFilledAddress,
    uint32_t okButtonActiveAddress,
    uint32_t cursorLocationAddress,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GenerateRosterGeckoCodes function");

    // check if any character ID is given
    bool rosterProvided = false;
    for (int i = 0; i < 9; i++)
    {
        if (charactersByPosition[i].has_value()) 
        {
            rosterProvided = true;
            break;
        }
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
            if (charactersByPosition[i].has_value())
            {
                uint8_t val = charactersByPosition[i].value();
                if (val > 0x35)
                    WARN_LOG_FMT(COMMON, "{} not a valid character ID for position {}. No gecko code produced.", val, i);
                else
                    outCodes.push_back(ToGeckoCode(0x00, rosterBaseAddress + i*stride, val));
            }
            else 
                ERROR_LOG_FMT(COMMON, "Missing character {} at address {:#010x}.", i, rosterBaseAddress + i*stride);
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
    const std::optional<uint32_t> positionByBattingOrder[9],
    uint32_t structBase,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GenerateOrderAndPositionGeckoCodes function");

    bool positionsProvided = false;
    for (int i = 0; i < 9; i++)
    {
        if (positionByBattingOrder[i].has_value()) 
        {
            positionsProvided = true;
            break;
        }
    }

    if (positionsProvided)
    {
        for (int i = 0; i < 9; i++)
        {
            // i + 1 since first index is a copy of the pitchers spot in the order + position.
            uint32_t address = 
                structBase + 
                MSBQuickMatchCodeBuilder::ORDER_AND_POSITION_STRUCT_CHARACTER_STRIDE * (i + 1) + 
                MSBQuickMatchCodeBuilder::ORDER_AND_POSITION_STRUCT_POSITION_STRIDE;

            if (positionByBattingOrder[i].has_value())
            {
                uint32_t val = positionByBattingOrder[i].value();
                if (val > 0x8)
                    WARN_LOG_FMT(COMMON, "Position not valid: {} for batting order index {}. No gecko code produced.", val, i);
                else
                    outCodes.push_back(ToGeckoCode(0x04, address, val));

                // if pitcher, fill in their spot in the batting order
                if (val == 0)
                {
                    outCodes.push_back(ToGeckoCode(0x04, structBase, static_cast<uint32_t>(i)));
                    outCodes.push_back(ToGeckoCode(0x04, structBase + MSBQuickMatchCodeBuilder::ORDER_AND_POSITION_STRUCT_POSITION_STRIDE, 0x00000000));
                }
            }
        }
    }
}

void GeneratePitcherStaminaGeckoCodes(
    const std::optional<uint16_t> staminaList[9],
    uint32_t baseAddr,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GeneratePitcherStaminaGeckoCodes function");

    for (int i = 0; i < 9; i++)
    {
        if (staminaList[i].has_value())
        {
            uint32_t address = baseAddr + MSBQuickMatchCodeBuilder::PITCHER_STAMINA_STRIDE * i;
            outCodes.push_back(ToGeckoCode(0x02, address, staminaList[i].value()));

        }
    }
}

void GenerateBattingOrderGeckoCodes(
    const MSBQuickMatchGameState& state,
    const bool p1IsAway,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GenerateBattingOrderGeckoCodes function");

    uint32_t positionP1ByBattingOrder[9];
    uint32_t positionP2ByBattingOrder[9];
    for (int i = 0; i < 9; i++)
    {
        positionP1ByBattingOrder[i] = p1IsAway ? state.awayPositionByBattingOrder[i].value() : state.homePositionByBattingOrder[i].value();
        positionP2ByBattingOrder[i] = p1IsAway ? state.homePositionByBattingOrder[i].value() : state.awayPositionByBattingOrder[i].value();
    }

    // add header
    outCodes.push_back(CustomGeckoCode(0xC2066A48, 0x00000016));
    // check if team equals P2 (1)
    outCodes.push_back(CustomGeckoCode(0x3AE10038, 0x2C080001));
    // branch to P2 code. Nop for alignment.
    outCodes.push_back(CustomGeckoCode(0x41820058, MSBQuickMatchCodeBuilder::NOP_INSTR));

    // P1 batting order
    for (int i = 0; i < 9; i++)
    {
        uint32_t position = positionP1ByBattingOrder[i];
        uint8_t charID = state.charactersP1ByPosition[position].value();

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
        uint32_t position = positionP2ByBattingOrder[i];
        uint8_t charID = state.charactersP2ByPosition[position].value();

        uint32_t firstWord = 0x39800000 | (charID & 0xFF);
        uint32_t secondWord = 0x99970000 | (i & 0xFF);

        outCodes.push_back(CustomGeckoCode(firstWord, secondWord));
    }

    // finish the code
    outCodes.push_back(CustomGeckoCode(MSBQuickMatchCodeBuilder::NOP_INSTR, 0x00000000));
}

void GenerateHandednessGeckoCodes(
    const MSBQuickMatchGameState& state,
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
        // get correct batting order team since that's defined as away/home.
        uint32_t positionByBattingOrder[9];
        for (int i = 0; i < 9; i++)
        {
            if (p1IsAway)
                positionByBattingOrder[i] = 
                    (team == 0) ? 
                        state.awayPositionByBattingOrder[i].value() : 
                        state.homePositionByBattingOrder[i].value();
            else
                positionByBattingOrder[i] = 
                    (team == 0) ? 
                        state.homePositionByBattingOrder[i].value() : 
                        state.awayPositionByBattingOrder[i].value();
        }

        for (int rosterSpot = 0; rosterSpot < 9; rosterSpot++)
        {
            uint32_t position = positionByBattingOrder[rosterSpot];

            uint8_t fieldingHand = 
                (team == 0) ? 
                    state.fieldingHandP1ByPosition[position].value() : 
                    state.fieldingHandP2ByPosition[position].value();
            uint8_t battingHand = 
                (team == 0) ? 
                    state.battingHandP1ByPosition[position].value() : 
                    state.battingHandP2ByPosition[position].value();

            // load handedness into registers r3 and r6.
            uint32_t fieldingInt = 0x38600000 | (fieldingHand & 0xFF);
            uint32_t battingInt = 0x38C00000 | (battingHand & 0xFF);
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
    const MSBQuickMatchGameState& state,
    const bool p1IsAway,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    INFO_LOG_FMT(COMMON, "Running GenerateSuperstarGeckoCodes function");
    
    int nSuperstarsP1 = 0;
    int nSuperstarsP2 = 0;
    for (int i = 0; i < 9; i++)
    {
        nSuperstarsP1 += state.superstarP1ByPosition[i].value();
        nSuperstarsP2 += state.superstarP2ByPosition[i].value();
    }

    // put the superstar indicators into a free spot in memory.
    for (int team = 0; team < 2; team++)
    {
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
            // get correct batting order team since that's defined as away/home.
            uint32_t positionByBattingOrder[9];
            for (int i = 0; i < 9; i++)
            {
                if (p1IsAway)
                    positionByBattingOrder[i] = 
                        (team == 0) ? 
                            state.awayPositionByBattingOrder[i].value() : 
                            state.homePositionByBattingOrder[i].value();
                else
                    positionByBattingOrder[i] = 
                        (team == 0) ? 
                            state.homePositionByBattingOrder[i].value() : 
                            state.awayPositionByBattingOrder[i].value();
            }

            for (int rosterSpot = 0; rosterSpot < 9; rosterSpot++)
            {
                uint32_t position = positionByBattingOrder[rosterSpot];

                uint8_t superstarVal = 
                    (team == 0) ? 
                        state.superstarP1ByPosition[position].value() : 
                        state.superstarP2ByPosition[position].value();

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
    const MSBQuickMatchGameState state,
    std::vector<Gecko::GeckoCode::Code>& outCodes
)
{
    bool basicInputsValidated = true;

    if (!state.firstBatter.has_value() || !state.halfInning.has_value())
        basicInputsValidated = false;
    
    for (int i = 0; i < 9; i++)
    {
        if (!state.charactersP1ByPosition[i].has_value() || !state.charactersP2ByPosition[i].has_value() || 
            !state.awayPositionByBattingOrder[i].has_value() || !state.homePositionByBattingOrder[i].has_value())
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
    if (state.halfInning.value() == 0)
        p1IsAway = state.firstBatter.value() == 0;
    else
        p1IsAway = state.firstBatter.value() == 1;
    INFO_LOG_FMT(COMMON, "P1 is away: {}", p1IsAway);

    GenerateBattingOrderGeckoCodes(state, p1IsAway, outCodes);

    // captain location in batting order
    if (state.captainPositionP1.has_value() && state.captainPositionP2.has_value())
    {
        for (int team = 0; team < 2; team++)
        {
            uint8_t captainPosition = (team == 0) ? state.captainPositionP1.value() : state.captainPositionP2.value();
            
            uint8_t captainOrderLoc = -1;
            for (int orderLoc = 0; orderLoc < 9; orderLoc++)
            {
                uint8_t position = (team == 0) ? 
                    (p1IsAway ? state.awayPositionByBattingOrder[orderLoc].value() : state.homePositionByBattingOrder[orderLoc].value()) :
                    (p1IsAway ? state.homePositionByBattingOrder[orderLoc].value() : state.awayPositionByBattingOrder[orderLoc].value());

                if (position == captainPosition)
                {
                    captainOrderLoc = orderLoc;
                    break;
                }
            }

            uint32_t address = (team == 0) ? MSBQuickMatchCodeBuilder::CAPTAIN_BATTING_ORDER_LOCATION_P1_ADDR : MSBQuickMatchCodeBuilder::CAPTAIN_BATTING_ORDER_LOCATION_P2_ADDR;

            if (captainPosition > 0x8 || captainOrderLoc == -1)
                WARN_LOG_FMT(COMMON, "Captain position not valid: {}. No gecko code produced for captain location.", captainPosition);
            else
                outCodes.push_back(ToGeckoCode(0x00, address, captainOrderLoc));
        }
    }

    // handedness
    bool handednessInputsValidated = true;
    for (int i = 0; i < 9; i++)
    {
        if (!state.fieldingHandP1ByPosition[i].has_value() || !state.fieldingHandP2ByPosition[i].has_value() ||
            !state.battingHandP1ByPosition[i].has_value() || !state.battingHandP2ByPosition[i].has_value() ||
            !state.awayPositionByBattingOrder[i].has_value() || !state.homePositionByBattingOrder[i].has_value())
        {
            handednessInputsValidated = false;
            ERROR_LOG_FMT(COMMON, "Not all handedness inputs provided. No codes produced.");
        }
    }
    if (handednessInputsValidated)
        GenerateHandednessGeckoCodes(state, p1IsAway, outCodes);

    // superstars
    bool superstarInputsValidated = true;
    for (int i = 0; i < 9; i++ )
    {
        if (!state.superstarP1ByPosition[i].has_value() || !state.superstarP2ByPosition[i].has_value() ||
            !state.awayPositionByBattingOrder[i].has_value() || !state.homePositionByBattingOrder[i].has_value())
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
    const MSBQuickMatchGameState& state)
{
    std::vector<Gecko::GeckoCode::Code> codes;

    // Pre-game codes - only runs on the main menu when rel = 4.
    // These are mainly for addresses related to game settings.
    codes.push_back(ToGeckoCode(0x28, REL_ADDR, MAIN_MENU_REL));
    
        if (state.captainCharacterP1.has_value())
        {
            uint32_t val = state.captainCharacterP1.value();
            if (val > 0x35)
                WARN_LOG_FMT(COMMON, "P1 captain not a valid character ID: {}. No gecko code produced.", val);
            else
                codes.push_back(ToGeckoCode(0x04, CAPTAIN_CHARACTER_P1_ADDR, state.captainCharacterP1.value()));
        }

        if (state.captainCharacterP2.has_value())
        {
            uint32_t val = state.captainCharacterP2.value();
            if (val > 0x35)
                WARN_LOG_FMT(COMMON, "P2 captain not a valid character ID: {}. No gecko code produced.", val);
            else
                codes.push_back(ToGeckoCode(0x04, CAPTAIN_CHARACTER_P2_ADDR, state.captainCharacterP2.value()));
        }

        if (state.captainCharacterP1.has_value() && state.captainCharacterP2.has_value() && menuInputRestrictionEnabled)
        {
            // if both captains provided, prevent P1 from selecting the CPU captain when spamming A to avoid an invalid read error.
            codes.push_back(ToGeckoCode(0x04, CAPTAIN_SCREEN_PREVENT_CPU_CAPTAIN_ADDR, CAPTAIN_SCREEN_PREVENT_CPU_CAPTAIN_NEW_INSTR));
        }
        
        GenerateRosterGeckoCodes(
            state.charactersP1ByPosition, 
            CHARACTERS_P1_BASE, 
            CHARACTER_STRIDE, 
            CHARACTER_SELECT_P1_SPOT_FILLED_ADDR,
            CHARACTER_SELECT_P1_OK_ACTIVE_ADDR,
            CHARACTER_SELECT_P1_CURSOR_ADDR,
            codes);

        GenerateRosterGeckoCodes(
            state.charactersP2ByPosition, 
            CHARACTERS_P2_BASE, 
            CHARACTER_STRIDE, 
            CHARACTER_SELECT_P2_SPOT_FILLED_ADDR,
            CHARACTER_SELECT_P2_OK_ACTIVE_ADDR,
            CHARACTER_SELECT_P2_CURSOR_ADDR,
            codes);

        // if both rosters provided, prevent cursor movement by nop'ing function call.
        if (state.charactersP1ByPosition[0].has_value() && state.charactersP2ByPosition[0].has_value() && menuInputRestrictionEnabled)
            codes.push_back(ToGeckoCode(0x04, CHARACTER_SELECT_PREVENT_CURSOR_MOVEMENT_ADDR, NOP_INSTR));

        // generate batting order codes.
        GenerateBattingOrderScreenGeckoCodes(state, codes);

        if (state.stadium.has_value())
        {
            uint8_t val = state.stadium.value();
            if (val > 0x5) // not accepting 6 for Toy Field. Support could be added in future.
                WARN_LOG_FMT(COMMON, "Stadium ID not valid: {}. No gecko code produced.", val);
            else
            {
                // 0=Mario, 1=Bowser, 2=Wario, 3=Yoshi, 4=Peach, 5=DK
                codes.push_back(ToGeckoCode(0x00, STADIUM_ADDR, val));

                // prevent cursor movement on stadium select screen
                if (menuInputRestrictionEnabled)
                {
                    codes.push_back(ToGeckoCode(0x02, STADIUM_CURSOR_RIGHT_INSTR_ADDR, 0x0000));
                    codes.push_back(ToGeckoCode(0x02, STADIUM_CURSOR_LEFT_INSTR_ADDR, 0x0000));
                }
            }
        }

        if (state.firstBatter.has_value())
        {
            uint8_t val = state.firstBatter.value();
            if (val > 0x1)
                WARN_LOG_FMT(COMMON, "First bat setting not valid: {}. No gecko code produced.", val);
            else
                codes.push_back(ToGeckoCode(0x00, FIRST_BATTER_ADDR, val));
        }

        if (state.starSkills.has_value())
        {
            uint8_t val = state.starSkills.value();
            if (val > 0x1)
                WARN_LOG_FMT(COMMON, "Star skill setting not valid: {}. No gecko code produced.", val);
            else
                codes.push_back(ToGeckoCode(0x00, STAR_SKILLS_ADDR, val));
        }

        if (state.inningsSelected.has_value())
        {
            // the address being set is the cursor index.
            // this code does the inverse of the bit manipulation to get from the index to the innings.
            // currently has a limitation that the innings selected can only be odd.
            // even numbers will result in the innings selected to be (value - 1)
            uint8_t inningsDesired = state.inningsSelected.value();

            if (inningsDesired % 2 == 0)
                WARN_LOG_FMT(COMMON, "Innings selected is even: {}. Only odd values work. Result will be {}", inningsDesired, inningsDesired - 1);

            uint8_t inningsMenuIndex = (inningsDesired - 1) >> 1;
            codes.push_back(ToGeckoCode(0x00, INNINGS_SELECTED_ADDR, inningsMenuIndex));
        }

        if (state.mercy.has_value())
        {
            uint8_t val = state.mercy.value();
            if (val > 0x1)
                WARN_LOG_FMT(COMMON, "Mercy setting not valid: {}. No gecko code produced.", val);
            else
                codes.push_back(ToGeckoCode(0x00, MERCY_ADDR, val));
        }

        // if all game settings are specified, prevent cursor movement on that screen
        if (state.firstBatter.has_value() &&
            state.starSkills.has_value() &&
            state.inningsSelected.has_value() &&
            state.mercy.has_value() &&
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
            if (state.inning.has_value())
            {
                uint32_t val = state.inning.value();
                if (val > 0x12)
                    WARN_LOG_FMT(COMMON, "Inning setting not valid: {}. Max is 18. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x04, INNING_ADDR, val));
            }

            if (state.halfInning.has_value())
            {
                uint8_t val = state.halfInning.value();
                if (val > 0x1)
                    WARN_LOG_FMT(COMMON, "Half inning setting not valid: {}. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x00, HALF_INNING_ADDR, val));
            }

            if (state.battingTeam.has_value())
            {
                uint32_t val = state.battingTeam.value();
                if (val > 0x1)
                    WARN_LOG_FMT(COMMON, "Batting team setting not valid: {}. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x04, BATTING_TEAM_ADDR, val));
            }

            if (state.fieldingTeam.has_value())
            {
                uint32_t val = state.fieldingTeam.value();
                if (val > 0x1)
                    WARN_LOG_FMT(COMMON, "Fielding team setting not valid: {}. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x04, FIELDING_TEAM_ADDR, val));
            }

            // Score functions
            GenerateTeamScoreGeckoCodes(state.awayScore, state.awayInningScores, SCORE_AWAY_ADDR, SCORE_BYINNING_AWAY_BASE, SCORE_STRIDE, codes);
            GenerateTeamScoreGeckoCodes(state.homeScore, state.homeInningScores, SCORE_HOME_ADDR, SCORE_BYINNING_HOME_BASE, SCORE_STRIDE, codes);

            // count
            if (state.strikes.has_value())
            {
                uint32_t val = state.strikes.value();
                if (val > 0x2)
                    WARN_LOG_FMT(COMMON, "Strikes setting not valid: {}. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x04, STRIKES_ADDR, val));
            }

            if (state.balls.has_value())
            {
                uint32_t val = state.balls.value();
                if (val > 0x3)
                    WARN_LOG_FMT(COMMON, "Balls setting not valid: {}. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x04, BALLS_ADDR, val));
            }

            if (state.outs.has_value()) 
            {
                uint32_t val = state.outs.value();
                if (val > 0x2)
                    WARN_LOG_FMT(COMMON, "Outs setting not valid: {}. No gecko code produced.", val);
                else
                {
                    codes.push_back(ToGeckoCode(0x04, OUTS_ADDR, val));
                    codes.push_back(ToGeckoCode(0x04, OUTS_STORED_ADDR, val)); // need to both addrs
                }
            }

            // team stars
            if (state.p1TeamStars.has_value())
            {
                uint8_t val = state.p1TeamStars.value();
                if (val > 0x5)
                    WARN_LOG_FMT(COMMON, "P1 team stars not valid: {}. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x00, TEAM_STARS_P1_ADDR, val));
            }

            if (state.p2TeamStars.has_value())
            {
                uint8_t val = state.p2TeamStars.value();
                if (val > 0x5)
                    WARN_LOG_FMT(COMMON, "P2 team stars not valid: {}. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x00, TEAM_STARS_P2_ADDR, val));
            }

            // star chance
            if (state.isStarChance.has_value())
            {
                uint8_t val = state.isStarChance.value();
                if (val > 0x1)
                    WARN_LOG_FMT(COMMON, "Star chance setting not valid: {}. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x00, IS_STAR_CHANCE_ADDR, val));
            }        
            
            if (state.logoAway.has_value())
            {
                uint32_t val = state.logoAway.value();
                if (val > 0x2F)
                    WARN_LOG_FMT(COMMON, "Away logo ID not valid: {}. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x04, LOGO_AWAY_ADDR, val));
            }

            if (state.logoHome.has_value())
            {
                uint32_t val = state.logoHome.value();
                if (val > 0x2F)
                    WARN_LOG_FMT(COMMON, "Home logo ID not valid: {}. No gecko code produced.", val);
                else
                    codes.push_back(ToGeckoCode(0x04, LOGO_HOME_ADDR, val));
            }


            // batting order and position struct
            GenerateOrderAndPositionGeckoCodes(
                state.awayPositionByBattingOrder, 
                ORDER_AND_POSITION_STRUCT_AWAY_BASE,
                codes);
            
            GenerateOrderAndPositionGeckoCodes(
                state.homePositionByBattingOrder, 
                ORDER_AND_POSITION_STRUCT_HOME_BASE,
                codes);

            // runners on base
            // will only work if the characterByPosition argument is also given. 
            // take care to ensure the values given align with characterByPosition.
            for (int i = 0; i < 3; i++) 
            {
                if (state.runnerRosterSpot[i].has_value() && state.runnerCharacterID[i].has_value())
                {
                    uint16_t val_roster = state.runnerRosterSpot[i].value();
                    if (val_roster > 0x8)
                        WARN_LOG_FMT(COMMON, "Runner roster ID not valid: {} for runner index {}. No gecko code produced.", val_roster, i);
                    else
                        codes.push_back(ToGeckoCode(0x02, RUNNER_ROSTER_LOCATION_BASE + RUNNER_STRIDE * i, val_roster));

                    uint16_t val_charID = state.runnerCharacterID[i].value();
                    if (val_charID> 0x35)
                        WARN_LOG_FMT(COMMON, "Runner character ID not valid: {} for runner index {}. No gecko code produced.", val_charID, i);
                    else
                        codes.push_back(ToGeckoCode(0x02, RUNNER_CHARACTER_ID_BASE + RUNNER_STRIDE * i, val_charID));

                    // nop the instruction that clears the roster ID when the game starts
                    codes.push_back(ToGeckoCode(0x04, RUNNER_NOP_BASE + RUNNER_NOP_STRIDE * i, NOP_INSTR));
                }
            }

            // pitcher stamina. On a P1/P2 basis, and by batting order.
            GeneratePitcherStaminaGeckoCodes(state.pitcherStaminaP1, PITCHER_STAMINA_P1_BASE, codes);
            GeneratePitcherStaminaGeckoCodes(state.pitcherStaminaP2, PITCHER_STAMINA_P2_BASE, codes);

        codes.push_back(EndConditional()); // end game-not-started conditional
        
        // after match started codes - generally everything should be set before this, but there is some post processing that could be needed.
        codes.push_back(ToGeckoCode(0x28, HAS_GAME_STARTED_ADDR, (GAME_STARTED_MASK << 16) | GAME_STARTED));

            // if runners initialized, replace the nop'd instruction once the game starts
            for (int i = 0; i < 3; i++) 
            {
                if (state.runnerRosterSpot[i].has_value() && state.runnerCharacterID[i].has_value())
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