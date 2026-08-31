#pragma once

#include "Core/GeckoCode.h"
#include <cstdint>
#include <optional>
#include <vector>

// Optional game state struct, filled from a HUD file (see MSB_HUDStateLoader.cpp).
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

    // The roster in the game's NATIVE draft-slot order (as the HUD's "Roster N"
    // is indexed), plus the fielding position each slot plays (the game's
    // positionSwapMapping). The game builds the on-field defense from these, so
    // they must be staged verbatim or the fielders land in the wrong spots.
    std::optional<uint8_t> rosterCharP1BySlot[9];
    std::optional<uint8_t> rosterCharP2BySlot[9];
    std::optional<uint8_t> positionByRosterSlotP1[9]; // 0=P 1=C 2=1B 3=2B 4=3B 5=SS 6=LF 7=CF 8=RF
    std::optional<uint8_t> positionByRosterSlotP2[9];

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
    std::optional<uint8_t> inningsSelected;

    std::optional<uint8_t> mercy; // 0=off, 1=on

    // Who is playing. isCpuMatch: 1 = P1 vs CPU. p2Port: physical controller
    // port of the second human, 1-based (2-4); ignored for CPU matches.
    std::optional<uint8_t> isCpuMatch;
    std::optional<uint8_t> p2Port;


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

    std::optional<uint32_t> logoP1; // 0-47
    std::optional<uint32_t> logoP2; // 0-47

    // for each spot in the batting order, enter the batters position.
    // NOTE: these are stored CURRENT-BATTER-FIRST (rotated so index 0 is the
    // batter who is up). Use the *BatterRosterLoc values to recover the natural
    // (leadoff-first) order and the current batter's slot.
    std::optional<uint32_t> awayPositionByBattingOrder[9];
    std::optional<uint32_t> homePositionByBattingOrder[9];

    // Current batter's index in the NATURAL (leadoff-first) batting order, 0-8.
    // The game wants the natural order plus this index, not the rotated order.
    std::optional<uint8_t> awayBatterRosterLoc;
    std::optional<uint8_t> homeBatterRosterLoc;

    // Runners on base (0 = 1B, 1 = 2B, 2 = 3B). runnerRosterSpot is the runner's
    // absolute natural batting slot (0 = leadoff), i.e. its index into the
    // batting team's in-memory roster.
    std::optional<uint16_t> runnerRosterSpot[3];
    std::optional<uint16_t> runnerCharacterID[3];

    // note the P1/P2 basis. Also, array based on batting order.
    std::optional<uint16_t> pitcherStaminaP1[9];
    std::optional<uint16_t> pitcherStaminaP2[9];
};

// Generates the "Boot To Match" gecko code: a single C0 (+ support C2s) code,
// compiled from ProjectRio-ASM "Gecko Codes/Global/Boot To Match.c" with
// CGecko, whose embedded BootMatchSpec/BootStateConfig payload is patched with
// the given game state. When active, the game boots directly from the main
// menu into the described match at the described game state.
class MSBQuickMatchCodeBuilder
{
public:
    static std::vector<Gecko::GeckoCode> MSB_GenerateQuickMatchSetupGeckoCode(const MSBQuickMatchGameState& state);
};

extern bool menuInputRestrictionEnabled; // set by the HUD loader. The boot-to-match flow no longer walks menus, so this only records whether the opponent is human.
