#pragma once

#include "GeckoCodeGenerator.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

// Optional game state struct
struct MSBGameState
{
    // Team current scores
    std::optional<uint16_t> homeScore;
    std::optional<uint16_t> awayScore;

    // Team scores per inning (0-17) 
    std::optional<uint16_t> homeInningScores[18];  // 18 innings is max the game holds in memory
    std::optional<uint16_t> awayInningScores[18];  

    // Count state
    std::optional<uint8_t> balls;       
    std::optional<uint8_t> strikes;     
    std::optional<uint8_t> outs;        
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

    // In game addresses
    static constexpr uint32_t BALLS_ADDR = 0x8089296C;
    static constexpr uint32_t STRIKES_ADDR = 0x80892968;
    static constexpr uint32_t OUTS_ADDR = 0x80892970;
    static constexpr uint32_t OUTS_STORED_ADDR = 0x80892974;
    
    static constexpr uint32_t SCORE_AWAY_ADDR = 0x808928CA;
    static constexpr uint32_t SCORE_HOME_ADDR = 0x80892974;

    // Inning score memory base addresses
    static constexpr uint32_t SCORE_BYINNING_AWAY_BASE = 0x808928a6;
    static constexpr uint32_t SCORE_BYINNING_HOME_BASE = 0x808928cc;
    static constexpr uint32_t SCORE_STRIDE = 0x02; 


};