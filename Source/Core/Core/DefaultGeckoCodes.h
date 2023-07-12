#pragma once

#include <string>
#include <array>
#include <vector>
#include <map>
#include <set>
#include <tuple>
#include "Core/HW/Memmap.h"
#include "Common/TagSet.h"

class DefaultGeckoCodes {
  public:
    void Init(std::optional<std::vector<ClientCode>> client_codes, bool tagset_active, bool is_night, bool disable_replays);
    void RunCodeInject();

  private:
    struct DefaultGeckoCode
    {
      u32 addr;
      u32 conditionalVal;  // set to 0 if no onditional needed; condition is for when we don't want
                           // to write at an addr every frame
      std::vector<u32> codeLines;
    };
    // DefaultGeckoCode mGeckoCode;

    void AddRequiredCodes();
    void AddTagSetCodes();
    void AddOptionalCodes();
    void WriteAsm(DefaultGeckoCode CodeBlock);

    u32 aWriteAddr;  // address where the first code gets written to

    bool initiated = false;
    bool TagSetActive;
    std::optional<std::vector<ClientCode>> ClientCodes;
    bool IsNight;
    bool DisableReplays;

    // Default Rumble On [LittleCoaks]
    static const u32 aControllerRumble = 0x80366177;

    // Boot to Main Menu [LittleCoaks]
    static const u32 aBootToMainMenu = 0x8063F964;

    // Skip Memory Card Check on Main Menu [LittleCoaks]
    static const u32 aSkipMemCardCheck = 0x803C50E8;

    // Unlimited Extra Innings [LittleCoaks]
    static const u32 aUnlimitedExtraInnings = 0x80699A10;

    // Unlock Everything [LittleCoaks]
    static const u32 aUnlockEverything_1 = 0x800E870E;  // 0x0 loops of 0x2
    static const u32 aUnlockEverything_2 = 0x800E8710;  // 0x5 loops of 0x3
    static const u32 aUnlockEverything_3 = 0x800E8716;  // 0x5 loops of 0x1
    static const u32 aUnlockEverything_4 = 0x80361680;  // 0x29 loops of 0x1
    static const u32 aUnlockEverything_5 = 0x803616B0;  // 0x0 loops of 0x1 
    static const u32 aUnlockEverything_6 = 0x80361B20;  // 0x35 loops of 0x1
    static const u32 aUnlockEverything_7 = 0x80361C04;  // 0x3 loops of 0x1
    static const u32 aUnlockEverything_8 = 0x80361C14;  // 0x1 loops of 0x1

    // Pitch Clock [LittleCoaks]
    static const u32 aPitchClock_1 = 0x806b4490;
    static const u32 aPitchClock_2 = 0x806b42d0;
    static const u32 aPitchClock_3 = 0x806b46b8;

    // Bat Sound Effect on Start Game [LittleCoaks]
    static const u32 aBatSound = 0x80042cd0;  // write to 0x386001bb

    // Disable Music [LittleCoaks]
    static const u32 aDisableMusic_1 = 0x80062ab0;  // write to 0x38000000
    static const u32 aDisableMusic_2 = 0x806cccb0;  // write to 0x38000000

    // Never Cull [Roeming]
    static const u32 aNeverCull_1 = 0x8001dcf8;  // write to 0x38000007
    static const u32 aNeverCull_2 = 0x806aa4e4;  // write to 0x38000001
    static const u32 aNeverCull_3 = 0x806ab8b4;  // write to 0x38000001
    static const u32 aNeverCull_4 = 0x806f7b7c;  // write to 0x38000003 if equal to 0x881a0093

    // Disable Replays [LittleCoaks]
    static const u32 aDisableReplays = 0x806bb214; // write to 0x38000000 if equal to 0x38000001

    // Force Star Skills Off [LittleCoaks]
    static const u32 aDisableStarSkills = 0x800498d4; // write to 0x98c7003d
    static const u32 aDisableStarSkills_1 = 0x80049030;  // write to 0x38a00000

    // Default Rules [LittleCoaks]
    static const u32 aDefaultMercyOn = 0x800498DC;  // write to 0x9867003F
    static const u32 aDefaultNineInnings = 0x800498C4;  // write to 0x38000004
    static const u32 aDefaultDropSpotsOff = 0x800498FC;   // write to 0x98C70048
    static const u32 aDefaultDropSpotsOff_1 = 0x8004991C;  // write to 0x98C7004C

    // Duplicate Characters [PeacockSlayer, LittleCoaks]
    static const u32 aDuplicate_1 = 0x803530f7; // 0x35 loops of 0x0
    static const u32 aDuplicate_2 = 0x803c6050; // 0x23 loops of 0xff
    static const u32 aDuplicate_3 = 0x80067bac; // write to 0x60000000
    static const u32 aDuplicate_4 = 0x80067bc8;
    static const u32 aDuplicate_5 = 0x80067be4;
    static const u32 aDuplicate_6 = 0x80067bac;
    static const u32 aDuplicate_7 = 0x80067c00;
    static const u32 aDuplicate_8 = 0x8064ece8;
    static const u32 aDuplicate_9 = 0x8064ec38;
    static const u32 aDuplicate_10 = 0x8064ec28;
    static const u32 aDuplicate_11 = 0x8004e548;
    static const u32 aDuplicate_12 = 0x806553f8;
    static const u32 aDuplicate_13 = 0x8004e6b0; // write to 0x2c0000ff
    static const u32 aDuplicate_14 = 0x806553d4;

    // Duplicates and Variants Have Chem [LittleCoaks]
    static constexpr u32 aDuplicateChem[140] = {
        0x8034e9db, 0x8034ea7c, 0x8034eb1d, 0x8034ebbe, 0x8034ec5f, 0x8034ed00, 0x8034eda1,
        0x8034ee42, 0x8034eee3, 0x8034ef84, 0x8034f025, 0x8034f0c6, 0x8034f167, 0x8034f185,
        0x8034f208, 0x8034f218, 0x8034f219, 0x8034f21a, 0x8034f21b, 0x8034f2a9, 0x8034f34a,
        0x8034f3eb, 0x8034f407, 0x8034f408, 0x8034f409, 0x8034f40a, 0x8034f48c, 0x8034f52d,
        0x8034f5ce, 0x8034f66f, 0x8034f686, 0x8034f710, 0x8034f711, 0x8034f712, 0x8034f7b0,
        0x8034f7b1, 0x8034f7b2, 0x8034f850, 0x8034f851, 0x8034f852, 0x8034f8f3, 0x8034f8f4,
        0x8034f8f5, 0x8034f993, 0x8034f994, 0x8034f995, 0x8034fa33, 0x8034fa34, 0x8034fa35,
        0x8034fad6, 0x8034faef, 0x8034faf0, 0x8034fb77, 0x8034fc08, 0x8034fc18, 0x8034fc19,
        0x8034fc1a, 0x8034fc1b, 0x8034fca8, 0x8034fcb8, 0x8034fcb9, 0x8034fcba, 0x8034fcbb,
        0x8034fd48, 0x8034fd58, 0x8034fd59, 0x8034fd5a, 0x8034fd5b, 0x8034fde8, 0x8034fdf8,
        0x8034fdf9, 0x8034fdfa, 0x8034fdfb, 0x8034fe9c, 0x8034fe9d, 0x8034fe9e, 0x8034fe9f,
        0x8034ff3c, 0x8034ff3d, 0x8034ff3e, 0x8034ff3f, 0x8034ffdc, 0x8034ffdd, 0x8034ffde,
        0x8034ffdf, 0x8035007c, 0x8035007d, 0x8035007e, 0x8035007f, 0x80350120, 0x803501c1,
        0x80350262, 0x80350303, 0x803503a4, 0x80350427, 0x80350445, 0x803504cf, 0x803504e6,
        0x8035056b, 0x80350587, 0x80350588, 0x80350589, 0x8035058a, 0x8035060b, 0x80350627,
        0x80350628, 0x80350629, 0x8035062a, 0x803506ab, 0x803506c7, 0x803506c8, 0x803506c9,
        0x803506ca, 0x8035074b, 0x80350767, 0x80350768, 0x80350769, 0x8035076a, 0x8035080b,
        0x8035080c, 0x8035080d, 0x8035080e, 0x803508ab, 0x803508ac, 0x803508ad, 0x803508ae,
        0x8035094b, 0x8035094c, 0x8035094d, 0x8035094e, 0x803509eb, 0x803509ec, 0x803509ed,
        0x803509ee, 0x80350a76, 0x80350a8f, 0x80350a90, 0x80350b16, 0x80350b2f, 0x80350b30};

    // Generate the Game ID at 0x802EBF8C when Start Game is pressed [LittleCoaks]
    const DefaultGeckoCode sGenerateGameID = {
        0x80042CCC, 0,
        {0x3C80802E, 0x6084BF8C, 0x7C6C42E6, 0x90640000, 0x3C80800F}
    };

    // Clear Game ID when exiting mid - game [LittleCoaks]
    const DefaultGeckoCode sClearGameID_1 = {
        0x806ed704, 0,
        {0x981F01D2, 0x3D00802E, 0x6108BF8C, 0x38000000, 0x90080000}
    };

    // Clear Game ID when exiting mid - game [LittleCoaks]
    const DefaultGeckoCode sClearGameID_2 = {
        0x806edf8c, 0,
        {0x981F01D2, 0x3D00802E, 0x6108BF8C, 0x38000000, 0x90080000}
    };

    // Clear Game ID when returning to main menu after game ends [LittleCoaks]
    const DefaultGeckoCode sClearGameID_3 = {
        0x8069AB2C, 0,
        {0x98050125, 0x3E40802E, 0x6252BF8C, 0x38600000, 0x90720000}
    };

    // Clear port info after game ends [LittleCoaks]
    const DefaultGeckoCode sClearPortInfo = {
        0x8063F14C, 0x38600000,
        {0x38600000, 0x3CA0802E, 0x60A5BF90, 0x98650001, 0xB0650002, 0xB0650004}
    };

    // Clear hit result after each pitch [PeacockSlayer]
    const DefaultGeckoCode sClearHitResult = {
        0x806bbf88, 0,
        {0x99090037, 0x3ea08089, 0x62b53baa, 0x99150000, 0x3aa00000}
    };

    // Store Port Info->0x802EBF94 (fielding port) and 0x802EBF95 (batting port) [LittleCoaks]
    const DefaultGeckoCode sStorePortInfo_1 = {
        0x80042CD8, 0,
        {0x9421FFB0, 0xBDC10008, 0x3DE0802E, 0x61EFBF91, 0x3E008089, 0x62102ACA,
        0x8A100000, 0x2C100001, 0x4182000C, 0x3A000001, 0x48000008, 0x3A000005,
        0x9A0F0000, 0x3E00800E, 0x6210874D, 0x8A100000, 0x3A100001, 0x9A0F0001,
        0xB9C10008, 0x38210050, 0x38A0007F}
    };

    // Store Port Info [LittleCoaks]
    const DefaultGeckoCode sStorePortInfo_2 = {
        0x806706B8, 0x3c608089,
        {0x3FE08089, 0x63FF3928, 0x7C04F800, 0x3FE0802E, 0x63FFBF91, 0x41820018, 0x887F0000,
        0x987F0004, 0x887F0001, 0x987F0003, 0x48000014, 0x887F0001, 0x987F0004, 0x887F0000,
        0x987F0003, 0x3C608089}
    };

    // Remember Who Quit; stores the port who quit a game at 0x802EBF93 [LittleCoaks]
    const DefaultGeckoCode sRememberWhoQuit_1 = {
        0x806ed700, 0,
        {0xb08300fe, 0x3e80802e, 0x6294bf93, 0x8ab40001, 0x9ab40000}
    };

    // Remember Who Quit [LittleCoaks]
    const DefaultGeckoCode sRememberWhoQuit_2 = {
        0x806edf88, 0,
        {0xb08300fe, 0x3e80802e, 0x6294bf93, 0x8ab40002, 0x9ab40000}
    };

    // Anti Quick Pitch [PeacockSlayer, LittleCoaks]
    const DefaultGeckoCode sAntiQuickPitch = {
        0x806B406C, 0xa01e0006,
        {0x3DC08089, 0x61CE80DE, 0x89CE0000, 0x2C0E0000, 0x4082001C, 0x38000000,
        0x3DC08089, 0x61CE099D, 0x89CE0000, 0x2C0E0001, 0x41820008, 0xA01E0006}
    };

    // Manual Fielder Select 4.0 [PeacockSlayer, LittleCoaks]
    const DefaultGeckoCode sManualSelect = {
        0x80678F8C, 0x88061BD1,
        {
            0x7C6E1B78, 0x7C0802A6, 0x90010004, 0x9421FF00, 0xBC610008, 0x3D20802E, 0x6129BF97,
            0x3C808089, 0x6084269E, 0xA0840000, 0x2C04000F, 0x40810178, 0x3C808089, 0x60842973,
            0x88840000, 0x2C040003, 0x40800024, 0x3C808089, 0x60842701, 0x88840000, 0x2C040000,
            0x41820010, 0x2C040003, 0x41820008, 0x48000144, 0x3D008089, 0x61082898, 0xA1080000,
            0x88890000, 0x71050010, 0x2C050010, 0x4082000C, 0x38800000, 0x98890000, 0x71050800,
            0x2C050800, 0x4082000C, 0x38800001, 0x98890000, 0x71050400, 0x2C050400, 0x4082000C,
            0x38800002, 0x98890000, 0x71050020, 0x2C050020, 0x4082000C, 0x38800003, 0x98890000,
            0x2C040004, 0x408000DC, 0x2C040000, 0x418200D4, 0x2C040003, 0x4182000C, 0x3884FFFF,
            0x48000074, 0x3CE08089, 0x60E70B38, 0x39000008, 0x38A00000, 0x38800000, 0x3CC08088,
            0x60C6F368, 0xC3870000, 0xC3A60000, 0xFFDCE828, 0xFFDEF02A, 0xFFC0F210, 0x7F883C2E,
            0xC3A60008, 0xFF9CE828, 0xFF9CE02A, 0xFF80E210, 0xFFDEE02A, 0x2C050000, 0x4182000C,
            0xFC0AF040, 0x4180000C, 0xFD40F090, 0x7CA42B78, 0x38A50001, 0x38C60268, 0x2C050009,
            0x4082FFB0, 0x3CC08088, 0x60C6F53B, 0x39000000, 0x1D280268, 0x7CA930AE, 0x2C05000F,
            0x4082000C, 0x38A00002, 0x7CA931AE, 0x39080001, 0x2C080009, 0x4180FFE0, 0x1CA40268,
            0x38E0000F, 0x7CE531AE, 0x3CC08089, 0x60C62800, 0x98860001, 0x98860007, 0x48000028,
            0x38800000, 0x98890000, 0xB8610008, 0x80010104, 0x38210100, 0x7C0803A6, 0x7DC37378,
            0x88061BD1, 0x4800001C, 0xB8610008, 0x80010104, 0x38210100, 0x7C0803A6, 0x7DC37378,
            0x38000001,
        }
    };

    // Night time Mario Stadium [LittleCoaks]
    const DefaultGeckoCode sNightStadium = {
        0x80650678, 0x98030058,
        {0x98030058, 0x89240009, 0x2C090000, 0x4082000C, 0x3A400001, 0x9A44000A}};

    // Pitch Clock (10 seconds) [LittleCoaks]
    const DefaultGeckoCode sPitchClock = {
        0x806B4070, 0x540005ef,
        {0x3DC08089, 0x61CE0AE0, 0xA1CE0000, 0x2C0E0258, // last 4 bytes of this instruction determine pitch clock length
        0x40820008, 0x38000100, 0x540005EF}
    };

    // Remove Dingus Bunting [LittleCoaks]
    const DefaultGeckoCode sRemoveDingus = {
        0x8069811C, 0x38000001,
        {0x3DC08089, 0x61CE2899,
        0x880E0000, 0x70000010,
        0x2C000000, 0x41820008, 0x38000001}};

    // Store Random Batting Ints [Roeming]
    const DefaultGeckoCode sStoreRandBattingInts = {
        0x80651E68, 0x98040091,
        {0x98040091, 0x3CA08089,
        0x38A52684, 0x3CC0802F,
        0x38C6C010, 0x80850000,
        0xB0860000, 0x80850004,
        0xB0860002, 0xA0850018, 0xB0860004}}; // Store Rand1 at 802ec010, Rand2 at 802ec012 and Rand3, at 802ec014

    // Checksum Calculator [LitleCoaks]
    const DefaultGeckoCode sChecksum = {
        0x8000928c, 0,
        {0x9421FFB0, 0xBDC10008, 0x39C00000, 0x3A000000, 0x3DE0800E, 0x61EF877E, 0xA1EF0000,
         0x7DCF7214, 0x3DE0800E, 0x61EF8782, 0xA1EF0000, 0x7DCF7214, 0x3DE0800E, 0x61EF874C,
         0xA1EF0000, 0x7DCF7214, 0x3DE0803C, 0x61EF6726, 0x7DF078AE, 0x7DCF7214, 0x7DD07214,
         0x3DE08035, 0x61EF323B, 0x7DF078AE, 0x7DCF7214, 0x7DD07214, 0x3A100001, 0x2C100012,
         0x4180FFD0, 0x3DE08089, 0x61EF2AAA, 0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF2AAB,
         0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF09A1, 0x89EF0000, 0x7DCF7214, 0x3DE08089,
         0x61EF2857, 0x89EF0000, 0x7DCF7214, 0x3DE08036, 0x61EFF3A9, 0x89EF0000, 0x7DCF7214,
         0x3DE08089, 0x61EF09AA, 0x89EF0000, 0x7DCF7214, 0x3DE08087, 0x61EF2540, 0x89EF0000,
         0x7DCF7214, 0x3DE08089, 0x61EF0971, 0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF28A3,
         0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF294D, 0x89EF0000, 0x7DCF7214, 0x3DE08089,
         0x61EF296F, 0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF296B, 0x89EF0000, 0x7DCF7214,
         0x3DE08089, 0x61EF2973, 0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF2AD6, 0x89EF0000,
         0x7DCF7214, 0x3DE08089, 0x61EF2AD7, 0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF2AD8,
         0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF09BA, 0x89EF0000, 0x7DCF7214, 0x3DE08088,
         0x61EFF09D, 0x89EF0000, 0x7DCF7214, 0x3DE08088, 0x61EFF1F1, 0x89EF0000, 0x7DCF7214,
         0x3DE08088, 0x61EFF345, 0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF38AD, 0x89EF0000,
         0x7DCF7214, 0x3DE08089, 0x61EF09A3, 0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF3BAA,
         0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF28A4, 0x89EF0000, 0x7DCF7214, 0x3DE08089,
         0x61EF28CA, 0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF0971, 0x89EF0000, 0x7DCF7214,
         0x3DE08089, 0x61EF0AD9, 0x89EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF0B38, 0x81EF0000,
         0x7DCF7214, 0x3DE08089, 0x61EF0B3C, 0x81EF0000, 0x7DCF7214, 0x3DE08089, 0x61EF0B40,
         0x81EF0000, 0x7DCF7214, 0x3DE0802E, 0x61EFBFB8, 0x91CF0000, 0xB9C10008, 0x38210050,
         0x28180000}};  // 0x802EBFB8 == checksum addr

    // Hold Z for Easy Batting [Roeming]
    const DefaultGeckoCode sEasyBattingZ = {
        0x806A82B0, 0x28000000,
        {0x3CA08089, 0x38852990, 0x80040000, 0x1C000004, 0x38852A78, 0x7C840214,
        0x80040000, 0x1C000010,0x3885392C, 0x7C840214, 0xA0040000, 0x70000010, 0x28000000}
    };

    // Hazardless Stadiums [LittleCoaks, PeacockSlayer]
    const DefaultGeckoCode sHazardless = {
        0x80699508, 0x88a40009,
        {0x88A40009, 0x9421FFB0, 0xBDC10008, 0x3E803800, 0x62940007, 0x2C050002, 0x40820030,
         0x3E608070, 0x6273FC30, 0x3EA06000, 0x92B30000, 0x3EA03800, 0x92B3376C, 0x3E60807C,
         0x62739964, 0x3EA042C8, 0x92B30000, 0x92B30034, 0x3E608070, 0x62737CB8, 0x92930000,
         0x3E608072, 0x62734428, 0x92930000, 0x3E608073, 0x62739A28, 0x92930000, 0x3E608073,
         0x62736D08, 0x92930000, 0x3E608073, 0x627343A4, 0x3EA06000, 0x92B30000, 0x92B3000C,
         0xB9C10008, 0x38210050}
    };

    // Restrict Batter Pausing [LittleCoaks]
    const DefaultGeckoCode sRestrictBatterPausing = {
        0x806EED5C, 0xA0040006,
        {0x3CC08089, 0x60C6099D, 0x88060000, 0x3CC08089, 0x60C609AD, 0x88C60000,
        0x7CC03214, 0xA0040006, 0x2C060000, 0x41820008, 0x38000000}
    };

    // Highlight Ball Shadow [LittleCoaks]
    const DefaultGeckoCode sHighlightBallShadow = {
        0x806A844C, 0x41820224,
        {0x40820020, 0x3C80806A, 0x608485B8, 0x3D80C01D, 0x91840000, 0x3D80C005, 0x618C0008,
        0x9184001C}
    };

    // Fix Random Captain Select [LittleCoaks]
    const DefaultGeckoCode sFixRandomCaptain = {
        0x8063F7C4, 0x90040000,
        {0x90040000, 0x90040330}
    };

    // Duplicate Characters [PeacockSlayer, LittleCoaks]
    const DefaultGeckoCode sDuplicateCharacters = {
        0x8064f394, 0,
        {0x88030002, 0x3c40803c, 0x60426726, 0x88420000, 0x3ce08035, 0x60e730f7, 0x7f023a14,
         0x38400001, 0x98580000, 0x3c40803c, 0x6042672f, 0x88420000, 0x7f023a14, 0x38400001,
         0x98580000, 0x3c40803d, 0x60424400, 0x38e00000, 0x3b000000}
    };

    std::vector<DefaultGeckoCode> sRequiredCodes = {
      sGenerateGameID,
      sClearGameID_1,
      sClearGameID_2,
      sClearGameID_3,
      sClearPortInfo,
      sClearHitResult,
      sStorePortInfo_1,
      sStorePortInfo_2,
      sRememberWhoQuit_1,
      sRememberWhoQuit_2,
      sStoreRandBattingInts,
      sChecksum,
      sFixRandomCaptain
    };
};
