#pragma once

#include <cstdint>
#include <array>
#include <utility>  // std::pair

// ============================================================
// Central constants file for Mario Superstar Baseball.
// All MSB files should pull shared UI/data constants from here.
// ============================================================

namespace MSB
{

// ── Characters ──────────────────────────────────────────────
// charID values as used by the game and MSB_Player::charID.
constexpr uint8_t CHAR_MARIO         = 0x00;
constexpr uint8_t CHAR_LUIGI         = 0x01;
constexpr uint8_t CHAR_DK            = 0x02;
constexpr uint8_t CHAR_DIDDY         = 0x03;
constexpr uint8_t CHAR_PEACH         = 0x04;
constexpr uint8_t CHAR_DAISY         = 0x05;
constexpr uint8_t CHAR_YOSHI         = 0x06;
constexpr uint8_t CHAR_BABY_MARIO    = 0x07;
constexpr uint8_t CHAR_BABY_LUIGI    = 0x08;
constexpr uint8_t CHAR_BOWSER        = 0x09;
constexpr uint8_t CHAR_WARIO         = 0x0A;
constexpr uint8_t CHAR_WALUIGI       = 0x0B;
constexpr uint8_t CHAR_KOOPA_G       = 0x0C;
constexpr uint8_t CHAR_TOAD_R        = 0x0D;
constexpr uint8_t CHAR_BOO           = 0x0E;
constexpr uint8_t CHAR_TOADETTE      = 0x0F;
constexpr uint8_t CHAR_SHY_GUY_R     = 0x10;
constexpr uint8_t CHAR_BIRDO         = 0x11;
constexpr uint8_t CHAR_MONTY         = 0x12;
constexpr uint8_t CHAR_BOWSER_JR     = 0x13;
constexpr uint8_t CHAR_PARATROOPA_R  = 0x14;
constexpr uint8_t CHAR_PIANTA_B      = 0x15;
constexpr uint8_t CHAR_PIANTA_R      = 0x16;
constexpr uint8_t CHAR_PIANTA_Y      = 0x17;
constexpr uint8_t CHAR_NOKI_B        = 0x18;
constexpr uint8_t CHAR_NOKI_R        = 0x19;
constexpr uint8_t CHAR_NOKI_G        = 0x1A;
constexpr uint8_t CHAR_BRO_H         = 0x1B;
constexpr uint8_t CHAR_TOADSWORTH    = 0x1C;
constexpr uint8_t CHAR_TOAD_B        = 0x1D;
constexpr uint8_t CHAR_TOAD_Y        = 0x1E;
constexpr uint8_t CHAR_TOAD_G        = 0x1F;
constexpr uint8_t CHAR_TOAD_P        = 0x20;
constexpr uint8_t CHAR_MAGIKOOPA_B   = 0x21;
constexpr uint8_t CHAR_MAGIKOOPA_R   = 0x22;
constexpr uint8_t CHAR_MAGIKOOPA_G   = 0x23;
constexpr uint8_t CHAR_MAGIKOOPA_Y   = 0x24;
constexpr uint8_t CHAR_KING_BOO      = 0x25;
constexpr uint8_t CHAR_PETEY         = 0x26;
constexpr uint8_t CHAR_DIXIE         = 0x27;
constexpr uint8_t CHAR_GOOMBA        = 0x28;
constexpr uint8_t CHAR_PARAGOOMBA    = 0x29;
constexpr uint8_t CHAR_KOOPA_R       = 0x2A;
constexpr uint8_t CHAR_PARATROOPA_G  = 0x2B;
constexpr uint8_t CHAR_SHY_GUY_B     = 0x2C;
constexpr uint8_t CHAR_SHY_GUY_Y     = 0x2D;
constexpr uint8_t CHAR_SHY_GUY_G     = 0x2E;
constexpr uint8_t CHAR_SHY_GUY_BK    = 0x2F;
constexpr uint8_t CHAR_DRY_BONES_GY  = 0x30;
constexpr uint8_t CHAR_DRY_BONES_G   = 0x31;
constexpr uint8_t CHAR_DRY_BONES_R   = 0x32;
constexpr uint8_t CHAR_DRY_BONES_B   = 0x33;
constexpr uint8_t CHAR_BRO_F         = 0x34;
constexpr uint8_t CHAR_BRO_B         = 0x35;

// Ordered (charID, display name) pairs for UI dropdowns.
// Names match MSB_StatTracker.h cCharIdToCharName exactly.
constexpr std::array<std::pair<uint8_t, const char*>, 54> CHAR_LIST = {{
    {CHAR_MARIO,        "Mario"},
    {CHAR_LUIGI,        "Luigi"},
    {CHAR_DK,           "DK"},
    {CHAR_DIDDY,        "Diddy"},
    {CHAR_PEACH,        "Peach"},
    {CHAR_DAISY,        "Daisy"},
    {CHAR_YOSHI,        "Yoshi"},
    {CHAR_BABY_MARIO,   "Baby Mario"},
    {CHAR_BABY_LUIGI,   "Baby Luigi"},
    {CHAR_BOWSER,       "Bowser"},
    {CHAR_WARIO,        "Wario"},
    {CHAR_WALUIGI,      "Waluigi"},
    {CHAR_KOOPA_G,      "Koopa(G)"},
    {CHAR_TOAD_R,       "Toad(R)"},
    {CHAR_BOO,          "Boo"},
    {CHAR_TOADETTE,     "Toadette"},
    {CHAR_SHY_GUY_R,    "Shy Guy(R)"},
    {CHAR_BIRDO,        "Birdo"},
    {CHAR_MONTY,        "Monty"},
    {CHAR_BOWSER_JR,    "Bowser Jr"},
    {CHAR_PARATROOPA_R, "Paratroopa(R)"},
    {CHAR_PIANTA_B,     "Pianta(B)"},
    {CHAR_PIANTA_R,     "Pianta(R)"},
    {CHAR_PIANTA_Y,     "Pianta(Y)"},
    {CHAR_NOKI_B,       "Noki(B)"},
    {CHAR_NOKI_R,       "Noki(R)"},
    {CHAR_NOKI_G,       "Noki(G)"},
    {CHAR_BRO_H,        "Bro(H)"},
    {CHAR_TOADSWORTH,   "Toadsworth"},
    {CHAR_TOAD_B,       "Toad(B)"},
    {CHAR_TOAD_Y,       "Toad(Y)"},
    {CHAR_TOAD_G,       "Toad(G)"},
    {CHAR_TOAD_P,       "Toad(P)"},
    {CHAR_MAGIKOOPA_B,  "Magikoopa(B)"},
    {CHAR_MAGIKOOPA_R,  "Magikoopa(R)"},
    {CHAR_MAGIKOOPA_G,  "Magikoopa(G)"},
    {CHAR_MAGIKOOPA_Y,  "Magikoopa(Y)"},
    {CHAR_KING_BOO,     "King Boo"},
    {CHAR_PETEY,        "Petey"},
    {CHAR_DIXIE,        "Dixie"},
    {CHAR_GOOMBA,       "Goomba"},
    {CHAR_PARAGOOMBA,   "Paragoomba"},
    {CHAR_KOOPA_R,      "Koopa(R)"},
    {CHAR_PARATROOPA_G, "Paratroopa(G)"},
    {CHAR_SHY_GUY_B,    "Shy Guy(B)"},
    {CHAR_SHY_GUY_Y,    "Shy Guy(Y)"},
    {CHAR_SHY_GUY_G,    "Shy Guy(G)"},
    {CHAR_SHY_GUY_BK,   "Shy Guy(Bk)"},
    {CHAR_DRY_BONES_GY, "Dry Bones(Gy)"},
    {CHAR_DRY_BONES_G,  "Dry Bones(G)"},
    {CHAR_DRY_BONES_R,  "Dry Bones(R)"},
    {CHAR_DRY_BONES_B,  "Dry Bones(B)"},
    {CHAR_BRO_F,        "Bro(F)"},
    {CHAR_BRO_B,        "Bro(B)"},
}};

// ── Fielding Positions ───────────────────────────────────────
// Index matches MSB_Player::position and MSB_Team array index.
constexpr uint8_t POS_PITCHER      = 0;
constexpr uint8_t POS_CATCHER      = 1;
constexpr uint8_t POS_FIRST_BASE   = 2;
constexpr uint8_t POS_SECOND_BASE  = 3;
constexpr uint8_t POS_THIRD_BASE   = 4;
constexpr uint8_t POS_SHORTSTOP    = 5;
constexpr uint8_t POS_LEFT_FIELD   = 6;
constexpr uint8_t POS_CENTER_FIELD = 7;
constexpr uint8_t POS_RIGHT_FIELD  = 8;

constexpr std::array<const char*, 9> POSITION_NAMES = {{
    "P", "C", "1B", "2B", "3B", "SS", "LF", "CF", "RF"
}};

// ── Handedness ───────────────────────────────────────────────
constexpr uint8_t HAND_RIGHT = 0;
constexpr uint8_t HAND_LEFT  = 1;

// ── Stadiums ─────────────────────────────────────────────────
// Values written to the stadium cursor address.
// Names match MSB_StatTracker.h cStadiumIdToStadiumName exactly.
constexpr std::array<std::pair<uint8_t, const char*>, 6> STADIUM_LIST = {{
    {0, "Mario Stadium"},
    {1, "Bowser Castle"},
    {2, "Wario Palace"},
    {3, "Yoshi Park"},
    {4, "Peach Garden"},
    {5, "DK Jungle"},
}};

}  // namespace MSB