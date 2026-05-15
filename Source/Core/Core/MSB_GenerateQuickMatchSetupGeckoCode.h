#pragma once

#include "Core/GeckoCode.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

// Optional game state struct
struct MSBQuickMatchGameState
{
    // Pre-game constants
    std::optional<uint32_t> captainCharacterP1;
    std::optional<uint32_t> captainCharacterP2;

    std::optional<uint8_t> captainPositionP1;
    std::optional<uint8_t> captainPositionP2;

    // rosters need to be given in position order (P, C, 1B, 2B, 3B, SS, LF, CF, RF)
    std::optional<uint8_t> charactersP1ByPosition[9]; 
    std::optional<uint8_t> charactersP2ByPosition[9];  
    
    // Handedness stored in position order, matching charactersP1/P2ByPosition
    // 0 = right, 1 = left
    std::optional<uint8_t> battingHandP1ByPosition[9];
    std::optional<uint8_t> battingHandP2ByPosition[9];
    std::optional<uint8_t> fieldingHandP1ByPosition[9];
    std::optional<uint8_t> fieldingHandP2ByPosition[9];

    // Superstar stored in position order
    // 0 = off, 1 = on
    std::optional<uint8_t> superstarP1ByPosition[9];
    std::optional<uint8_t> superstarP2ByPosition[9];

    std::optional<uint8_t> stadium; // this is the cursor position. Diff from in-game enums, which are: 0=Mario, 1=Bowser, 2=Wario, 3=Yoshi, 4=Peach, 5=DK

    std::optional<uint8_t> firstBatter; // 0=P1, 1=P2
    std::optional<uint8_t> starSkills; // 0=off, 1=on
    
    // actual int of the innings, not the cursor index. 
    // only odd numbers work. Evens will result in (value - 1) since we're setting the cursor index.
    // values > 9 also work.
    std::optional<uint8_t> inningsSelected; 

    std::optional<uint8_t> mercy; // 0=off, 1=on


    // In-game constants
    std::optional<uint32_t> inning; 
    std::optional<uint8_t> halfInning; // 0=top, 1=bottom

    // In home/away format.
    std::optional<uint32_t> battingTeam; 
    std::optional<uint32_t> fieldingTeam; 

    std::optional<uint16_t> homeScore;
    std::optional<uint16_t> awayScore;
    std::optional<uint16_t> homeInningScores[18];  // 18 innings is max the game holds in memory
    std::optional<uint16_t> awayInningScores[18];  

    std::optional<uint32_t> strikes;  
    std::optional<uint32_t> balls;       
    std::optional<uint32_t> outs;   
    
    std::optional<uint8_t> p1TeamStars;
    std::optional<uint8_t> p2TeamStars;

    std::optional<uint8_t> isStarChance; // 0=off, 1=on

    std::optional<uint32_t> logoAway; // 0-47
    std::optional<uint32_t> logoHome; // 0-47

    // for each spot in the batting order, enter the batters position
    std::optional<uint32_t> awayPositionByBattingOrder[9];
    std::optional<uint32_t> homePositionByBattingOrder[9];

    // needs to align the batting team's given roster order and characters
    std::optional<uint16_t> runnerRosterSpot[3]; // use their index in the characterByPosition struct.
    std::optional<uint16_t> runnerCharacterID[3];

    // note the P1/P2 basis. Also, array based on batting order.
    std::optional<uint16_t> pitcherStaminaP1[9];
    std::optional<uint16_t> pitcherStaminaP2[9];
};

class MSBQuickMatchCodeBuilder
{
public:
    static std::vector<Gecko::GeckoCode> MSB_GenerateQuickMatchSetupGeckoCode(const MSBQuickMatchGameState& state);

    // Logical Constants
    static constexpr uint32_t REL_ADDR = 0x800e877c;
    static constexpr uint16_t MAIN_MENU_REL = 4;
    static constexpr uint16_t IN_GAME_REL = 5;

    static constexpr uint32_t HAS_GAME_STARTED_ADDR = 0x80892ab4;
    static constexpr uint8_t GAME_NOT_STARTED = 0;
    static constexpr uint8_t GAME_STARTED = 1;
    static constexpr uint16_t GAME_STARTED_MASK = 0xFF00;

    static constexpr uint32_t NOP_INSTR = 0x60000000;


    // Pre game addresses
    static constexpr uint32_t CAPTAIN_CHARACTER_P1_ADDR = 0x80353080;
    static constexpr uint32_t CAPTAIN_CHARACTER_P2_ADDR = 0x80353084;

    static constexpr uint32_t CAPTAIN_BATTING_ORDER_LOCATION_P1_ADDR = 0x803530a9;
    static constexpr uint32_t CAPTAIN_BATTING_ORDER_LOCATION_P2_ADDR = 0x803530aa;

    static constexpr uint32_t CAPTAIN_SCREEN_PREVENT_CPU_CAPTAIN_ADDR = 0x806548dc;
    static constexpr uint32_t CAPTAIN_SCREEN_PREVENT_CPU_CAPTAIN_NEW_INSTR = 0x48000234;
    static constexpr uint32_t CAPTAIN_SCREEN_PREVENT_CPU_CAPTAIN_REPLACEMENT_INSTR = 0x38E70154;
    
    static constexpr uint32_t CHARACTERS_P1_BASE = 0x803C6726;
    static constexpr uint32_t CHARACTERS_P2_BASE = 0x803C672F;
    static constexpr uint32_t CHARACTER_STRIDE = 0x01; 
    static constexpr uint32_t CHARACTER_SELECT_P1_SPOT_FILLED_ADDR = 0x803C676E;
    static constexpr uint32_t CHARACTER_SELECT_P2_SPOT_FILLED_ADDR = 0x803C6777;
    static constexpr uint32_t CHARACTER_SELECT_P1_OK_ACTIVE_ADDR = 0x80750C7F;
    static constexpr uint32_t CHARACTER_SELECT_P2_OK_ACTIVE_ADDR = 0x80750C80;
    static constexpr uint32_t CHARACTER_SELECT_P1_CURSOR_ADDR = 0x80750c48;
    static constexpr uint32_t CHARACTER_SELECT_P2_CURSOR_ADDR = 0x80750c4C;
    static constexpr uint32_t CHARACTER_SELECT_PREVENT_CURSOR_MOVEMENT_ADDR = 0x8064df60;

    static constexpr uint32_t SUPERSTAR_INJECTION_ADDR = 0x8005a4f4;
    static constexpr uint32_t SUPERSTAR_INJECTION_REPLACEMENT_INSTR = 0x3C608033;
    static constexpr uint32_t SUPERSTAR_BOOLS_P1_BASE = 0x80353be5;
    static constexpr uint32_t SUPERSTAR_BOOLS_P2_BASE = 0x80354185;
    static constexpr uint32_t SUPERSTAR_BOOLS_STRIDE = 0xA0;
    static constexpr uint32_t SUPERSTAR_CODE_P1_INDEX_ADDR = 0x802EBF99;
    static constexpr uint32_t SUPERSTAR_CODE_P2_INDEX_ADDR = 0x802EBF9A;

    static constexpr uint32_t STADIUM_ADDR = 0x80750c37;
    static constexpr uint32_t STADIUM_CURSOR_RIGHT_INSTR_ADDR = 0x80650586;
    static constexpr uint32_t STADIUM_CURSOR_LEFT_INSTR_ADDR = 0x80650536;

    static constexpr uint32_t FIRST_BATTER_ADDR = 0x803c5f40;
    static constexpr uint32_t STAR_SKILLS_ADDR = 0x803c5f41;
    static constexpr uint32_t INNINGS_SELECTED_ADDR = 0x803c5f42;
    static constexpr uint32_t MERCY_ADDR = 0x803c5f43;
    static constexpr uint32_t GAME_SETTINGS_CURSOR_RIGHT_INSTR_ADDR = 0x80049616;
    static constexpr uint32_t GAME_SETTINGS_CURSOR_LEFT_INSTR_ADDR = 0x800495da;

    static constexpr uint32_t TEAM_MANAGEMENT_UP_PRESS_INSTR_ADDR = 0x800463c8;
    static constexpr uint32_t TEAM_MANAGEMENT_DOWN_PRESS_INSTR_ADDR = 0x80046440;
    static constexpr uint32_t TEAM_MANAGEMENT_UP_PRESS_REPLACEMENT_INSTR = 0x41820074;
    static constexpr uint32_t TEAM_MANAGEMENT_DOWN_PRESS_REPLACEMENT_INSTR = 0x41820074;


    // In game addresses
    static constexpr uint32_t INNING_ADDR = 0x808928A0;
    static constexpr uint32_t HALF_INNING_ADDR = 0x8089294D;
    
    static constexpr uint32_t BATTING_TEAM_ADDR = 0x80892998;
    static constexpr uint32_t FIELDING_TEAM_ADDR = 0x8089299C;

    static constexpr uint32_t SCORE_AWAY_ADDR = 0x808928a4;
    static constexpr uint32_t SCORE_HOME_ADDR = 0x808928CA;
    static constexpr uint32_t SCORE_BYINNING_AWAY_BASE = 0x808928a6;
    static constexpr uint32_t SCORE_BYINNING_HOME_BASE = 0x808928cc;
    static constexpr uint32_t SCORE_STRIDE = 0x02; 

    static constexpr uint32_t STRIKES_ADDR = 0x80892968;
    static constexpr uint32_t BALLS_ADDR = 0x8089296C;
    static constexpr uint32_t OUTS_ADDR = 0x80892970;
    static constexpr uint32_t OUTS_STORED_ADDR = 0x80892974;

    static constexpr uint32_t TEAM_STARS_P1_ADDR = 0x80892ad6;
    static constexpr uint32_t TEAM_STARS_P2_ADDR = 0x80892ad7;

    static constexpr uint32_t IS_STAR_CHANCE_ADDR = 0x80892ad8;

    static constexpr uint32_t LOGO_AWAY_ADDR = 0x808929b0;
    static constexpr uint32_t LOGO_HOME_ADDR = 0x808929bc;

    static constexpr uint32_t ORDER_AND_POSITION_STRUCT_AWAY_BASE = 0x808929c8;
    static constexpr uint32_t ORDER_AND_POSITION_STRUCT_HOME_BASE = 0x80892a18;
    static constexpr uint32_t ORDER_AND_POSITION_STRUCT_CHARACTER_STRIDE = 0x08;
    static constexpr uint32_t ORDER_AND_POSITION_STRUCT_POSITION_STRIDE = 0x04;

    static constexpr uint32_t RUNNER_ROSTER_LOCATION_BASE = 0x8088f04c;
    static constexpr uint32_t RUNNER_CHARACTER_ID_BASE = 0x8088f04e;
    static constexpr uint32_t RUNNER_STRIDE = 0x154;
    static constexpr uint32_t RUNNER_NOP_BASE = 0x806c9420;
    static constexpr uint32_t RUNNER_NOP_STRIDE = 0x30;
    static constexpr uint32_t RUNNER_REPLACEMENT_INSTRUCTIONS[3] = {0xB0650234, 0xB06500E0, 0xB06500E0};

    static constexpr uint32_t PITCHER_STAMINA_P1_BASE = 0x803535d8;
    static constexpr uint32_t PITCHER_STAMINA_P2_BASE = 0x803536e6;
    static constexpr uint32_t PITCHER_STAMINA_STRIDE = 0x1E;
};

extern bool menuInputRestrictionEnabled; // enables gecko codes that prevent the user from moving during the menuing.