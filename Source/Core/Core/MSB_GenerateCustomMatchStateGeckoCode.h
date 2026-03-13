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

    // Single value memory addresses
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