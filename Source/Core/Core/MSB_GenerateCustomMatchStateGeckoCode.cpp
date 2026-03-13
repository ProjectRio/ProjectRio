#include "MSB_GenerateCustomMatchStateGeckoCode.h" 
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <optional>
#include <algorithm>
#include <cstdint>

// Helper: convert address, value, and Gecko type to Gecko code line
static std::string ToGeckoLine(uint32_t geckoType, uint32_t address, uint32_t value)
{
    std::ostringstream oss;
    oss << std::hex;

    // Gecko type: 00, 02, 04, etc.
    oss.width(2); oss.fill('0'); oss << geckoType;

    // Address: always 6 hex digits
    oss.width(6); oss.fill('0'); oss << address << " ";

    // Value: always 8 hex digits
    oss.width(8); oss.fill('0'); oss << value;

    return oss.str();
}

// score helper function to manage cases related to providing current and/or inning scores.
void GenerateTeamScoreGeckoCodes(
    const std::optional<uint16_t>& currentScore,
    const std::optional<uint16_t> inningScores[18],
    uint32_t currentScoreAddress,
    uint32_t inningScoresBaseAddress,
    std::vector<std::string>& outLines,
    const std::string& teamName = "",
    uint32_t stride = 2  // default 2 bytes between innings
)
{
    // Determine if any inning scores are provided
    bool inningScoresProvided = false;
    uint32_t inningScoresSum = 0;
    for (int i = 0; i < 18; ++i)
    {
        const auto& scoreOpt = inningScores[i];
        if (scoreOpt.has_value())
        {
            inningScoresProvided = true;
            int val = scoreOpt.value();
            // Validate value
            if (val < 0) val = 0;
            inningScoresSum += val;
        }
    }

    bool totalScoreProvided = currentScore.has_value();
    uint16_t totalScore = totalScoreProvided ? currentScore.value() : 0;
    if (totalScore <0)
    {
        totalScore = 0;
        std::cout << "Error: Total score: " << totalScore << " less than 0. Changing to 0." << std::endl;
    }

    // Case: both total score and inning scores provided
    if (totalScoreProvided && inningScoresProvided)
    {
        // Validate total vs sum
        if (totalScore != inningScoresSum)
        {
            // mismatch → set total to 99 to show error
            totalScore = 99;
            std::cout << "Score mismatch: " << totalScore << " vs sum " << inningScoresSum << std::endl;
        }

        // Generate total score Gecko code
        outLines.push_back(ToGeckoLine(0x02, currentScoreAddress, totalScore));

        // Generate per-inning Gecko codes
        for (int i = 0; i < 18; ++i)
        {
            const auto& scoreOpt = inningScores[i];
            if (scoreOpt.has_value())
            {
                uint16_t val = scoreOpt.value();
                val = std::max<uint16_t>(0, scoreOpt.value()); // validate non-negative
                outLines.push_back(ToGeckoLine(0x02, inningScoresBaseAddress + i*stride, val));
            }
        }
    }
    // Case: only total score provided
    else if (totalScoreProvided)
    {
        // Set first inning to total score
        outLines.push_back(ToGeckoLine(0x02, inningScoresBaseAddress, totalScore));
        // Generate total score Gecko code
        outLines.push_back(ToGeckoLine(0x02, currentScoreAddress, totalScore));
    }
    // Case: only inning scores provided
    else if (inningScoresProvided)
    {
        // Sum the innings for total score
        totalScore = inningScoresSum;

        // Generate total score Gecko code
        outLines.push_back(ToGeckoLine(0x02, currentScoreAddress, totalScore));

        // Generate per-inning Gecko codes
        for (int i = 0; i < 18; ++i)
        {
            const auto& scoreOpt = inningScores[i];
            if (scoreOpt.has_value())
            {
                uint16_t val = scoreOpt.value();
                val = std::max<uint16_t>(0, scoreOpt.value()); // validate non-negative
                outLines.push_back(ToGeckoLine(0x02, inningScoresBaseAddress + i*stride, val));
            }
        }
    }
    // Case: neither provided → do nothing
    else
    {
        // no Gecko codes generated
    }
}

void MSBMatchCodeBuilder::MSB_GenerateCustomMatchStateGeckoCode(
    const std::string& game_id,
    const MSBGameState& state,
    const std::string& code_name)
{
    std::vector<std::string> lines;

    // Score functions
    GenerateTeamScoreGeckoCodes(state.awayScore, state.awayInningScores, SCORE_AWAY_ADDR, SCORE_BYINNING_AWAY_BASE, lines, "Away", SCORE_STRIDE);
    GenerateTeamScoreGeckoCodes(state.homeScore, state.homeInningScores, SCORE_HOME_ADDR, SCORE_BYINNING_HOME_BASE, lines, "Home", SCORE_STRIDE);
    
    // 3. Count state (balls, strikes, outs)
    if (state.balls.has_value())
        lines.push_back(ToGeckoLine(0x00, BALLS_ADDR, state.balls.value()));

    if (state.strikes.has_value())
        lines.push_back(ToGeckoLine(0x00, STRIKES_ADDR, state.strikes.value()));

    if (state.outs.has_value())
        lines.push_back(ToGeckoLine(0x00, OUTS_ADDR, state.outs.value()));

    // 4. Ad-hoc "if codes" or special codes can be added here
    // lines.push_back(ToGeckoLine(0x04, some_address, some_value));

    // 5. Write all generated lines to the INI
    GeckoCodeGenerator::WriteGeneratedCode(game_id, code_name, lines);
}