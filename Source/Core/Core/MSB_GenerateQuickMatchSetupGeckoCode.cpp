#include "MSB_GenerateQuickMatchSetupGeckoCode.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

#include "Common/Logging/Log.h"
#include "Core/GeckoCode.h"

bool menuInputRestrictionEnabled = true; // set by the HUD loader; see header.

namespace
{
// ============================================================================
// "Boot To Match" code template
//
// This is the compiled output of ProjectRio-ASM "Gecko Codes/Global/Boot To
// Match.c" (built with CGecko): a C0 that stages a full match from the main
// menu and force-swaps straight into it, plus two support C2s that fix
// duplicate-character model loading.
//
// The C0 embeds a BootMatchPayload data blob right after its PIC `bl`:
//     word  BOOT_PAYLOAD_MAGIC ('RIOB')
//     BootMatchSpec  spec      (payload offset 4)  - who is playing what match
//     BootStateConfig config   (payload offset 90) - the game state to land on
// The generator below locates the blob by its magic word and overwrites every
// spec/config field with the HUD file's game state.
//
// TO REGENERATE after editing Boot To Match.c: rebuild it with CGecko and
// paste the "$Boot To Match" block's hex here (two words per line). If the
// struct layouts changed, the PAYLOAD_* offsets below must be updated to
// match (they mirror BootMatchPayload; verify against the disassembly).
// ============================================================================
constexpr uint32_t BOOT_PAYLOAD_MAGIC = 0x52494F42; // 'RIOB'

constexpr uint32_t BOOT_TO_MATCH_TEMPLATE[] = {
    0xC0000000, 0x00000102, 0x7C0802A6, 0x90010004, 0x9421FF00, 0xBC610008,
    0x7C3E0B78, 0x480000E5, 0x52494F42, 0x09040904, 0x32111D01, 0x2A142104,
    0x040F1816, 0x0612100E, 0x00010001, 0x00010000, 0x00010001, 0x00000000,
    0x00000001, 0x00010001, 0x00000001, 0x00010000, 0x00000000, 0x01000000,
    0x01000000, 0x00000100, 0x00000000, 0x00010306, 0x050C0301, 0x01050100,
    0x02000103, 0x00000008, 0x00030302, 0x02050201, 0x01040F18, 0x16061204,
    0x100E0432, 0x11091D01, 0x2A142100, 0x02030405, 0x06010708, 0x01020300,
    0x04050607, 0x08010101, 0x00010000, 0x01000000, 0x01010000, 0x00000000,
    0x00010001, 0x00000100, 0x00000101, 0x00000000, 0x00000000, 0x00000101,
    0x00000000, 0x00000000, 0x00000100, 0x01000000, 0x00000000, 0x00000001,
    0x02030405, 0x06070800, 0x01020304, 0x05060708, 0x7FE802A6, 0x9421FFE0,
    0x93810010, 0x93A10014, 0x60000000, 0x3BBF0004, 0x7FBCEB78, 0x3BBD0056,
    0x3D20800E, 0x6129877C, 0xA9290000, 0x2C090004, 0x41820108, 0x893D0000,
    0x2C090000, 0x418200EC, 0x3D20800E, 0x6129877C, 0xA9290000, 0x2C090005,
    0x408200D8, 0x3D208089, 0x6129298C, 0x89290129, 0x2C090000, 0x418203E8,
    0x893D006B, 0x2C090000, 0x41820018, 0x3D20806C, 0x61299420, 0x3D40B065,
    0x614A0234, 0x91490000, 0x893D006C, 0x2C090000, 0x41820018, 0x3D20806C,
    0x61299450, 0x3D40B065, 0x614A00E0, 0x91490000, 0x893D006D, 0x2C090000,
    0x41820018, 0x3D20806C, 0x61299480, 0x3D40B065, 0x614A00E0, 0x91490000,
    0x3D20802E, 0x6129C01A, 0x89290000, 0x552A063E, 0x280A00BD, 0x4181004C,
    0x39290001, 0x3D40802E, 0x614AC01A, 0x992A0000, 0x5529063E, 0x280900B4,
    0x40810030, 0x3D208088, 0x6129A81B, 0x89290000, 0x2C090000, 0x408205E8,
    0x3D208089, 0x61292968, 0x895D000A, 0x91490008, 0x895D000A, 0x9149000C,
    0x83810010, 0x83A10014, 0x38210020, 0x480005D0, 0x7C0802A6, 0x90010024,
    0x9361000C, 0x3D20800E, 0x6129877C, 0x39400005, 0xB1490000, 0x3D208011,
    0x61291310, 0x39400001, 0xB1490000, 0x38C00000, 0x38A0003F, 0x38800040,
    0x386001BB, 0x3D20800C, 0x6129836C, 0x7D2903A6, 0x4E800421, 0x895C0053,
    0x714900FF, 0x4182028C, 0x39200000, 0x3D40800E, 0x614A86FC, 0x992A0010,
    0x3D408034, 0x614AE9A0, 0x992A46F8, 0x3940FFFF, 0x39000000, 0x3D208034,
    0x6129E9A0, 0x38E00000, 0x98E946FC, 0x994946FD, 0x3940FFFF, 0x994946FE,
    0x994946FF, 0x99094729, 0x3D40803C, 0x614A5EA9, 0x990A0000, 0x895C0000,
    0x914946E0, 0x895C0001, 0x914946E4, 0x39200000, 0x3CE08034, 0x60E7E9A0,
    0x39000000, 0x39400036, 0x7D4903A6, 0x7D474A14, 0x990A4757, 0x39290001,
    0x4200FFF4, 0x395C0001, 0x3CE08034, 0x60E7E9A0, 0x39000001, 0x39200009,
    0x7D2903A6, 0x8D2A0001, 0x7D274A14, 0x99094757, 0x4200FFF4, 0x395C000A,
    0x3CE08034, 0x60E7E9A0, 0x39000001, 0x39200009, 0x7D2903A6, 0x8D2A0001,
    0x7D274A14, 0x99094757, 0x4200FFF4, 0x39400000, 0x389C0002, 0x3CA0803C,
    0x60A56726, 0x38DD0074, 0x38E00001, 0x39200009, 0x7D2903A6, 0x7D0450AE,
    0x7CA92B78, 0x7D0951EE, 0x7D0650AE, 0x99090012, 0x98E90048, 0x394A0001,
    0x4200FFE4, 0x389C000B, 0x38BD007D, 0x39200000, 0x3CC0803C, 0x60C66726,
    0x38E00001, 0x39400009, 0x7D4903A6, 0x7D0448AE, 0x7D464A14, 0x990A0009,
    0x7D0548AE, 0x990A001B, 0x98EA0051, 0x39290001, 0x4200FFE4, 0x3D208064,
    0x612979D4, 0x7D2903A6, 0x4E800421, 0x3F608006, 0x637B78CC, 0x38600000,
    0x7F6903A6, 0x4E800421, 0x38600001, 0x7F6903A6, 0x4E800421, 0x3B7BD138,
    0x38600000, 0x7F6903A6, 0x4E800421, 0x38600001, 0x7F6903A6, 0x4E800421,
    0x3B7B13E8, 0x38600000, 0x7F6903A6, 0x4E800421, 0x38600001, 0x7F6903A6,
    0x4E800421, 0x3D208006, 0x61299854, 0x7D2903A6, 0x4E800421, 0x893C004E,
    0x3D40806D, 0x614A834C, 0x7D4A48AE, 0x3D20800E, 0x61298705, 0x99490000,
    0x895C004F, 0x39290005, 0x99490000, 0x895C0051, 0x3929004A, 0x99490000,
    0x895C0050, 0x39290004, 0x99490000, 0x893C0052, 0x712A00FF, 0x41820008,
    0x3920000A, 0x3D40800E, 0x614A8759, 0x992A0000, 0x3D208006, 0x6129496C,
    0x7D2903A6, 0x4E800421, 0x895C004A, 0x3D208034, 0x6129E9A0, 0x99494709,
    0x895C004B, 0x9949470A, 0x895C004C, 0x9949470D, 0x895C004D, 0x9949470E,
    0x3D20802E, 0x6129C01A, 0x39400000, 0x99490000, 0x8361000C, 0x80010024,
    0x7C0803A6, 0x4BFFFC20, 0x3D20800E, 0x612986FC, 0x39000001, 0x99090010,
    0x3D008034, 0x6108E9A0, 0x39200000, 0x992846F8, 0x893C0054, 0x3929FFFF,
    0x992846F9, 0x39000001, 0x4BFFFD6C, 0x3D208089, 0x612928A0, 0x895D0001,
    0x91490000, 0x895D0002, 0x994900AD, 0x3D008089, 0x6108298C, 0x554A063E,
    0x30EAFFFF, 0x7D475110, 0x9148000C, 0x895D0002, 0x7D4A0034, 0x554AD97E,
    0x91480010, 0xA15D0004, 0xB1490004, 0xA15D0004, 0xB1490006, 0xA15D0006,
    0xB149002A, 0xA15D0006, 0xB149002C, 0x392900C8, 0x895D0008, 0x91490004,
    0x895D0009, 0x91490000, 0x893D000B, 0x9928014A, 0x893D000C, 0x9928014B,
    0x893D000D, 0x9928014C, 0x893D000E, 0x2C090000, 0x40820184, 0x893D006B,
    0x2C090000, 0x41820030, 0x895D006E, 0x3D208088, 0x6129F04C, 0xB1490000,
    0x895D0071, 0x39290002, 0xB1490000, 0x3D20806C, 0x61299420, 0x3D406000,
    0x91490000, 0x893D006C, 0x2C090000, 0x41820030, 0x895D006F, 0x3D208088,
    0x6129F1A0, 0xB1490000, 0x895D0072, 0x39290002, 0xB1490000, 0x3D20806C,
    0x61299450, 0x3D406000, 0x91490000, 0x893D006D, 0x2C090000, 0x41A2FBCC,
    0x895D0070, 0x3D208088, 0x6129F2F4, 0xB1490000, 0x895D0073, 0x39290002,
    0xB1490000, 0x3D20806C, 0x61299480, 0x3D406000, 0x91490000, 0x4BFFFB9C,
    0x42400040, 0x7CE548AE, 0x7D264B78, 0x39290001, 0x39490010, 0x554A1838,
    0x7D485214, 0x90CA000C, 0x552A1838, 0x7D485214, 0x90EA0090, 0x2C070000,
    0x40A2FFD0, 0x90C8008C, 0x90880090, 0x4BFFFFC4, 0x7FAAEB78, 0x8D0A0034,
    0x3D208089, 0x6129298C, 0x910900E0, 0x20DDFFCC, 0x3CE08035, 0x60E73BE0,
    0x39200009, 0x7D2903A6, 0x7D265214, 0x8D0A0001, 0x1D2900A0, 0x7D274A14,
    0x99090026, 0x890A0012, 0x99090027, 0x890A0024, 0x99090005, 0x4200FFDC,
    0x395D003D, 0x39000000, 0x3CC08035, 0x60C63BE0, 0x39200009, 0x7D2903A6,
    0x8CEA0001, 0x1D2800A0, 0x7D264A14, 0x98E905C6, 0x88EA0012, 0x98E905C7,
    0x88EA0024, 0x98E905A5, 0x39080001, 0x4200FFDC, 0x4BFFFE84, 0x38BD0021,
    0x39200000, 0x3D008089, 0x6108298C, 0x38800000, 0x39400009, 0x7D4903A6,
    0x48000008, 0x42400040, 0x7CE548AE, 0x7D264B78, 0x39290001, 0x39490006,
    0x554A1838, 0x7D485214, 0x90CA000C, 0x552A1838, 0x7D485214, 0x90EA0040,
    0x2C070000, 0x40A2FFD0, 0x90C8003C, 0x90880040, 0x4BFFFFC4, 0x895D0033,
    0x3D208089, 0x6129298C, 0x914900DC, 0x38BD002A, 0x39200000, 0x3D008089,
    0x6108298C, 0x38800000, 0x39400009, 0x7D4903A6, 0x4BFFFEA4, 0x7D495378,
    0x3940FFBE, 0x99490000, 0x4BFFFA28, 0xB8610008, 0x80010104, 0x38210100,
    0x7C0803A6, 0x4E800020, 0xC20156B0, 0x00000009, 0x7C0802A6, 0x90010004,
    0x9421FF00, 0xBC610008, 0x7C3E0B78, 0x813E0010, 0x81290000, 0x2C090000,
    0x4082000C, 0x3D208037, 0x61290F1C, 0x913E0008, 0xB8610008, 0x80010104,
    0x38210100, 0x7C0803A6, 0x60000000, 0x00000000, 0xC20156E4, 0x0000000C,
    0x7C0802A6, 0x90010004, 0x9421FF00, 0xBC610008, 0x7C3E0B78, 0x815E0008,
    0x813E001C, 0x5529103A, 0x3D298037, 0x81291208, 0x2C0A0000, 0x41820010,
    0x394A0018, 0x912A0000, 0x48000010, 0x3D408037, 0x614A0F34, 0x4BFFFFF0,
    0xB8610008, 0x80010104, 0x38210100, 0x7C0803A6, 0x60000000, 0x00000000,
};

// ============================================================================
// BootMatchPayload byte layout. Mirrors the structs in Boot To Match.c —
// keep both sides in sync. All offsets are from the start of the blob (the
// magic word). Rosters/hands/superstar are [2][9]: team 0 = P1, team 1 = P2,
// slots in POSITION order (P, C, 1B, 2B, 3B, SS, LF, CF, RF).
// ============================================================================
// The C0 data blob, padded to a 4-byte boundary: 224 bytes / 56 words (matches
// the `bl` offset in the template: 0x48000001 | (224 + 4)).
constexpr size_t PAYLOAD_SIZE = 224;
constexpr size_t PAYLOAD_WORDS = PAYLOAD_SIZE / 4;

// BootMatchSpec (payload offset 4)
constexpr size_t SPEC_CAPTAIN = 4;            // byte[2], charID per team
constexpr size_t SPEC_ROSTER = 6;             // byte[2][9], charIDs
constexpr size_t SPEC_BATTING_HAND = 24;      // byte[2][9], 0=R 1=L
constexpr size_t SPEC_FIELDING_HAND = 42;     // byte[2][9], 0=R 1=L
constexpr size_t SPEC_SUPERSTAR = 60;         // byte[2][9], 0=off 1=on
constexpr size_t SPEC_CAPTAIN_ORDER_LOC = 78; // byte[2], captain's batting-order slot
constexpr size_t SPEC_LOGO = 80;              // byte[2], 0-0x2F
constexpr size_t SPEC_STADIUM_CURSOR = 82;    // stadium-select cursor index, 0-5
constexpr size_t SPEC_FIRST_BATTER = 83;      // 0 = P1 bats first (P1 away), 1 = P2
constexpr size_t SPEC_STAR_SKILLS = 84;       // 0=off 1=on
constexpr size_t SPEC_INNINGS = 85;           // actual inning count
constexpr size_t SPEC_MERCY = 86;             // 0=off 1=on
constexpr size_t SPEC_IS_CPU_MATCH = 87;      // 1 = P1 vs CPU
constexpr size_t SPEC_P2_PORT = 88;           // 1-based controller port of 2nd human

// BootStateConfig (payload offset 90; 89 is struct padding)
constexpr size_t CFG_APPLY_STATE = 90;  // 0 = fresh match, 1 = jump to state below
constexpr size_t CFG_INNING = 91;       // 1-based, keep <= SPEC_INNINGS
constexpr size_t CFG_BOTTOM = 92;       // 0=top 1=bottom (93 is struct padding)
constexpr size_t CFG_SCORE_AWAY = 94;   // u16 big-endian
constexpr size_t CFG_SCORE_HOME = 96;   // u16 big-endian
constexpr size_t CFG_BALLS = 98;
constexpr size_t CFG_STRIKES = 99;
constexpr size_t CFG_OUTS = 100;
constexpr size_t CFG_STARS_P1 = 101;
constexpr size_t CFG_STARS_P2 = 102;
constexpr size_t CFG_STAR_CHANCE = 103;

// Batting order + fielding positions (see Boot To Match.c BootStateConfig).
// Arrays are [2][9] in AWAY/HOME order (team 0 = away, 1 = home) and, within a
// team, CURRENT-BATTER-FIRST: index 0 is the batter who is/was up, 1 on deck.
constexpr size_t CFG_APPLY_ORDER = 104;   // 0 = leave the game's default order
constexpr size_t CFG_ORDER_CHAR = 105;    // byte[2][9] charID batting in each slot (natural)
constexpr size_t CFG_ORDER_POS = 123;     // byte[2][9] fielding position in each slot (natural)
constexpr size_t CFG_CURRENT_BATTER = 141; // byte[2] current batter slot, 1-indexed (away, home)
constexpr size_t CFG_ORDER_FIELDHAND = 143; // byte[2][9] P1/P2, natural order, 0=R 1=L
constexpr size_t CFG_ORDER_BATHAND = 161;   // byte[2][9] P1/P2, natural order, 0=R 1=L
constexpr size_t CFG_ORDER_SUPERSTAR = 179; // byte[2][9] P1/P2, natural order, 0=off 1=on
// Runners on base: 0 = 1B, 1 = 2B, 2 = 3B.
constexpr size_t CFG_RUNNER_PRESENT = 197;   // byte[3] 1 = base occupied
constexpr size_t CFG_RUNNER_ROSTERLOC = 200; // byte[3] runner's natural batting slot
constexpr size_t CFG_RUNNER_CHARID = 203;    // byte[3] runner's charID
// positionSwapMapping: byte[2][9] P1/P2, the fielding position each DRAFT-slot
// plays (paired with SPEC_ROSTER, which is also in draft order).
constexpr size_t CFG_POSITIONSWAP = 206;

using PayloadImage = std::array<uint8_t, PAYLOAD_SIZE>;

void WriteU16(PayloadImage& p, size_t offset, uint16_t value)
{
    p[offset] = static_cast<uint8_t>(value >> 8);
    p[offset + 1] = static_cast<uint8_t>(value & 0xFF);
}

// Clamp-with-warning helper for optional fields: absent -> fallback, out of
// range -> logged and clamped so a bad HUD value can't corrupt the payload.
uint8_t FieldOr(const char* name, std::optional<uint32_t> value, uint32_t maxValue, uint8_t fallback)
{
    if (!value.has_value())
        return fallback;
    if (value.value() > maxValue)
    {
        WARN_LOG_FMT(COMMON, "Boot To Match: {} = {} exceeds max {}; using {}.", name,
                     value.value(), maxValue, maxValue);
        return static_cast<uint8_t>(maxValue);
    }
    return static_cast<uint8_t>(value.value());
}

uint8_t FieldOr(const char* name, std::optional<uint16_t> value, uint32_t maxValue, uint8_t fallback)
{
    return FieldOr(name, value.has_value() ? std::optional<uint32_t>(value.value()) : std::nullopt,
                   maxValue, fallback);
}

uint8_t FieldOr(const char* name, std::optional<uint8_t> value, uint32_t maxValue, uint8_t fallback)
{
    return FieldOr(name, value.has_value() ? std::optional<uint32_t>(value.value()) : std::nullopt,
                   maxValue, fallback);
}

// Team score: prefer the HUD total; fall back to summing the per-inning box.
// The boot code books the whole total into inning 1's box score.
uint16_t TeamScore(const std::optional<uint16_t>& total,
                   const std::optional<uint16_t> inningScores[18])
{
    if (total.has_value())
        return total.value();
    uint16_t sum = 0;
    for (int i = 0; i < 18; i++)
        sum += inningScores[i].value_or(0);
    return sum;
}

// Un-rotate the HUD's current-batter-first order into the game's natural
// (leadoff-first) order. rotated[i] is the position batting i slots after the
// current batter; batterLoc is the current batter's natural slot. Result:
// natural[k] = position batting in natural slot k.
void NaturalOrder(const std::optional<uint32_t> rotated[9], uint8_t batterLoc, uint8_t natural[9])
{
    for (int k = 0; k < 9; k++)
    {
        const int i = (((k - batterLoc) % 9) + 9) % 9;
        natural[k] = static_cast<uint8_t>(rotated[i].value_or(0));
    }
}

// The captain's slot in a team's NATURAL batting order. Falls back to slot 3.
uint8_t CaptainOrderLoc(const char* teamName, const std::optional<uint8_t>& captainPosition,
                        const uint8_t naturalOrder[9])
{
    constexpr uint8_t FALLBACK = 3;
    if (!captainPosition.has_value())
    {
        WARN_LOG_FMT(COMMON, "Boot To Match: no captain position for {}; captain bats {}.",
                     teamName, FALLBACK + 1);
        return FALLBACK;
    }
    for (uint8_t orderLoc = 0; orderLoc < 9; orderLoc++)
        if (naturalOrder[orderLoc] == captainPosition.value())
            return orderLoc;
    WARN_LOG_FMT(COMMON,
                 "Boot To Match: captain position {} not found in {} batting order; captain bats {}.",
                 captainPosition.value(), teamName, FALLBACK + 1);
    return FALLBACK;
}

// Fill the payload from the HUD game state. Every field is written, so no
// placeholder value from the template can leak into the generated code.
bool BuildPayload(const MSBQuickMatchGameState& state, PayloadImage& payload)
{
    payload.fill(0);
    payload[0] = static_cast<uint8_t>(BOOT_PAYLOAD_MAGIC >> 24);
    payload[1] = static_cast<uint8_t>(BOOT_PAYLOAD_MAGIC >> 16);
    payload[2] = static_cast<uint8_t>(BOOT_PAYLOAD_MAGIC >> 8);
    payload[3] = static_cast<uint8_t>(BOOT_PAYLOAD_MAGIC);

    // ---- hard requirements: full rosters and both captains -----------------
    for (int i = 0; i < 9; i++)
    {
        if (!state.charactersP1ByPosition[i].has_value() ||
            !state.charactersP2ByPosition[i].has_value())
        {
            ERROR_LOG_FMT(COMMON, "Boot To Match: missing roster character at position {}; cannot build code.", i);
            return false;
        }
    }
    if (!state.captainCharacterP1.has_value() || !state.captainCharacterP2.has_value())
    {
        ERROR_LOG_FMT(COMMON, "Boot To Match: missing captain character(s); cannot build code.");
        return false;
    }

    // ---- who is P1: away or home? ------------------------------------------
    // The HUD loader encodes firstBatter as (p1IsAway ? halfInning : 1 - halfInning):
    // the player whose team is batting when play resumes. Invert that here to
    // recover the TRUE away assignment, which team-based mappings below need.
    if (!state.halfInning.has_value() || !state.firstBatter.has_value())
        WARN_LOG_FMT(COMMON, "Boot To Match: half inning / first batter missing; assuming P1 away, top of inning.");
    const uint8_t halfInning = FieldOr("halfInning", state.halfInning, 1, 0);
    const uint8_t firstBatter = FieldOr("firstBatter", state.firstBatter, 1, 0);
    const bool bottomOfInning = (halfInning == 1);
    const bool p1IsAway = !bottomOfInning ? (firstBatter == 0) : (firstBatter == 1);
    INFO_LOG_FMT(COMMON, "Boot To Match: P1 is {}.", p1IsAway ? "away" : "home");

    // Un-rotate the HUD's current-batter-first order into the game's natural
    // (leadoff-first) order for each team. The game stores the natural order in
    // the mapping and points currentBatter at the active slot; if we instead
    // fed the rotated order with currentBatter=1, the whole batting team ends
    // up shifted by however many batters have already hit this inning.
    const uint8_t awayBatterLoc = FieldOr("awayBatterRosterLoc", state.awayBatterRosterLoc, 8, 0);
    const uint8_t homeBatterLoc = FieldOr("homeBatterRosterLoc", state.homeBatterRosterLoc, 8, 0);
    uint8_t awayNatural[9];
    uint8_t homeNatural[9];
    NaturalOrder(state.awayPositionByBattingOrder, awayBatterLoc, awayNatural);
    NaturalOrder(state.homePositionByBattingOrder, homeBatterLoc, homeNatural);

    // ---- BootMatchSpec ------------------------------------------------------
    payload[SPEC_CAPTAIN + 0] = FieldOr("captainCharacterP1", state.captainCharacterP1, 0x35, 0);
    payload[SPEC_CAPTAIN + 1] = FieldOr("captainCharacterP2", state.captainCharacterP2, 0x35, 0);

    // The roster (rosterCharID) and positionSwapMapping are staged in the game's
    // native DRAFT-slot order so the on-field defense is built correctly. If the
    // HUD lacks the draft-slot data, fall back to position order + identity
    // positionSwap (fielders may be off but the batting order still works).
    bool draftComplete = true;
    for (int i = 0; i < 9; i++)
    {
        if (!state.rosterCharP1BySlot[i].has_value() || !state.rosterCharP2BySlot[i].has_value() ||
            !state.positionByRosterSlotP1[i].has_value() || !state.positionByRosterSlotP2[i].has_value())
        {
            draftComplete = false;
            break;
        }
    }
    if (!draftComplete)
        WARN_LOG_FMT(COMMON, "Boot To Match: HUD lacks draft-slot roster; fielders use position order.");

    for (int i = 0; i < 9; i++)
    {
        if (draftComplete)
        {
            payload[SPEC_ROSTER + i] = FieldOr("rosterCharP1", state.rosterCharP1BySlot[i], 0x35, 0);
            payload[SPEC_ROSTER + 9 + i] = FieldOr("rosterCharP2", state.rosterCharP2BySlot[i], 0x35, 0);
            payload[CFG_POSITIONSWAP + i] = FieldOr("positionSwapP1", state.positionByRosterSlotP1[i], 8, static_cast<uint8_t>(i));
            payload[CFG_POSITIONSWAP + 9 + i] = FieldOr("positionSwapP2", state.positionByRosterSlotP2[i], 8, static_cast<uint8_t>(i));
        }
        else
        {
            payload[SPEC_ROSTER + i] = FieldOr("characterP1", state.charactersP1ByPosition[i], 0x35, 0);
            payload[SPEC_ROSTER + 9 + i] = FieldOr("characterP2", state.charactersP2ByPosition[i], 0x35, 0);
            payload[CFG_POSITIONSWAP + i] = static_cast<uint8_t>(i);
            payload[CFG_POSITIONSWAP + 9 + i] = static_cast<uint8_t>(i);
        }

        // Handedness/superstar in the spec are by position; unused by the boot
        // (ApplyGameState applies them per batting slot) but kept for reference.
        payload[SPEC_BATTING_HAND + i] = FieldOr("battingHandP1", state.battingHandP1ByPosition[i], 1, 0);
        payload[SPEC_BATTING_HAND + 9 + i] = FieldOr("battingHandP2", state.battingHandP2ByPosition[i], 1, 0);
        payload[SPEC_FIELDING_HAND + i] = FieldOr("fieldingHandP1", state.fieldingHandP1ByPosition[i], 1, 0);
        payload[SPEC_FIELDING_HAND + 9 + i] = FieldOr("fieldingHandP2", state.fieldingHandP2ByPosition[i], 1, 0);
        payload[SPEC_SUPERSTAR + i] = FieldOr("superstarP1", state.superstarP1ByPosition[i], 1, 0);
        payload[SPEC_SUPERSTAR + 9 + i] = FieldOr("superstarP2", state.superstarP2ByPosition[i], 1, 0);
    }

    // P1's batting order is the away order when P1 is away, else the home order.
    payload[SPEC_CAPTAIN_ORDER_LOC + 0] =
        CaptainOrderLoc("P1", state.captainPositionP1, p1IsAway ? awayNatural : homeNatural);
    payload[SPEC_CAPTAIN_ORDER_LOC + 1] =
        CaptainOrderLoc("P2", state.captainPositionP2, p1IsAway ? homeNatural : awayNatural);

    // The game hardcodes "the first batter's team is the in-game away side and
    // bats when the match starts" -- ApplyGameState's battingTeam writes only
    // relabel the state, they don't flip who takes the field. So the display
    // fetches the away-row logo from the first batter's teamName slot, and for
    // a bottom-of-inning load (first batter = the true HOME player) the P1/P2
    // logo slots must swap so each scoreboard row still shows the true team's
    // logo. Same fix as the legacy menu-walk codes (PR #140).
    const uint8_t logoP1 = FieldOr("logoP1", state.logoP1, 0x2F, 0);
    const uint8_t logoP2 = FieldOr("logoP2", state.logoP2, 0x2F, 0);
    payload[SPEC_LOGO + 0] = bottomOfInning ? logoP2 : logoP1;
    payload[SPEC_LOGO + 1] = bottomOfInning ? logoP1 : logoP2;

    payload[SPEC_STADIUM_CURSOR] = FieldOr("stadium", state.stadium, 5, 0);
    // Who bats when play resumes: the away player for a top-half load, the
    // home player for a bottom-half load. This is exactly the HUD loader's
    // firstBatter encoding, so use it directly. Do NOT write the true away
    // assignment here -- that puts the wrong team at the plate for
    // bottom-of-inning loads.
    payload[SPEC_FIRST_BATTER] = firstBatter;
    payload[SPEC_STAR_SKILLS] = FieldOr("starSkills", state.starSkills, 1, 1);
    payload[SPEC_INNINGS] = FieldOr("inningsSelected", state.inningsSelected, 18, 9);
    payload[SPEC_MERCY] = FieldOr("mercy", state.mercy, 1, 0);

    const uint8_t isCpuMatch = FieldOr("isCpuMatch", state.isCpuMatch, 1, 0);
    if (isCpuMatch)
        WARN_LOG_FMT(COMMON, "Boot To Match: CPU-match boot path is untested in the force-swap flow.");
    payload[SPEC_IS_CPU_MATCH] = isCpuMatch;
    payload[SPEC_P2_PORT] = FieldOr("p2Port", state.p2Port, 4, 2);

    // ---- BootStateConfig ----------------------------------------------------
    // Only jump into a mid-game state when the HUD actually recorded one.
    const bool applyState = state.inning.has_value();
    payload[CFG_APPLY_STATE] = applyState ? 1 : 0;

    const uint8_t inning = FieldOr("inning", state.inning, 18, 1);
    if (inning > payload[SPEC_INNINGS])
        WARN_LOG_FMT(COMMON,
                     "Boot To Match: inning {} is past the {}-inning setting; booting into extras.",
                     inning, payload[SPEC_INNINGS]);
    payload[CFG_INNING] = inning;
    payload[CFG_BOTTOM] = halfInning;

    WriteU16(payload, CFG_SCORE_AWAY, TeamScore(state.awayScore, state.awayInningScores));
    WriteU16(payload, CFG_SCORE_HOME, TeamScore(state.homeScore, state.homeInningScores));

    payload[CFG_BALLS] = FieldOr("balls", state.balls, 3, 0);
    payload[CFG_STRIKES] = FieldOr("strikes", state.strikes, 2, 0);
    payload[CFG_OUTS] = FieldOr("outs", state.outs, 2, 0);
    payload[CFG_STARS_P1] = FieldOr("p1TeamStars", state.p1TeamStars, 5, 0);
    payload[CFG_STARS_P2] = FieldOr("p2TeamStars", state.p2TeamStars, 5, 0);
    payload[CFG_STAR_CHANCE] = FieldOr("isStarChance", state.isStarChance, 1, 0);

    // ---- batting order, fielding positions, current batter -----------------
    // Reproduce the HUD's real order. The mapping (positions + chars) and the
    // current batter are AWAY/HOME indexed; the handedness/superstar go into the
    // in-memory roster which is P1/P2 indexed. Everything uses the NATURAL
    // (leadoff-first) order, with currentBatter = batterLoc + 1.
    bool orderComplete = true;
    for (int i = 0; i < 9; i++)
    {
        if (!state.awayPositionByBattingOrder[i].has_value() ||
            !state.homePositionByBattingOrder[i].has_value())
        {
            orderComplete = false;
            break;
        }
    }
    if (orderComplete)
    {
        payload[CFG_APPLY_ORDER] = 1;

        // AWAY/HOME: mapping positions + chars, and the current batter slot.
        payload[CFG_CURRENT_BATTER + 0] = static_cast<uint8_t>(awayBatterLoc + 1);
        payload[CFG_CURRENT_BATTER + 1] = static_cast<uint8_t>(homeBatterLoc + 1);
        const std::optional<uint8_t>* awayChars =
            p1IsAway ? state.charactersP1ByPosition : state.charactersP2ByPosition;
        const std::optional<uint8_t>* homeChars =
            p1IsAway ? state.charactersP2ByPosition : state.charactersP1ByPosition;
        for (int team = 0; team < 2; team++) // 0 = away, 1 = home
        {
            const uint8_t* natural = (team == 0) ? awayNatural : homeNatural;
            const std::optional<uint8_t>* teamChars = (team == 0) ? awayChars : homeChars;
            for (int i = 0; i < 9; i++)
            {
                const uint8_t pos = natural[i];
                payload[CFG_ORDER_POS + team * 9 + i] = pos;
                payload[CFG_ORDER_CHAR + team * 9 + i] =
                    FieldOr("battingOrderChar", teamChars[pos], 0x35, 0);
            }
        }

        // P1/P2: handedness + superstar per natural batting slot. P1's natural
        // order is the away order when P1 is away, else the home order.
        for (int pteam = 0; pteam < 2; pteam++) // 0 = P1, 1 = P2
        {
            const bool teamIsAway = (pteam == 0) ? p1IsAway : !p1IsAway;
            const uint8_t* natural = teamIsAway ? awayNatural : homeNatural;
            const std::optional<uint8_t>* fieldHand =
                (pteam == 0) ? state.fieldingHandP1ByPosition : state.fieldingHandP2ByPosition;
            const std::optional<uint8_t>* batHand =
                (pteam == 0) ? state.battingHandP1ByPosition : state.battingHandP2ByPosition;
            const std::optional<uint8_t>* superstar =
                (pteam == 0) ? state.superstarP1ByPosition : state.superstarP2ByPosition;
            for (int i = 0; i < 9; i++)
            {
                const uint8_t pos = natural[i];
                payload[CFG_ORDER_FIELDHAND + pteam * 9 + i] =
                    FieldOr("orderFieldHand", fieldHand[pos], 1, 0);
                payload[CFG_ORDER_BATHAND + pteam * 9 + i] =
                    FieldOr("orderBatHand", batHand[pos], 1, 0);
                payload[CFG_ORDER_SUPERSTAR + pteam * 9 + i] =
                    FieldOr("orderSuperstar", superstar[pos], 1, 0);
            }
        }
    }
    else
    {
        payload[CFG_APPLY_ORDER] = 0;
        WARN_LOG_FMT(COMMON,
                     "Boot To Match: incomplete batting order in HUD; using the game's default order.");
    }

    // ---- runners on base ---------------------------------------------------
    for (int i = 0; i < 3; i++)
    {
        if (state.runnerRosterSpot[i].has_value() && state.runnerCharacterID[i].has_value())
        {
            payload[CFG_RUNNER_PRESENT + i] = 1;
            payload[CFG_RUNNER_ROSTERLOC + i] =
                FieldOr("runnerRosterLoc", state.runnerRosterSpot[i], 8, 0);
            payload[CFG_RUNNER_CHARID + i] =
                FieldOr("runnerCharID", state.runnerCharacterID[i], 0x35, 0);
        }
    }

    // ---- HUD state the boot code cannot represent --------------------------
    for (int i = 0; i < 9; i++)
    {
        if (state.pitcherStaminaP1[i].has_value() || state.pitcherStaminaP2[i].has_value())
        {
            WARN_LOG_FMT(COMMON, "Boot To Match: pitcher stamina is not supported; pitchers start fresh.");
            break;
        }
    }

    return true;
}

// Format two words as a gecko code line.
Gecko::GeckoCode::Code MakeLine(uint32_t w1, uint32_t w2)
{
    Gecko::GeckoCode::Code code;
    code.address = w1;
    code.data = w2;
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << w1 << " "
        << std::setw(8) << w2;
    code.original_line = oss.str();
    return code;
}

// Recover the true away assignment from the HUD loader's firstBatter encoding
// (firstBatter = who bats when play resumes: away in the top, home in the bottom).
bool ComputeP1IsAway(const MSBQuickMatchGameState& state)
{
    const uint8_t halfInning = static_cast<uint8_t>(state.halfInning.value_or(0));
    const uint8_t firstBatter = static_cast<uint8_t>(state.firstBatter.value_or(0));
    return (halfInning == 1) ? (firstBatter == 1) : (firstBatter == 0);
}

// The batting-order CHARACTER hook. The boot C0 sets each batting slot's
// fielding position (the mapping) and the current batter, but the CHARACTER at
// each slot comes from inMemRoster, which the game builds in its own default
// auto order. This C2 injects the HUD charIDs into the game's roster-build
// routine at 0x80066A48 (r23 = the build's scratch list) so inMemRoster is
// BUILT in the HUD batting order instead. Ported verbatim from the proven
// menu-walk generator; gated on rel==4 so the codehandler installs the hook
// before the boot C0's staging runs the build. Returned BEFORE the boot code
// so it is processed first. Returns nullopt if the HUD lacks a full order.
std::optional<Gecko::GeckoCode> BuildBattingOrderHookCode(const MSBQuickMatchGameState& state,
                                                          bool p1IsAway)
{
    for (int i = 0; i < 9; i++)
    {
        if (!state.awayPositionByBattingOrder[i].has_value() ||
            !state.homePositionByBattingOrder[i].has_value() ||
            !state.charactersP1ByPosition[i].has_value() ||
            !state.charactersP2ByPosition[i].has_value())
        {
            WARN_LOG_FMT(COMMON, "Boot To Match: incomplete batting order; skipping the char hook.");
            return std::nullopt;
        }
    }

    // Build inMemRoster in the NATURAL (leadoff-first) order, matching the boot
    // C0's mapping. P1's natural order is the away order when P1 is away.
    const uint8_t awayBatterLoc = static_cast<uint8_t>(state.awayBatterRosterLoc.value_or(0));
    const uint8_t homeBatterLoc = static_cast<uint8_t>(state.homeBatterRosterLoc.value_or(0));
    uint8_t awayNatural[9];
    uint8_t homeNatural[9];
    NaturalOrder(state.awayPositionByBattingOrder, awayBatterLoc, awayNatural);
    NaturalOrder(state.homePositionByBattingOrder, homeBatterLoc, homeNatural);
    const uint8_t* p1Natural = p1IsAway ? awayNatural : homeNatural;
    const uint8_t* p2Natural = p1IsAway ? homeNatural : awayNatural;

    std::vector<Gecko::GeckoCode::Code> codes;
    codes.push_back(MakeLine(0x280E877C, 0x00000004)); // if rel == 4 (main menu)

    codes.push_back(MakeLine(0xC2066A48, 0x00000016)); // C2 hook, 0x16 lines follow
    codes.push_back(MakeLine(0x3AE10038, 0x2C080001)); // is this team P2?
    codes.push_back(MakeLine(0x41820058, 0x60000000)); // branch to P2 block; nop

    for (int i = 0; i < 9; i++) // P1 batting order
    {
        const uint8_t charID = state.charactersP1ByPosition[p1Natural[i]].value();
        codes.push_back(MakeLine(0x39800000 | (charID & 0xFF), 0x99970000 | (i & 0xFF)));
    }
    codes.push_back(MakeLine(0x48000050, 0x60000000)); // branch to end; nop

    for (int i = 0; i < 9; i++) // P2 batting order
    {
        const uint8_t charID = state.charactersP2ByPosition[p2Natural[i]].value();
        codes.push_back(MakeLine(0x39800000 | (charID & 0xFF), 0x99970000 | (i & 0xFF)));
    }
    codes.push_back(MakeLine(0x60000000, 0x00000000)); // finish the injected block

    codes.push_back(MakeLine(0xE0000000, 0x80008000)); // end rel == 4 conditional

    Gecko::GeckoCode gc;
    gc.name = "Boot To Match Batting Order (HUD)";
    gc.enabled = true;
    gc.built_in_code = true;
    gc.user_defined = false;
    gc.codes = std::move(codes);
    return gc;
}
} // namespace

std::vector<Gecko::GeckoCode> MSBQuickMatchCodeBuilder::MSB_GenerateQuickMatchSetupGeckoCode(
    const MSBQuickMatchGameState& state)
{
    INFO_LOG_FMT(COMMON, "Generating Boot To Match gecko code from HUD state");

    PayloadImage payload;
    if (!BuildPayload(state, payload))
        return {};

    // ---- locate the payload blob in the template by its magic word ---------
    constexpr size_t templateWords = sizeof(BOOT_TO_MATCH_TEMPLATE) / sizeof(uint32_t);
    static_assert(templateWords % 2 == 0, "template must be whole gecko lines");

    size_t magicIndex = 0;
    int magicCount = 0;
    for (size_t i = 0; i < templateWords; i++)
    {
        if (BOOT_TO_MATCH_TEMPLATE[i] == BOOT_PAYLOAD_MAGIC)
        {
            magicIndex = i;
            magicCount++;
        }
    }
    // The blob sits right after the C0's PIC `bl`, whose branch offset is the
    // payload size plus the trailing `mflr r31`. Checking both properties
    // guarantees the magic word found really is the payload and not a
    // coincidental instruction encoding.
    constexpr uint32_t expectedBl = 0x48000001 | ((PAYLOAD_SIZE + 4) & 0x03FFFFFC);
    if (magicCount != 1 || magicIndex == 0 ||
        BOOT_TO_MATCH_TEMPLATE[magicIndex - 1] != expectedBl ||
        magicIndex + PAYLOAD_WORDS > templateWords)
    {
        ERROR_LOG_FMT(COMMON,
                      "Boot To Match: template/payload mismatch (magic count {}, index {}). "
                      "Was the template refreshed after a Boot To Match.c change?",
                      magicCount, magicIndex);
        return {};
    }

    // ---- patch the payload into the template --------------------------------
    std::vector<uint32_t> words(BOOT_TO_MATCH_TEMPLATE, BOOT_TO_MATCH_TEMPLATE + templateWords);
    for (size_t w = 0; w < PAYLOAD_WORDS; w++)
    {
        const size_t o = w * 4;
        words[magicIndex + w] = (static_cast<uint32_t>(payload[o]) << 24) |
                                (static_cast<uint32_t>(payload[o + 1]) << 16) |
                                (static_cast<uint32_t>(payload[o + 2]) << 8) |
                                static_cast<uint32_t>(payload[o + 3]);
    }

    // ---- emit as one gecko code --------------------------------------------
    Gecko::GeckoCode geckoCode;
    geckoCode.name = "Boot To Match (HUD)";
    geckoCode.enabled = true;
    geckoCode.built_in_code = true;
    geckoCode.user_defined = false;

    for (size_t i = 0; i < words.size(); i += 2)
    {
        Gecko::GeckoCode::Code code;
        code.address = words[i];
        code.data = words[i + 1];

        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        oss << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << words[i] << " "
            << std::setw(8) << words[i + 1];
        code.original_line = oss.str();

        geckoCode.codes.push_back(std::move(code));
    }

    INFO_LOG_FMT(COMMON, "Boot To Match code generated: {} lines", geckoCode.codes.size());

    // The char hook must be processed before the boot C0 so the codehandler
    // installs the 0x80066A48 branch before the C0's staging runs the roster
    // build. Emit it first.
    std::vector<Gecko::GeckoCode> result;
    if (auto hookCode = BuildBattingOrderHookCode(state, ComputeP1IsAway(state)))
    {
        INFO_LOG_FMT(COMMON, "Boot To Match batting-order hook generated: {} lines",
                     hookCode->codes.size());
        result.push_back(std::move(*hookCode));
    }
    result.push_back(std::move(geckoCode));
    return result;
}
