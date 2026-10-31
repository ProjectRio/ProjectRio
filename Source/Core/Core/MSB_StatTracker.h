#pragma once

#include <string>
#include <array>
#include <vector>
#include <map>
#include <set>
#include <tuple>
#include <iostream>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "Core/HW/Memmap.h"
#include <picojson.h>

#include "Common/HttpRequest.h"

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"

#include "Core/LocalPlayers.h"
#include "Core/Logger.h"
#include "Core/TrackerAdr.h"
#include "Core/GeckoCodeConfig.h"

namespace Tag {
class TagSet;
}

enum class GAME_STATE
{
  PREGAME,
  INGAME,
  ENDGAME_LOGGED,
  UNDEFINED
};

static std::map<GAME_STATE, std::string> c_game_state = {
    {GAME_STATE::PREGAME, "PREGAME"},
    {GAME_STATE::INGAME, "INGAME"},
    {GAME_STATE::ENDGAME_LOGGED, "ENDGAME_LOGGED"},
    {GAME_STATE::UNDEFINED, "UNDEFINED"}
};

enum class EVENT_STATE
{
    INIT_EVENT,
    PITCH_RESULT,
    CONTACT_RESULT,
    MONITOR_RUNNERS,
    PLAY_OVER,
    FINAL_RESULT,
    WAITING_FOR_EVENT,
    GAME_OVER,
    UNDEFINED
};

enum class DEAD_BALL_REASON
{
    N_A,
    HOME_RUN,
    FOUL_BALL,
    GROUND_RULE_DOUBLE,
    BALL_DEAD
};

enum class SPECIAL_LIVE_BALL_REASON : u8
{
    NONE    = 0,
    PICKOFF = 1,
    STEAL   = 2
};

// Runner.outType in mssb-dtk
enum class OUT_TYPE : u8
{
    NONE             = 0x00,
    CAUGHT           = 0x01, // Batter only
    FORCE            = 0x02,
    TAG              = 0x03,
    WHILE_TAGGING_UP = 0x04, // Failed to return after a catch
    STRIKEOUT        = 0x10  // Set by the stat tracker, never written by the game
};

// AT_BAT_RESULT in mssb-dtk
// OUT_ON_PLAY and FIELDING_ERROR avoid the OUT and ERROR macros in the Windows headers
enum class AT_BAT_RESULT : u8
{
    NONE              = 0x00,
    STRIKEOUT         = 0x01,
    WALK              = 0x02,
    HIT_BY_PITCH      = 0x03,
    OUT_ON_PLAY       = 0x04,
    CAUGHT_LINE_DRIVE = 0x05,
    CAUGHT            = 0x06,
    SINGLE            = 0x07,
    DOUBLE            = 0x08,
    TRIPLE            = 0x09,
    HOME_RUN          = 0x0A,
    FIELDING_ERROR    = 0x0B,
    FIELDERS_CHOICE   = 0x0C,
    BUNT              = 0x0D, // Successful sacrifice-style bunt, the batter may still be out
    SAC_FLY           = 0x0E,
    DOUBLE_PLAY       = 0x0F, // Ball not caught, includes triple plays
    FOUL_CATCH        = 0x10  // AT_BAT_RESULT_CAUGHT_INFIELD_FLY in mssb-dtk, but only set for foul balls
};

// BALL_RESULT_TYPE in mssb-dtk. The game stores this as an s16, we read the low byte
enum class BALL_RESULT_TYPE : u8
{
    IN_AIR  = 0x00,
    LANDED  = 0x01,
    FIELDED = 0x02,
    CAUGHT  = 0x03,
    FOUL    = 0xFF
};

// Tracker only, not game values. Decoded by cPrimaryContactResult
// CAUGHT avoids the OUT macro in the Windows headers
enum class PRIMARY_CONTACT_RESULT : u8
{
    CAUGHT  = 0x00,
    FOUL    = 0x01,
    FAIR    = 0x02,
    FIELDED = 0x03
};

// Tracker only, not game values. Decoded by cSecondaryContactResult
// 0x07 to 0x0F are also used, copied directly from AT_BAT_RESULT
enum class SECONDARY_CONTACT_RESULT : u8
{
    OUT_CAUGHT             = 0x00,
    OUT_FORCE              = 0x01,
    OUT_TAG                = 0x02,
    FOUL                   = 0x03,
    BATTER_SAFE_RUNNER_OUT = 0x04,
    NONE                   = 0xFF
};

// From Nuche
enum class GAME_CONTROL_STATE : u8
{
    DEFAULT                              = 0x0,
    AT_BAT                               = 0x1,
    LIVE_BALL                            = 0x2,
    INNING_TRANSITION                    = 0x3,
    LOAD_GAME                            = 0x4,
    GAME_START_MOVIE                     = 0x5,
    TRANSITION_TO_MINIGAME_START         = 0x6,
    TRANSITION_PREPARE_NEXT_PLAY         = 0x7,
    TRANSITION                           = 0x8,
    END_OF_GAME                          = 0x9,
    PAUSED                               = 0xb,
    HOW_TO_PLAY_SCREEN                   = 0xd,
    MVP_END_GAME_SCREEN                  = 0xe,
    MINIGAME_POST_GAME_TRANSITION        = 0xf,
    HOME_RUN_END                         = 0x13,
    HOME_RUN_LAP                         = 0x14,
    POST_REPLAY_BATTER_CELEBRATION       = 0x15,
    STAR_CHANCE_VS_SCREEN                = 0x16,
    CHAMPIONSHIP_SCREEN                  = 0x17,
    MINIGAME_NEW_ROUND                   = 0x19,
    MINIGAME_TRANSITION_TO_BATTING_1     = 0x1a,
    MINIGAME_SELECT_SCREEN               = 0x1c,
    TOY_FIELD_STADIUM_LOAD_SCREEN        = 0x1d,
    CHARACTER_SELECT_MINIGAME_TOY_FIELD  = 0x1e,
    READY_MINIGAME_SCREEN                = 0x21,
    POST_MINIGAME_MENU                   = 0x22
};

static std::map<EVENT_STATE, std::string> c_event_state = {
    {EVENT_STATE::INIT_EVENT, "INIT_EVENT"},
    {EVENT_STATE::PITCH_RESULT, "PITCH_RESULT"},
    {EVENT_STATE::CONTACT_RESULT, "CONTACT_RESULT"},
    {EVENT_STATE::MONITOR_RUNNERS, "MONITOR_RUNNERS"},
    {EVENT_STATE::PLAY_OVER, "PLAY_OVER"},
    {EVENT_STATE::FINAL_RESULT, "FINAL_RESULT"},
    {EVENT_STATE::WAITING_FOR_EVENT, "WAITING_FOR_EVENT"},
    {EVENT_STATE::GAME_OVER, "GAME_OVER"},
    {EVENT_STATE::UNDEFINED, "UNDEFINED"}
};



//Conversion Maps

static const std::map<u8, std::string> cCharIdToCharName = {
    {0x0, "Mario"},
    {0x1, "Luigi"},
    {0x2, "DK"},
    {0x3, "Diddy"},
    {0x4, "Peach"},
    {0x5, "Daisy"},
    {0x6, "Yoshi"},
    {0x7, "Baby Mario"},
    {0x8, "Baby Luigi"},
    {0x9, "Bowser"},
    {0xa, "Wario"},
    {0xb, "Waluigi"},
    {0xc, "Koopa(G)"},
    {0xd, "Toad(R)"},
    {0xe, "Boo"},
    {0xf, "Toadette"},
    {0x10, "Shy Guy(R)"},
    {0x11, "Birdo"},
    {0x12, "Monty"},
    {0x13, "Bowser Jr"},
    {0x14, "Paratroopa(R)"},
    {0x15, "Pianta(B)"},
    {0x16, "Pianta(R)"},
    {0x17, "Pianta(Y)"},
    {0x18, "Noki(B)"},
    {0x19, "Noki(R)"},
    {0x1a, "Noki(G)"},
    {0x1b, "Bro(H)"},
    {0x1c, "Toadsworth"},
    {0x1d, "Toad(B)"},
    {0x1e, "Toad(Y)"},
    {0x1f, "Toad(G)"},
    {0x20, "Toad(P)"},
    {0x21, "Magikoopa(B)"},
    {0x22, "Magikoopa(R)"},
    {0x23, "Magikoopa(G)"},
    {0x24, "Magikoopa(Y)"},
    {0x25, "King Boo"},
    {0x26, "Petey"},
    {0x27, "Dixie"},
    {0x28, "Goomba"},
    {0x29, "Paragoomba"},
    {0x2a, "Koopa(R)"},
    {0x2b, "Paratroopa(G)"},
    {0x2c, "Shy Guy(B)"},
    {0x2d, "Shy Guy(Y)"},
    {0x2e, "Shy Guy(G)"},
    {0x2f, "Shy Guy(Bk)"},
    {0x30, "Dry Bones(Gy)"},
    {0x31, "Dry Bones(G)"},
    {0x32, "Dry Bones(R)"},
    {0x33, "Dry Bones(B)"},
    {0x34, "Bro(F)"},
    {0x35, "Bro(B)"}
};

static const std::set<u8> cCaptainTypeCharIds = {
    0x0,  // Mario
    0x1,  // Luigi
    0x2,  // DK
    0x3,  // Diddy
    0x4,  // Peach
    0x5,  // Daisy
    0x6,  // Yoshi
    0x9,  // Bowser
    0xa,  // Wario
    0xb,  // Waluigi
    0x11, // Birdo
    0x13, // Bowser Jr
};

static const std::map<u8, std::string> cStadiumIdToStadiumName = {
    {0x0, "Mario Stadium"},
    {0x1, "Bowser Castle"},
    {0x2, "Wario Palace"},
    {0x3, "Yoshi Park"},
    {0x4, "Peach Garden"},
    {0x5, "DK Jungle"},
    {0x6, "Toy Field"}
};

static const std::map<u32, std::string> cLogoIdToTeamName = {
    {0x0,  "Mario Sunshines"},
    {0x1,  "Mario All Stars"},
    {0x2,  "Mario Fireballs"},
    {0x3,  "Mario Heroes"},
    {0x4,  "Luigi Mansioneers"},
    {0x5,  "Luigi Leapers"},
    {0x6,  "Luigi Vacuums"},
    {0x7,  "Luigi Gentlemen"},
    {0x8,  "Peach Monarchs"},
    {0x9,  "Peach Princesses"},
    {0xA,  "Peach Dynasties"},
    {0xB,  "Peach Roses"},
    {0xC,  "Daisy Queen Bees"},
    {0xD,  "Daisy Petals"},
    {0xE,  "Daisy Cupids"},
    {0xF,  "Daisy Lillies"},
    {0x10, "Yoshi Islanders"},
    {0x11, "Yoshi Flutters"},
    {0x12, "Yoshi Speed Stars"},
    {0x13, "Yoshi Eggs"},
    {0x14, "Birdo Bows"},
    {0x15, "Birdo Fans"},
    {0x16, "Birdo Models"},
    {0x17, "Birdo Beauties"},
    {0x18, "Wario Greats"},
    {0x19, "Wario Beasts"},
    {0x1A, "Wario Steakheads"},
    {0x1B, "Wario Garlics"},
    {0x1C, "Waluigi Flankers"},
    {0x1D, "Waluigi Mashers"},
    {0x1E, "Waluigi Smart Alecks"},
    {0x1F, "Waluigi Mystiques"},
    {0x20, "DK Kongs"},
    {0x21, "DK Animals"},
    {0x22, "DK Wild Ones"},
    {0x23, "DK Explorers"},
    {0x24, "Diddy Tails"},
    {0x25, "Diddy Red Caps"},
    {0x26, "Diddy Ninjas"},
    {0x27, "Diddy Survivors"},
    {0x28, "Bowser Monsters"},
    {0x29, "Bowser Black Stars"},
    {0x2A, "Bowser Blue Shells"},
    {0x2B, "Bowser Flames"},
    {0x2C, "Jr Pixies"},
    {0x2D, "Jr Rookies"},
    {0x2E, "Jr Bombers"},
    {0x2F, "Jr Fangs"},
};

static const std::map<u8, std::string> cTypeOfContactToHR = {
    {0xFF, "Miss"},
    {0, "Sour - Left"},
    {1, "Nice - Left"}, 
    {2, "Perfect"},
    {3, "Nice - Right"}, 
    {4, "Sour - Right"}
};

static const std::map<u8, std::string> cHandToHR = {
    {0, "Right"},
    {1, "Left"}
};

static const std::map<u8, std::string> cInputDirectionToHR = {
    {0, "None"},
    {1, "Towards Batter"},
    {2, "Away From Batter"}
};

static const std::map<u8, std::string> cPitchTypeToHR = {
    {0, "Curve"},
    {1, "Charge"},
    {2, "ChangeUp"}
};

static const std::map<u8, std::string> cChargePitchTypeToHR = {
    {0, "N/A"},
    {1, "Charged Captain Star"},
    {2, "Slider"},
    {3, "Perfect"}
};

static const std::map<u8, std::string> cTypeOfSwing = {
    {0, "None"},
    {1, "Slap"},
    {2, "Charge"},
    {3, "Star"},
    {4, "Bunt"}
};

static const std::map<u8, std::string> cPosition = {
    {0, "P"},
    {1, "C"},
    {2, "1B"},
    {3, "2B"},
    {4, "3B"},
    {5, "SS"},
    {6, "LF"},
    {7, "CF"},
    {8, "RF"},
    {0xFF, "Inv"}
};

static const std::map<u8, std::string> cFielderActions = {
    {0, "None"},
    {2, "Sliding"},
    {3, "Walljump"},
};

static const std::map<u8, std::string> cFielderBobbles = {
    {0, "None"},
    {1, "Slide/stun lock"},
    {2, "Fumble"},
    {3, "Bobble"},
    {4, "Fireball"},
    {0x10, "Garlic knockout"},
    {0xFF, "None"}
};

static const std::map<u8, std::string> cStealType = {
    {0, "None"},
    {1, "Ready"},
    {2, "Normal"},
    {3, "Perfect"},
    {0xFF, "None"}
};

static const std::map<u8, std::string> cOutType = {
    {0, "None"},
    {1, "Caught"},
    {2, "Force"},
    {3, "Tag"},
    {4, "Force Back"},
    {0x10, "Strike-out"},
};

static const std::map<u8, std::string> cPitchResult = {
    {0, "HBP"},
    {1, "BB"},
    {2, "Ball"},
    {3, "Strike-looking"},
    {4, "Strike-swing"},
    {5, "Strike-bunting"},
    {6, "Contact"},
    {7, "Unknown"}
};

static const std::map<u8, std::string> cPrimaryContactResult = {
    {0, "Out"},
    {1, "Foul"},
    {2, "Fair"},
    {3, "Fielded"},
    {4, "Unknown"},
};

static const std::map<u8, std::string> cSecondaryContactResult = {
    {0x0,  "Out-caught"},
    {0x1,  "Out-force"},
    {0x2,  "Out-tag"},
    {0x3,  "Foul"},
    {0x4,  "Batter safe, runner out"},
    {0x7,  "Single"},
    {0x8,  "Double"},
    {0x9,  "Triple"},
    {0xA,  "HR"},
    {0xB,  "Error"},
    {0xC,  "Fielders Choice"},
    {0xD,  "Bunt"},
    {0xE,  "SacFly"},
    {0xF,  "Ground ball double Play"},
    {0xFF, "None"}
};

static const std::map<u8, std::string> cAtBatResult = {
    {0x0,  "None"},
    {0x1,  "Strikeout"},
    {0x2,  "Walk (BB)"},
    {0x3,  "Walk (HBP)"},
    {0x4,  "Out"},
    {0x5,  "Caught line-drive"},
    {0x6,  "Caught"},
    {0x7,  "Single"},
    {0x8,  "Double"},
    {0x9,  "Triple"},
    {0xA,  "HR"},
    {0xB,  "Error"},
    {0xC,  "Fielders Choice"},
    {0xD,  "Bunt"},
    {0xE,  "SacFly"},
    {0xF,  "Ground ball double Play"},
    {0x10, "Foul catch"}
};

static const std::map<u8, std::string> cDeadBallReason = {
    {0x0,  "N/A"},
    {0x1,  "Home Run"},
    {0x2,  "Foul Ball"},
    {0x3,  "Ground Rule Double"},
    {0x4,  "Ball Dead"}
};

static const std::map<u8, std::string> cManualSelectDecode = {
    {0x0,  "No Selected Char"},
    {0x1,  "Pitcher"},
    {0x2,  "Catcher"},
    {0x3,  "Closest to Ball"},
    {0x4,  "Closest to Drop"},
};

static const std::map<GAME_CONTROL_STATE, std::string> cGameControlState = {
    {GAME_CONTROL_STATE::DEFAULT,                              "DEFAULT"},
    {GAME_CONTROL_STATE::AT_BAT,                               "AT_BAT"},
    {GAME_CONTROL_STATE::LIVE_BALL,                            "LIVE_BALL"},
    {GAME_CONTROL_STATE::INNING_TRANSITION,                    "INNING_TRANSITION"},
    {GAME_CONTROL_STATE::LOAD_GAME,                            "LOAD_GAME"},
    {GAME_CONTROL_STATE::GAME_START_MOVIE,                     "GAME_START_MOVIE"},
    {GAME_CONTROL_STATE::TRANSITION_TO_MINIGAME_START,         "TRANSITION_TO_MINIGAME_START"},
    {GAME_CONTROL_STATE::TRANSITION_PREPARE_NEXT_PLAY,         "TRANSITION_PREPARE_NEXT_PLAY"},
    {GAME_CONTROL_STATE::TRANSITION,                           "TRANSITION"},
    {GAME_CONTROL_STATE::END_OF_GAME,                          "END_OF_GAME"},
    {GAME_CONTROL_STATE::PAUSED,                               "PAUSED"},
    {GAME_CONTROL_STATE::HOW_TO_PLAY_SCREEN,                   "HOW_TO_PLAY_SCREEN"},
    {GAME_CONTROL_STATE::MVP_END_GAME_SCREEN,                  "MVP_END_GAME_SCREEN"},
    {GAME_CONTROL_STATE::MINIGAME_POST_GAME_TRANSITION,        "MINIGAME_POST_GAME_TRANSITION"},
    {GAME_CONTROL_STATE::HOME_RUN_END,                         "HOME_RUN_END"},
    {GAME_CONTROL_STATE::HOME_RUN_LAP,                         "HOME_RUN_LAP"},
    {GAME_CONTROL_STATE::POST_REPLAY_BATTER_CELEBRATION,       "POST_REPLAY_BATTER_CELEBRATION"},
    {GAME_CONTROL_STATE::STAR_CHANCE_VS_SCREEN,                "STAR_CHANCE_VS_SCREEN"},
    {GAME_CONTROL_STATE::CHAMPIONSHIP_SCREEN,                  "CHAMPIONSHIP_SCREEN"},
    {GAME_CONTROL_STATE::MINIGAME_NEW_ROUND,                   "MINIGAME_NEW_ROUND"},
    {GAME_CONTROL_STATE::MINIGAME_TRANSITION_TO_BATTING_1,     "MINIGAME_TRANSITION_TO_BATTING_1"},
    {GAME_CONTROL_STATE::MINIGAME_SELECT_SCREEN,               "MINIGAME_SELECT_SCREEN"},
    {GAME_CONTROL_STATE::TOY_FIELD_STADIUM_LOAD_SCREEN,        "TOY_FIELD_STADIUM_LOAD_SCREEN"},
    {GAME_CONTROL_STATE::CHARACTER_SELECT_MINIGAME_TOY_FIELD,  "CHARACTER_SELECT_MINIGAME_TOY_FIELD"},
    {GAME_CONTROL_STATE::READY_MINIGAME_SCREEN,                "READY_MINIGAME_SCREEN"},
    {GAME_CONTROL_STATE::POST_MINIGAME_MENU,                   "POST_MINIGAME_MENU"},
};

//Const for structs
static const int cRosterSize = 9;
static const int cNumOfTeams = 2;
static const int cNumOfPositions = 9;

//Addrs for triggering evts
static const u32 aEndOfGameFlag    = 0x80892AB3;
static const u32 aPausedBy         = 0x8039D7D3; //who opened the pause menu: 1 == batting team, 2 == fielding team
static const u32 aGameControlStateCurr = 0x80892aaa;
static const u32 aGameControlStatePrev = 0x80892aab;
static const u32 aLoadedRel        = 0x800E877C; //(halfword) 5 == game rel is loaded
static const u32 aGameHasStarted   = 0x80892AB5; //set by the game at the first at bat. Only valid while the game rel is loaded
static const u32 aFramesExitingToMenu = 0x80892AAE; //counts up while the game fades out of the match to the main menu; 0 otherwise
static const u32 aGameTypeSelected = 0x800E8703; //4 == title screen demo

static const u32 aAB_PitchThrown            = 0x8088A81B; // 1 when windup starts, cleared when pitch over or steal attempt completed.
static const u32 aAB_ContactResult          = 0x808926B3; //BALL_RESULT_TYPE
static const u32 aAB_ContactMade            = 0x808909a1; //Boolean, from Roeming
static const u32 aAB_SpecialLiveBallReason  = 0x80892857; //0=None, 1=Pickoff, 2=Steal

static const u32 aAB_GameIsLive  = 0x8036F3A9; //0 at beginning of game and inbetween innings/changes
static const u32 aAB_PlayIsReadyToStart  = 0x808909AA; //Key addr that tells us when all addrs have been initialized for the play
static const u32 aAB_IsReplay  = 0x80872540;

//Addrs for GameInfo
static const u32 aStadiumId = 0x800E8705;

static const u32 aTeam0_Captain = 0x80353083;
static const u32 aTeam1_Captain = 0x80353087;

static const u32 aAway_Logo = 0x808929b0;
static const u32 aHome_Logo = 0x808929bc;

static const u32 aTeam0_Captain_Roster_Loc = 0x80892A83;
static const u32 aTeam1_Captain_Roster_Loc = 0x80892A87;

static const u32 aAwayTeam_Score = 0x808928A4;
static const u32 aHomeTeam_Score = 0x808928CA;

static const u32 aInningsSelected = 0x8089294A;
static const u32 aFirstBattingTeam = 0x803c5f40;
static const u32 aStarSkillsOn = 0x803c5f41;
static const u32 aMercyOn = 0x803c5f43;

static const u8 c_roster_table_offset = 0xa0;

static const u32 aInGame_CharAttributes_CharId       = 0x80353C05;
static const u32 aInGame_CharAttributes_FieldingHand = 0x80353C06;
static const u32 aInGame_CharAttributes_BattingHand  = 0x80353C07;

static const u32 aPlayer1Port                        = 0x800E874C;
static const u32 aPlayer2Port                        = 0x800E874D;
static const u32 aBattingTeam_P1P2                   = 0x80892990;
static const u32 aFieldingTeam_P1P2                  = 0x80892994;

//Addrs for DefensiveStats
static const u32 aPitcher_BattersFaced      = 0x803535C9;
static const u32 aPitcher_RunsAllowed       = 0x803535CA;
static const u32 aPitcher_EarnedRuns        = 0x803535CC;
static const u32 aPitcher_BattersWalked     = 0x803535CE;
static const u32 aPitcher_BattersHit        = 0x803535D0;
static const u32 aPitcher_HitsAllowed       = 0x803535D2;
static const u32 aPitcher_HRsAllowed        = 0x803535D4;
static const u32 aPitcher_PitchesThrown     = 0x803535D6;
static const u32 aPitcher_Stamina           = 0x803535D8;
static const u32 aPitcher_WasPitcher        = 0x803535DA;
static const u32 aPitcher_BatterOuts        = 0x803535E1;
static const u32 aPitcher_OutsPitched       = 0x803535E2;
static const u32 aPitcher_StrikeOuts        = 0x803535E4;
static const u32 aPitcher_StarPitchesThrown = 0x803535E5;
static const u32 aPitcher_IsStarred         = 0x8035323B;

static const u8 c_defensive_stat_offset = 0x1E;

//Addrs for OffensiveStats
static const u32 aBatter_AtBats       = 0x803537E8;
static const u32 aBatter_Hits         = 0x803537E9;
static const u32 aBatter_Singles      = 0x803537EA;
static const u32 aBatter_Doubles      = 0x803537EB;
static const u32 aBatter_Triples      = 0x803537EC;
static const u32 aBatter_Homeruns     = 0x803537ED;
static const u32 aBatter_BuntSuccess  = 0x803537EE; //Increments any time a bunt moves a runner
static const u32 aBatter_SacFlys      = 0x803537EF;
static const u32 aBatter_Strikeouts   = 0x803537F1;
static const u32 aBatter_Walks_4Balls = 0x803537F2;
static const u32 aBatter_Walks_Hit    = 0x803537F3;
static const u32 aBatter_RBI          = 0x803537F4;
static const u32 aBatter_BasesStolen  = 0x803537F6;
static const u32 aBatter_BigPlays     = 0x80353807;
static const u32 aBatter_StarHits     = 0x80353808;

static const u8 c_offensive_stat_offset = 0x26;


//Event Scenario 
static const u32 aAB_BatterRosterID  = 0x80890971;
static const u32 aAB_Inning          = 0x808928A3;
static const u32 aAB_HalfInning      = 0x8089294D;
static const u32 aAB_Balls           = 0x8089296F;
static const u32 aAB_Strikes         = 0x8089296B;
static const u32 aAB_Outs            = 0x80892973;
static const u32 aAB_P1_Stars        = 0x80892AD6;
static const u32 aAB_P2_Stars        = 0x80892AD7;
static const u32 aAB_IsStarChance    = 0x80892AD8;
static const u32 aAB_ChemLinksOnBase = 0x808909BA;

static const u32 aAB_RunnerOn1       = 0x8088F09D;
static const u32 aAB_RunnerOn2       = 0x8088F1F1;
static const u32 aAB_RunnerOn3       = 0x8088F345;

static const u32 aAB_AwayBatter     = 0x80892a68; //int but can be downcast to byte
static const u32 aAB_HomeBatter     = 0x80892a6c; //always valid; will show up to bat for next inning for fielding team

//Pitch
static const u32 aAB_PitcherRosterID       = 0x80890AD9;
static const u32 aAB_PitcherID             = 0x80890ADB;
static const u32 aAB_PitcherHandedness     = 0x80890B01;
static const u32 aAB_PitchType             = 0x80890B21; //0=Curve, Charge=1, ChangeUp=2. On charge star, remains as the default 0.
static const u32 aAB_ChargePitchType       = 0x80890B1F; //1=Charged Captain Star Pitch, 2=Slider, 3=Perfect
static const u32 aAB_StarPitch_Captain     = 0x80890B25;
static const u32 aAB_StarPitch_NonCaptain  = 0x80890B34;
static const u32 aAB_PitchSpeed            = 0x80890B0A;
static const u32 aAB_PitchCurveInput       = 0x80890A24; //0 if no curve is applied, otherwise its non-zero
static const u32 aAB_PitcherHasCtrlofPitch = 0x80890B12; //Above addr is valid when this addr =1
static const u32 aAB_PitchBallPosZStrikezone  = 0x80890A14;
static const u32 aAB_PitchStrikezoneEdgeLeft  = 0x80890A3C;
static const u32 aAB_PitchStrikezoneEdgeRight = 0x80890A40;

//At-Bat Hit
static const u32 aAB_BallPower      = 0x808926D6;
static const u32 aAB_VertAngle      = 0x808926D2;
static const u32 aAB_HorizAngle      = 0x808926D4;

static const u32 aAB_BallVel_X      = 0x80890E50;
static const u32 aAB_BallVel_Y      = 0x80890E54;
static const u32 aAB_BallVel_Z      = 0x80890E58;

static const u32 aAB_BallAccel_X    = 0x80890E5C;
static const u32 aAB_BallAccel_Y    = 0x80890E60;
static const u32 aAB_BallAccel_Z    = 0x80890E64;

static const u32 aAB_BallContactPos_X = 0x80890934;
static const u32 aAB_BallContactPos_Y = 0x80890938;
static const u32 aAB_BallContactPos_Z = 0x8089093c;

static const u32 aAB_BatContactPos_X = 0x8089095c;
static const u32 aAB_BatContactPos_Y = 0x80890960;
static const u32 aAB_BatContactPos_Z = 0x80890964;

static const u32 aAB_ContactRandInt1 = 0x802ec010;
static const u32 aAB_ContactRandInt2 = 0x802ec012;
static const u32 aAB_ContactRandInt3 = 0x802ec014;

static const u32 aAB_ContactAbsolute = 0x80890950;
static const u32 aAB_ContactQuality  = 0x80890954;
// 0=slap/linedrive star, 1=charge/grounder star/pop star, 2=captain star/moonshot, 3=bunt
static const u32 aAB_TypeOfSwing    = 0x8089099B; 
static const u32 aAB_ChargeUp       = 0x80890968;
static const u32 aAB_ChargeDown     = 0x8089096C;
static const u32 aAB_BatterHand     = 0x8089098B; //Right=0, Left=1
static const u32 aAB_InputDirection = 0x808909B9; //0=None, 1=PullingStickTowardsHitting, 2=PushStickAway
static const u32 aAB_StarSwing      = 0x808909b1; 
static const u32 aAB_MoonShot       = 0x808909B5;
static const u32 aAB_TypeOfContact  = 0x808909A2; //0=Sour, 1=Nice, 2=Perfect, 3=Nice, 4=Sour
static const u32 aAB_RBI            = 0x80893B9A; //RBI for the AB
static const u32 aAB_FramesUnitlBallArrivesBatter  = 0x80890AF2;
static const u32 aAB_TotalFramesOfPitch            = 0x80890AF4;
static const u32 aAB_MissedBall                    = 0x80890b18;

static const u32 aAB_ControlStickInput = 0x8089392C; //P1
static const u8 cControl_Offset = 0x10;

//static const u32 aBattingRandInt1 = 0x802ec010; // short
//static const u32 aBattingRandInt2 = 0x802ec012; // short
//static const u32 aBattingRandInt3 = 0x802ec014; // short

//At-Bat Miss
static const u32 aAB_Miss_SwingOrBunt = 0x808909A9; //(0=NoSwing, 1=Swing, 2=Bunt)
static const u32 aAB_Miss_AnyStrike = 0x80890B17;
static const u32 aAB_AnySwing = 0x8089099D; //1=Charge, Star, or Slap swing

//At-Bat Contact Result
static const u32 aAB_BallPos_X = 0x80890B38;
static const u32 aAB_BallPos_Y = 0x80890B3C;
static const u32 aAB_BallPos_Z = 0x80890B40;

static const u32 aAB_NumOutsDuringPlay = 0x808938AD;
static const u32 aAB_HitByPitch = 0x808909A3;

static const u32 aAB_DeadBallReason = 0x80892709; //0 = n/a, 1 = HR, 2=foul, 3=ground rule double, 4 = ball dead
static const u32 aAB_FinalResult = 0x80893BAA; //AT_BAT_RESULT

//Frame Data. Capture once play is over
static const u32 aAB_FrameOfSwing = 0x80890976; //(halfword) frame of swing animation; stops increasing when contact is made
static const u32 aAB_FrameOfPitchSeqUponSwing    = 0x80890978; //(halfword) frame of pitch that the batter swung


//Fielder addrs
//All of these addrs start with the pitcher. The rest are 0x268 away
static const u32 aFielder_ControlStatus = 0x8088F53B; //0xA=Fielder is holding ball
static const u32 aFielder_CharId = 0x8088F4E3; //Pitcher. Use Filder_Offset to calc the rest 
static const u32 aFielder_RosterLoc = 0x8088F4E1; //Pitcher. Use Filder_Offset to calc the rest 
static const u32 aFielder_AnyJump = 0x8088F56B; //Pitcher
static const u32 aFielder_Action = 0x8088F5C1; //Pitcher. 2=Slide, 3=Walljump
static const u32 aFielder_Bobble = 0x8088F5C0; //Pitcher
static const u32 aFielder_Knockout = 0x8088F578; //Pitcher
static const u32 aFielder_Pos_X = 0x8088F368; //Pitcher
static const u32 aFielder_Pos_Z = 0x8088F370; //Pitcher
static const u32 aFielder_Pos_Y = 0x8088F374; //Pitcher
static const u32 aFielder_ManualSelectArg = 0x802EBF97; //Pitcher
static const u32 cFielder_Offset = 0x268;


// This table is laid out by away/home (block 0 = away, block 1 = home), NOT by the
// controller-port team0/team1 ordering used by the pitcher/character stat tables.
static const u32 aBattingOrderAndPosition_Away = 0x808929D0;
static const u32 aBattingOrderAndPosition_Home = 0x80892A20;
static const u8 cBattingOrderAndPosition_Offset = 0x4;
static const u8 cRoster_Offset = 0x8;

//Runner addrs
static const u32 aRunner_BasepathPercentage = 0x8088EE7C;
static const u32 aRunner_RosterLoc = 0x8088EEF9;
static const u32 aRunner_CharId = 0x8088EEFB;
static const u32 aRunner_OutType = 0x8088EF44; //OUT_TYPE
static const u32 aRunner_OutLoc = 0x8088EF3F; //Technically holds the next base
static const u32 aRunner_CurrentBase = 0x8088EF3D;
static const u32 aRunner_Stealing = 0x8088EF66;
static const u32 cRunner_Offset = 0x154;


class StatTracker{
public:
    //StatTracker() { };
    Logger state_logger = Logger("state_log");;

    struct EndGameRosterDefensiveStats{
        u8  batters_faced;
        u16 runs_allowed;
        u16 earned_runs;
        u16 batters_walked;
        u16 batters_hit;
        u16 hits_allowed;
        u16 homeruns_allowed;
        u16 pitches_thrown;
        u16 stamina;
        u8 was_pitcher;
        u8 outs_pitched;
        u8 batter_outs;
        u8 strike_outs;
        u8 star_pitches_thrown;

        u8 big_plays;
    };

    struct EndGameRosterOffensiveStats{
        u64 game_id;
        u32 team_id;
        u32 roster_id;

        u8 at_bats;
        u8 hits;
        u8 singles;
        u8 doubles;
        u8 triples;
        u8 homeruns;
        u8 sac_flys;
        u8 successful_bunts;
        u8 strikouts;
        u8 walks_4balls;
        u8 walks_hit;
        u8 rbi;
        u8 bases_stolen;
        u8 star_hits;
    };

    struct CharacterSummary{
        u8 team_id;
        u8 roster_id;
        u8 char_id;
        u8 is_starred;
        u8 fielding_hand;
        u8 batting_hand;

        EndGameRosterDefensiveStats end_game_defensive_stats;
        EndGameRosterOffensiveStats end_game_offensive_stats;
    };

    struct Runner {
        u8 roster_loc;
        u8 char_id;
        u8 initial_base; //0=Batter, 1=1B, 2=2B, 3=3B
        u8 out_type = 0;
        u8 out_location = 0;
        u8 result_base = 0;
        u8 steal = 0;
        u32 basepath_location = 0;
    };

    struct Fielder {
        u8 fielder_roster_loc;
        u8 fielder_pos;
        u8 fielder_char_id;
        u8 fielder_swapped_for_batter;
        u8 fielder_action = 0; //2=slide, 3=walljump
        u8 fielder_jump = 0; //1=Jump
        u8 fielder_manual_select_arg; //0=No one selected, 1=Other player selected, 2=This player selected
        u32 fielder_x_pos;
        u32 fielder_y_pos;
        u32 fielder_z_pos;
        u8 bobble = 0; //Bobble info
    };

    struct Contact {
        //Vars with 1:1 Adrs
        TrackerAdr<u16> power       = TrackerAdr<u16>("Ball Power", aAB_BallPower, 0xFFFF);
        TrackerAdr<u16> vert_angle  = TrackerAdr<u16>("Vert Angle", aAB_VertAngle, 0xFFFF);
        TrackerAdr<u16> horiz_angle = TrackerAdr<u16>("Horiz Angle", aAB_HorizAngle, 0xFFFF);

        TrackerAdr<u32> ball_x_velo = TrackerAdr<u32>("Ball Velocity - X", aAB_BallVel_X, 0xFFFFFFFF);
        TrackerAdr<u32> ball_y_velo = TrackerAdr<u32>("Ball Velocity - Y", aAB_BallVel_Y, 0xFFFFFFFF);
        TrackerAdr<u32> ball_z_velo = TrackerAdr<u32>("Ball Velocity - Z", aAB_BallVel_Z, 0xFFFFFFFF);

        TrackerAdr<u32> ball_contact_x_pos = TrackerAdr<u32>("Ball Contact Pos - X", aAB_BallContactPos_X, 0xFFFFFFFF);
        TrackerAdr<u32> ball_contact_z_pos = TrackerAdr<u32>("Ball Contact Pos - Z", aAB_BallContactPos_Z, 0xFFFFFFFF);

        TrackerAdr<u32> contact_absolute = TrackerAdr<u32>("Contact Absolute", aAB_ContactAbsolute, 0xFFFFFFFF);
        TrackerAdr<u32> contact_quality = TrackerAdr<u32>("Contact Quality", aAB_ContactQuality, 0xFFFFFFFF);

        TrackerAdr<u16> rng1 = TrackerAdr<u16>("RNG1", aAB_ContactRandInt1, 0xFFFF);
        TrackerAdr<u16> rng2 = TrackerAdr<u16>("RNG2", aAB_ContactRandInt2, 0xFFFF);
        TrackerAdr<u16> rng3 = TrackerAdr<u16>("RNG3", aAB_ContactRandInt3, 0xFFFF);

        //Hit Status
        TrackerAdr<u8> type_of_contact = TrackerAdr<u8>("Type of Contact", aAB_TypeOfContact, 0xFF);
        TrackerAdr<u8> moon_shot = TrackerAdr<u8>("Star Swing Five-Star", aAB_MoonShot, 0xFF);

        //Charge Power
        TrackerAdr<u32> charge_power_up = TrackerAdr<u32>("Charge Power Up", aAB_ChargeUp, 0xFFFFFFFF);
        TrackerAdr<u32> charge_power_down = TrackerAdr<u32>("Charge Power Down", aAB_ChargeDown, 0xFFFFFFFF);
        
        TrackerValue<u8> input_direction_stick = TrackerValue<u8>("Input Direction - Stick", 0);
        TrackerAdr<u8> input_direction_push_pull = TrackerAdr<u8>("Input Direction - Push/Pull", aAB_InputDirection, 0xFF);

        TrackerAdr<u16> frame_of_swing = TrackerAdr<u16>("Frame of Swing Upon Contact", aAB_FrameOfSwing, 0xFFFF);
        
        //Final Result Ball
        TrackerAdr<u32> ball_x_pos = TrackerAdr<u32>("Ball Landing Position - X", aAB_BallPos_X, 0xFFFFFFFF);
        TrackerAdr<u32> ball_y_pos = TrackerAdr<u32>("Ball Landing Position - Y", aAB_BallPos_Y, 0xFFFFFFFF);
        TrackerAdr<u32> ball_z_pos = TrackerAdr<u32>("Ball Landing Position - Z", aAB_BallPos_Z, 0xFFFFFFFF);

        //More ball flight info
        TrackerAdr<u32> ball_max_height = TrackerAdr<u32>("Ball Max Height", 0x8089250c, 0xFFFFFFFF);
        TrackerAdr<u16> ball_hang_time = TrackerAdr<u16>("Ball Hang Time", 0x80892696, 0xFFFF);

        //PRIMARY_CONTACT_RESULT
        u8 primary_contact_result;

        //SECONDARY_CONTACT_RESULT, or AT_BAT_RESULT from SINGLE to DOUBLE_PLAY
        u8 secondary_contact_result = static_cast<u8>(SECONDARY_CONTACT_RESULT::NONE);

        std::optional<Fielder> first_fielder;
        std::optional<Fielder> collect_fielder;
    };

    struct Pitch{
        //Pitcher Status
        bool logged = false;
        u8 pitcher_team_id;
        u8 pitcher_char_id;
        u8 pitch_type;
        u8 charge_type;
        u8 star_pitch;
        u8 pitch_speed;

        u8 batter_roster_loc;
        u8 batter_id;

        //Ball pos for pitch visualization
        u32 ball_z_strike_vs_ball;
        u8 ball_in_strikezone;

        TrackerAdr<u32> bat_contact_x_pos = TrackerAdr<u32>("Bat Contact Pos - X", aAB_BatContactPos_X, 0xFFFFFFFF);
        TrackerAdr<u32> bat_contact_z_pos = TrackerAdr<u32>("Bat Contact Pos - Z", aAB_BatContactPos_Z, 0xFFFFFFFF);

        //For integrosity - TODO
        u8 db = 0;
        bool potential_db = false;

        //0=HBP
        //1=BB
        //2=Ball
        //3=Strike-looking
        //4=Strike-swing
        //5=Strike-bunting
        //6=Contact
        //7=Unknown
        u8 pitch_result; 

        //Info about the batter.
        u8 type_of_swing;
        std::optional<Contact> contact;
    };

    struct Event{
        u16 event_num;
        bool pick_off_attempt = false;

        u8 inning;
        u8 half_inning;
        u16 away_score;
        u16 home_score;
        u8 is_star_chance;
        u8 away_stars;
        u8 home_stars;
        u8 chem_links_ob;
        u16 pitcher_stamina;

        u8 pitcher_roster_loc;
        u8 batter_roster_loc;
        u8 catcher_roster_loc;

        std::array<u16, 18> away_inning_scores = {};
        std::array<u16, 18> home_inning_scores = {};

        u8 away_batter_roster_loc = 0; // Current batter for away team (persists when fielding)
        u8 home_batter_roster_loc = 0; // Current batter for home team (persists when fielding)

        u8 balls;
        u8 strikes;
        u8 outs;

        std::optional<Runner> runner_batter;
        std::optional<Runner> runner_1;
        std::optional<Runner> runner_2;
        std::optional<Runner> runner_3;
        std::optional<Pitch>  pitch;

        std::array<u8, cRosterSize> manual_select_locks;

        //Double play or more
        TrackerAdr<u8> num_outs_during_play = TrackerAdr<u8>("Num Outs During Play", aAB_NumOutsDuringPlay, 0xFF);

        u8 rbi;
        u8 dead_ball_reason;
        u8 result_of_atbat;

        //Partial game. indicates this game has not been finished
        std::pair<bool, bool> write_hud_ab = {true, true};

        std::vector<EVENT_STATE> history;
        std::string stringifyHistory() {
            std::string stringifiedHistory;
            for(EVENT_STATE i : history) {  
                stringifiedHistory += c_event_state[i] + "\n";
            }
            return stringifiedHistory;
        }
    };
    
    struct GameInfo{
        u64 game_id;
        bool init_game = true;
        bool game_active = false;
        std::string start_unix_date_time;
        std::string start_local_date_time;
        std::string end_unix_date_time;
        std::string end_local_date_time;

        u8 team0_port = 0xFF;
        u8 team1_port = 0xFF;
        u8 away_port;
        u8 home_port;

        u8 team0_captain_roster_loc = 0xFF;
        u8 team1_captain_roster_loc = 0xFF;

        u32 away_logo;
        u32 home_logo;

        LocalPlayers::LocalPlayers::Player team0_player;
        LocalPlayers::LocalPlayers::Player team1_player;
        int avg_ping = 0;
        int lag_spikes = 0;

        //Auto capture
        u16 away_score;
        u16 home_score;

        u8 stadium;

        u8 innings_selected;
        u8 innings_played;

        u8 first_batting_team;
        u8 star_skills_on;
        u8 mercy_on;

        //Netplay info
        bool netplay;
        std::string netplay_opponent_alias;

        //Started mid-game using the fast reset from HUD code.
        //Set authoritatively at the PREGAME->INGAME transition in Run(); see MSB_StatTracker.cpp.
        bool fastResetFromHUD = false;

        //TagSet info
        std::optional<int> tag_set_id = std::nullopt;

        //Quit?
        u8 quitter_team = 0xFF;

        //Tracks the current batter for each team across half-inning switches; index 0=Away, 1=Home
        std::array<u8, 2> current_batter_roster_locs = {0, 0};

        //Bookkeeping
        //int pitch_num = 0;
        int event_num = 0;

        //Update server with new AB
        bool update_ongoing_game = true;
        bool post_ongoing_game = true;

        //Array of both teams' character summaries
        std::array<std::array<CharacterSummary, cRosterSize>, cNumOfTeams> character_summaries;

        //All of the events for this game
        std::map<u16, Event> events;
        std::optional<Event> previous_state;
        bool write_hud = true;

        //Buffer used to delay the event init by num of frames (60 for now)
        u8 event_init_frame_buffer = 0;
        u8 cNumOfFramesBeforeEventInit = 20;

        std::map<int, LocalPlayers::LocalPlayers::Player> NetplayerUserInfo;  // int is port

        Event& getCurrentEvent() { return events.at(event_num); }
        bool currentEventVld() { return (events.count(event_num) >= 1); }

        LocalPlayers::LocalPlayers::Player getAwayTeamPlayer() { 
            if (team0_port == away_port) {
                return team0_player;
            }
            else{
                return team1_player;
            }
        }
        LocalPlayers::LocalPlayers::Player getHomeTeamPlayer() { 
            if (team0_port == home_port) {
                return team0_player;
            }
            else{
                return team1_player;
            }
        }
    };
    GameInfo m_game_info;

    struct FielderInfo{
        u8 current_pos = 0xFF;
        u8 previous_pos = 0xFF;

        std::array<int, cNumOfPositions> pitch_count_by_position = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        std::array<int, cNumOfPositions> batter_count_by_position = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        std::array<int, cNumOfPositions> out_count_by_position   = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        std::array<int, cNumOfPositions> batter_outs_by_position   = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    };

    struct FielderTracker {

        u8 team_id = 0xFF;
        //Roster_loc, Fielder Info
        //Set changed=true any time the sampled position (each pitch) does not match the current position
        //Rest changed upon new batter
        std::map<u8, FielderInfo> fielder_map = {
            {0, FielderInfo()},
            {1, FielderInfo()},
            {2, FielderInfo()},
            {3, FielderInfo()},
            {4, FielderInfo()},
            {5, FielderInfo()},
            {6, FielderInfo()},
            {7, FielderInfo()},
            {8, FielderInfo()}
        };

        bool initialized = false;
        u8 prev_batter_roster_loc = 0xFF; //Used to check each pitch if the batter has changed.
                                          //Mark current positions when changed

        void initTracker(const Core::CPUThreadGuard& guard, u8 inTeamId){
            team_id = inTeamId;
            initialized = true;
            for (u8 pos=0; pos < cRosterSize; ++pos){
                u32 aFielderRosterLoc_calc = aBattingOrderAndPosition_Away + (pos * cRoster_Offset) + (10 * cRoster_Offset * team_id) + cBattingOrderAndPosition_Offset;

                u8 fielder_loc = static_cast<u8>(PowerPC::MMU::HostRead_U32(guard, aFielderRosterLoc_calc));

                std::cout << "RosterLoc:" << std::to_string(pos) 
                          << " Init Pos=" << cPosition.at(fielder_loc) << std::endl;

                fielder_map[pos].current_pos = fielder_loc;
                fielder_map[pos].previous_pos = fielder_loc;
            }
        }

        void newBatter() {
            for (auto& kv : fielder_map){
                kv.second.previous_pos = kv.second.current_pos;
            }
        }
        
        //Scans field to see who is playing which position and increments counts for positions
        void evaluateFielders(const Core::CPUThreadGuard& guard) {
            for (u8 pos=0; pos < cRosterSize; ++pos){
                u32 aFielderRosterLoc_calc = aBattingOrderAndPosition_Away + (pos * cRoster_Offset) + (10 * cRoster_Offset * team_id) + cBattingOrderAndPosition_Offset;

                u8 fielder_loc = static_cast<u8>(PowerPC::MMU::HostRead_U32(guard, aFielderRosterLoc_calc));

                //If new position, mark changed, then set new position
                if (fielder_map[pos].current_pos != fielder_loc){
                    std::cout << " Team=" << std::to_string(team_id) << " RosterLoc:" << std::to_string(pos)
                                << " swapped from " << cPosition.at(fielder_map[pos].current_pos)
                                << " to " << cPosition.at(fielder_loc) << std::endl;
                    fielder_map[pos].current_pos = fielder_loc;
                }

                //Increment the number of pitches this player has seen at this position
                ++fielder_map[pos].pitch_count_by_position[fielder_loc];
                //std::cout << " Team=" << std::to_string(team_id) << " RosterLoc=" << std::to_string(roster_loc)
                //          << " Pos=" << std::to_string(pos) << "++" << std::endl; 
            }
            return;
        }

        bool outsAtAnyPosition(u8 roster_loc, int starting_pos) {
            for (int pos=starting_pos; pos < cRosterSize; ++pos){
                if (fielder_map[roster_loc].out_count_by_position[pos] > 0) { 
                    return true;
                }
            }
            return false;
        }

        bool pitchesAtAnyPosition(u8 roster_loc, int starting_pos) {
            for (int pos=starting_pos; pos < cRosterSize; ++pos){
                if (fielder_map[roster_loc].pitch_count_by_position[pos] > 0) { 
                    return true;
                }
            }
            return false;
        }

        bool batterOutsAtAnyPosition(u8 roster_loc, int starting_pos) {
            for (int pos=starting_pos; pos < cRosterSize; ++pos){
                if (fielder_map[roster_loc].batter_outs_by_position[pos] > 0) { 
                    return true;
                }
            }
            return false;
        }

        bool battersAtAnyPosition(u8 roster_loc, int starting_pos) {
            for (int pos=starting_pos; pos < cRosterSize; ++pos){
                if (fielder_map[roster_loc].batter_count_by_position[pos] > 0) { 
                    return true;
                }
            }
            return false;
        }

        void incrementOutForPosition(u8 roster_loc, u8 pos){
            ++fielder_map[roster_loc].out_count_by_position[pos];
            //std::cout << " incrementOutForPosition: " << std::to_string(fielder_map[roster_loc].out_count_by_position[pos]);
        }

        void incrementBatterOutForPosition(int num_outs){
            for (u8 roster=0; roster < cRosterSize; ++roster){
                //Increment the number of batter outs this player has seen at this position
                fielder_map[roster].batter_outs_by_position[fielder_map[roster].current_pos] = fielder_map[roster].batter_outs_by_position[fielder_map[roster].current_pos] + num_outs;
                // std::cout << " incrementBatterOutForPosition: " << std::to_string(fielder_map[roster].batter_outs_by_position[fielder_map[roster].current_pos]);
            }
            return;
        }

        void incrementBattersForPosition(){
            for (u8 roster=0; roster < cRosterSize; ++roster){
                //Increment the number of batter outs this player has seen at this position
                ++fielder_map[roster].batter_count_by_position[fielder_map[roster].current_pos];
                // std::cout << " incrementBattersForPosition: " << std::to_string(fielder_map[roster].batter_count_by_position[fielder_map[roster].current_pos]);
            }
            return;
        }

        u8 wasFielderSwappedForBatter(u8 roster_loc){
            if (fielder_map[roster_loc].current_pos != fielder_map[roster_loc].previous_pos){
                return 1;
            }
            return 0;
        }
    };
    std::array<FielderTracker, cNumOfTeams> m_fielder_tracker; //One per team

    void init(){
        //Reset all game info
        m_game_info = GameInfo(); // crashed here when enabling night stadium on netplay in debugging
        m_fielder_tracker[0] = FielderTracker();
        m_fielder_tracker[1] = FielderTracker();

        //Reset state machines
        m_game_state  = GAME_STATE::PREGAME;
        m_event_state = EVENT_STATE::INIT_EVENT;
    }

    //Not reset by init(): the flag stays set until the game rel is reloaded, so the finished game must not start a new one
    bool m_game_has_started_prev = false;
    bool m_saw_game_start_movie = false;

    //Host's game id waiting to be applied by the CPU thread (0 == none). Not reset by init(): it can
    //arrive before this client's game has started
    std::atomic<u64> m_pending_netplay_game_id{0};
    u64 generateGameID();
    void applyPendingNetplayGameID();
    bool hasLeftGame(const Core::CPUThreadGuard& guard);

    GAME_STATE  m_game_state  = GAME_STATE::PREGAME;
    GAME_STATE  m_game_state_prev = GAME_STATE::UNDEFINED;
    EVENT_STATE m_event_state = EVENT_STATE::INIT_EVENT;
    EVENT_STATE m_event_state_prev = EVENT_STATE::UNDEFINED;

    struct state_members{
        bool m_netplay_session = false;
        std::optional<int> m_tag_set;
        std::string m_netplay_opponent_alias = "";
        std::optional<int> tag_set_id_local = std::nullopt;
        std::optional<int> tag_set_id_netplay = std::nullopt;
    } m_state;

    union
    {
        u32 num;
        float fnum;
    } float_converter;

    void setTagSetId(Tag::TagSet tag_set, bool netplay);
    void clearTagSetId(bool netplay);
    void setNetplaySession(bool netplay_session, std::string opponent_name = "");
    void setAvgPing(int avgPing);
    void setLagSpikes(int nLagSpikes);
    void setNetplayerUserInfo(std::map<int, LocalPlayers::LocalPlayers::Player> userInfo);
    // Called from the netplay thread when the host's id for the current game arrives
    void setNetplayGameID(u64 gameID);
    bool isGameInProgress() const { return m_game_state == GAME_STATE::INGAME; }
    // void setTags(std::vector tags);
    // void setTagSet(int tagset);

    void Run(const Core::CPUThreadGuard& guard);
    void lookForTriggerEvents(const Core::CPUThreadGuard& guard);

    void logGameInfo(const Core::CPUThreadGuard& guard);
    void logDefensiveStats(const Core::CPUThreadGuard& guard, int team_id, int roster_id);
    void logOffensiveStats(const Core::CPUThreadGuard& guard, int team_id, int roster_id);
    
    void logEventState(const Core::CPUThreadGuard& guard, Event& in_event);
    void logContact(const Core::CPUThreadGuard& guard, Event& in_event);
    void logPitch(const Core::CPUThreadGuard& guard, Event& in_event);
    void logContactResult(const Core::CPUThreadGuard& guard, Contact* in_contact);
    void logFinalResults(const Core::CPUThreadGuard& guard, Event& in_event);
    //void logManualSelectLocks(Event& in_event);

    //Quit function
    void onGameQuit(const Core::CPUThreadGuard& guard);
    bool shouldSubmitGame();

    //RunnerInfo
    std::optional<Runner> logRunnerInfo(const Core::CPUThreadGuard& guard, u8 base);
    bool anyRunnerStealing(const Core::CPUThreadGuard& guard, Event& in_event);
    bool hasEnoughStarsForStarSwing(const Core::CPUThreadGuard& guard, Event& in_event);
    void logRunnerEvents(const Core::CPUThreadGuard& guard, Runner* in_runner);

    //TODO Redo these tuple functions
    std::optional<Fielder> logFielderWithBall(const Core::CPUThreadGuard& guard);

    std::optional<Fielder> logFielderBobble(const Core::CPUThreadGuard& guard);
    //Read players from ini file and assign to team
    void readPlayerNames(bool local_game);
    //void setDefaultNames(bool local_game);

    float floatConverter(u32 in_value) {
        float out_float;
        float_converter.num = in_value;
        out_float = float_converter.fnum;
        return out_float;
    }

    Common::HttpRequest m_http{std::chrono::minutes{3}};

    // Sends ongoing-game submissions to the web server on a dedicated background
    // thread so the blocking HTTP round-trip never stalls the emulation thread.
    //
    // The emulation thread builds the JSON payload (cheap) and hands it off via
    // QueuePost()/QueueUpdate(); the worker owns its own HttpRequest and performs
    // the blocking POST. "Post" submissions establish the ongoing-game record and
    // are always sent in order, never dropped. "Update" submissions are full state
    // snapshots, so if several pile up behind a slow request the worker coalesces
    // consecutive pending updates down to the newest one. A post is never dropped
    // or reordered relative to the updates around it.
    class OngoingGameSubmitter
    {
    public:
        OngoingGameSubmitter() = default;
        ~OngoingGameSubmitter();

        // Non-blocking. Safe to call from the emulation thread.
        void QueuePost(std::string payload);
        void QueueUpdate(std::string payload);

    private:
        enum class Kind { Post, Update };
        struct Item
        {
            Kind kind = Kind::Update;
            std::string payload;
        };

        void EnsureThreadStarted();
        void Enqueue(Item&& item);
        void ThreadLoop();

        static constexpr char s_url[] = "https://api.projectrio.app/populate_db/ongoing_game/";

        // Short timeout: these are fire-and-forget telemetry, so a stalled request
        // should be abandoned quickly rather than holding up later submissions.
        Common::HttpRequest m_http{std::chrono::seconds{10}};

        std::thread m_thread;
        std::mutex m_lock;
        std::condition_variable m_cv;
        std::queue<Item> m_items;
        bool m_thread_started = false;
        bool m_shutdown = false;
    };

    OngoingGameSubmitter m_ongoing_game_submitter;

    //The type of value to decode, the value to be decoded, bool for decode if true or original value if false
    std::string decode(std::string type, u8 value, bool decode);

    //Returns JSON, PathToWriteTo
    std::string getStatJSON(bool inDecode, bool hide_riokey = true);
    std::string getEventJSON(u16 in_event_num, Event& in_event, bool inDecode);
    std::string getHUDJSON(std::string in_event_num, Event& in_curr_event, std::optional<Event> in_prev_event, bool inDecode);
    //Returns path to save json
    std::string getStatJsonPath(std::string prefix);

    void postOngoingGame(Event& in_event);
    void updateOngoingGame(Event& in_event);

    std::pair<u8,u8> getBatterFielderPorts(const Core::CPUThreadGuard& guard){
        // These values are the actual port numbers
        // and are indexed into using the below u8s
        std::array<u8, 2> ports = {PowerPC::MMU::HostRead_U8(guard, aPlayer1Port), PowerPC::MMU::HostRead_U8(guard, aPlayer2Port)};

        // These registers will always be 0 or 1
        // and swap values each half inning
        u32 BattingTeam = PowerPC::MMU::HostRead_U32(guard, aBattingTeam_P1P2);
        u32 PitchingTeam = PowerPC::MMU::HostRead_U32(guard, aFieldingTeam_P1P2);
        
        u8 BattingPort = ports[BattingTeam];
        u8 FieldingPort = ports[PitchingTeam];

        return std::make_pair(BattingPort, FieldingPort);
    }

    /*
    std::pair<u8,u8> getHomeAwayPort(){
        // These values are the actual port numbers
        // and are indexed into using the below u8s
        std::array<u8, 2> ports = {PowerPC::MMU::HostRead_U8(0x800e874c), PowerPC::MMU::HostRead_U8(0x800e874d)};
        
        m_game_info.home_port = ports[0];
        m_game_info.away_port = ports[1];

        return std::make_pair(m_game_info.home_port, m_game_info.away_port);
    }
    */

    void initPlayerInfo(const Core::CPUThreadGuard& guard);
    void initCaptains(const Core::CPUThreadGuard& guard);

    //If mid-game, dump game
    void dumpGame(const Core::CPUThreadGuard& guard){
        if (m_game_state == GAME_STATE::INGAME){
            m_game_info.quitter_team = 2;
            logGameInfo(guard);

            //Remove current event, wasn't finished
            auto it = m_game_info.events.find(m_game_info.event_num);
            if (it != m_game_info.events.end())
            {
              m_game_info.events.erase(it);
            }

            //Game has ended. Write file but do not submit
            std::string jsonPath = getStatJsonPath("crash.decode.");
            std::string json = getStatJSON(true);
            
            File::WriteStringToFile(jsonPath, json);

            jsonPath = getStatJsonPath("crash.");
            json = getStatJSON(false, true);
            
            File::WriteStringToFile(jsonPath, json);
            init();
        }
    }
};
