#pragma once

#include "GeckoCodeGenerator.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

// Optional game state struct
struct MSBGameState
{
    // Pre-game constants
    std::optional<uint32_t> captainCharacterP1; 
    std::optional<uint32_t> captainCharacterP2;  

    // rosters need to be given in position order (P, C, 1B, 2B, 3B, SS, LF, CF, RF)
    std::optional<uint8_t> charactersP1ByPosition[9]; 
    std::optional<uint8_t> charactersP2ByPosition[9];  

    std::optional<uint8_t> logoP1; // 0-47
    std::optional<uint8_t> logoP2; // 0-47

    std::optional<uint8_t> stadium; // 0=Mario, 1=Bowser, 2=Wario, 3=Yoshi, 4=Peach, 5=DK, 6=TF

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

    std::optional<uint16_t> homeScore;
    std::optional<uint16_t> awayScore;
    std::optional<uint16_t> homeInningScores[18];  // 18 innings is max the game holds in memory
    std::optional<uint16_t> awayInningScores[18];  

    std::optional<uint32_t> strikes;  
    std::optional<uint32_t> balls;       
    std::optional<uint32_t> outs;   
    
    std::optional<uint8_t> awayTeamStars;
    std::optional<uint8_t> homeTeamStars;

    std::optional<uint8_t> isStarChance; // 0=off, 1=on
};

class MSBMatchCodeBuilder
{
public:
    static void MSB_GenerateCustomMatchStateGeckoCode(
        const std::string& game_id,
        const MSBGameState& state,
        const std::string& code_name = "Custom Match State");

    // Logical Constants
    static constexpr uint32_t REL_ADDR = 0x800e877c;
    static constexpr uint16_t MAIN_MENU_REL = 4;
    static constexpr uint16_t IN_GAME_REL = 5;

    static constexpr uint32_t HAS_GAME_STARTED_ADDR = 0x80892ab4;
    static constexpr uint8_t GAME_NOT_STARTED = 0;
    static constexpr uint8_t GAME_STARTED = 1;
    static constexpr uint16_t GAME_STARTED_MASK = 0xFF00;


    // Pre game addresses
    static constexpr uint32_t CAPTAIN_CHARACTER_P1_ADDR = 0x80353080;
    static constexpr uint32_t CAPTAIN_CHARACTER_P2_ADDR = 0x80353084;
    
    static constexpr uint32_t CHARACTERS_P1_BASE = 0x803C6726;
    static constexpr uint32_t CHARACTERS_P2_BASE = 0x803C672F;
    static constexpr uint32_t CHARACTER_STRIDE = 0x01; 

    static constexpr uint32_t STADIUM_ADDR = 0x80750c37;
    static constexpr uint32_t STADIUM_CURSOR_RIGHT_INSTR_ADDR = 0x80650586;
    static constexpr uint32_t STADIUM_CURSOR_LEFT_INSTR_ADDR = 0x80650536;

    static constexpr uint32_t FIRST_BATTER_ADDR = 0x803c5f40;
    static constexpr uint32_t STAR_SKILLS_ADDR = 0x803c5f41;
    static constexpr uint32_t INNINGS_SELECTED_ADDR = 0x803c5f42;
    static constexpr uint32_t MERCY_ADDR = 0x803c5f43;
    static constexpr uint32_t GAME_SETTINGS_CURSOR_RIGHT_INSTR_ADDR = 0x80049616;
    static constexpr uint32_t GAME_SETTINGS_CURSOR_LEFT_INSTR_ADDR = 0x800495da;

    static constexpr uint32_t LOGO_P1_ADDR = 0x803530AD;
    static constexpr uint32_t LOGO_P2_ADDR = 0x803530AE;


    // In game addresses
    static constexpr uint32_t INNING_ADDR = 0x808928A0;
    static constexpr uint32_t HALF_INNING_ADDR = 0x8089294D;

    static constexpr uint32_t SCORE_AWAY_ADDR = 0x808928a4;
    static constexpr uint32_t SCORE_HOME_ADDR = 0x808928CA;
    static constexpr uint32_t SCORE_BYINNING_AWAY_BASE = 0x808928a6;
    static constexpr uint32_t SCORE_BYINNING_HOME_BASE = 0x808928cc;
    static constexpr uint32_t SCORE_STRIDE = 0x02; 

    static constexpr uint32_t STRIKES_ADDR = 0x80892968;
    static constexpr uint32_t BALLS_ADDR = 0x8089296C;
    static constexpr uint32_t OUTS_ADDR = 0x80892970;
    static constexpr uint32_t OUTS_STORED_ADDR = 0x80892974;

    static constexpr uint32_t TEAM_STARS_AWAY_ADDR = 0x80892ad6;
    static constexpr uint32_t TEAM_STARS_HOME_ADDR = 0x80892ad7;

    static constexpr uint32_t IS_STAR_CHANCE_ADDR = 0x80892ad8;

};