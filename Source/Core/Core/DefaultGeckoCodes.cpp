#include "Core/DefaultGeckoCodes.h"
#include "NetPlayProto.h"
#include "Config/NetplaySettings.h"
#include <VideoCommon/VideoConfig.h>

void DefaultGeckoCodes::Init(std::optional<std::vector<ClientCode>> client_codes, bool tagset_active, bool is_night,
                             bool disable_replays)
{
  TagSetActive = tagset_active;
  ClientCodes = client_codes;
  IsNight = is_night;
  DisableReplays = disable_replays;
  initiated = true;
}

void DefaultGeckoCodes::RunCodeInject()
{
  if (!initiated)
    return;

  aWriteAddr = 0x802ED200;  // starting asm write addr

  AddRequiredCodes();
  AddTagSetCodes();
  AddOptionalCodes();
}

void DefaultGeckoCodes::AddRequiredCodes()
{
  // enable rumble
  PowerPC::HostWrite_U8(0x1, aControllerRumble);

  // Boot to Main Menu
  if (PowerPC::HostRead_U32(aBootToMainMenu) == 0x38600001)
    PowerPC::HostWrite_U32(0x38600005, aBootToMainMenu);

  // Skip Mem Card Check
  PowerPC::HostWrite_U8(0x1, aSkipMemCardCheck);

  // Unlock Everything
  PowerPC::HostWrite_U8(0x2, aUnlockEverything_1);

  for (int i = 0; i <= 0x5; i++)
    PowerPC::HostWrite_U8(0x3, aUnlockEverything_2 + i);

  for (int i = 0; i <= 0x5; i++)
    PowerPC::HostWrite_U8(0x1, aUnlockEverything_3 + i);

  for (int i = 0; i <= 0x29; i++)
    PowerPC::HostWrite_U8(0x1, aUnlockEverything_4 + i);

  PowerPC::HostWrite_U8(0x1, aUnlockEverything_5);

  for (int i = 0; i <= 0x3; i++)
    PowerPC::HostWrite_U8(0x1, aUnlockEverything_7 + i);

  PowerPC::HostWrite_U16(0x0101, aUnlockEverything_8);

  PowerPC::HostWrite_U32(0x9867003F, aDefaultMercyOn);

  // Cool bat sound on Start Game
  PowerPC::HostWrite_U32(0x386001bb, aBatSound);

  // this is a very bad and last-minute "fix" to the pitcher stamina bug. i can't find the true
  // source of the bug so i'm manually fixing it with this line here. will remove soon once bug is
  // truly squashed
  if (PowerPC::HostRead_U8(0x8069BBDD) == 0xC5)
    PowerPC::HostWrite_U8(05, 0x8069BBDD);

  // handle asm writes for required code
  for (DefaultGeckoCode geckocode : sRequiredCodes)
    WriteAsm(geckocode);
}

void DefaultGeckoCodes::AddTagSetCodes()
{
  bool antiQuickPitch = true;
  bool manualFielderSelect = true;
  bool unlimitedExtraInnings = true;
  bool superstars = true;
  bool starSkills = true;
  bool antiDingusBunt = true;
  bool hazardless = false;
  bool gameModeration = false;
  bool defaultNineInnings = false;
  bool defaultDropSpotsOff = false;
  bool duplicateCharacters = false;
  bool duplicateChem = false;

  if (ClientCodes.has_value())
  {
    for (auto& code : ClientCodes.value())
    {
      if (code == ClientCode::DisableAntiQuickPitch)
        antiQuickPitch = false;
      else if (code == ClientCode::DisableManualFielderSelect)
        manualFielderSelect = false;
      else if (code == ClientCode::DisableUnlimitedExtraInnings)
        unlimitedExtraInnings = false;
      else if (code == ClientCode::DisableSuperstars)
        superstars = false;
      else if (code == ClientCode::DisableStarSkills)
        starSkills = false;
      else if (code == ClientCode::DisableAntiDingusBunt)
        antiDingusBunt = false;
      else if (code == ClientCode::EnableHazardless)
        hazardless = true;
      else if (code == ClientCode::EnableGameModeration)
        gameModeration = true;
      else if (code == ClientCode::DefaultNineInnings)
        defaultNineInnings = true;
      else if (code == ClientCode::DefaultDropSpotsOff)
        defaultDropSpotsOff = true;
      else if (code == ClientCode::DuplicateCharacters)
        duplicateCharacters = true;
      else if (code == ClientCode::DuplicateChem)
        duplicateChem = true;
    }
  }

  if (antiQuickPitch)
    WriteAsm(sAntiQuickPitch);

  if (manualFielderSelect)
    WriteAsm(sManualSelect);

  if (unlimitedExtraInnings)
    PowerPC::HostWrite_U32(0x380400f6, aUnlimitedExtraInnings);

  if (superstars)
  {
    for (int i = 0; i <= 0x35; i++)
      PowerPC::HostWrite_U8(0x1, aUnlockEverything_6 + i);
  }

  if (!starSkills)
  {
    PowerPC::HostWrite_U32(0x98c7003d, aDisableStarSkills);
    PowerPC::HostWrite_U32(0x38a00000, aDisableStarSkills_1);
  }

  if (antiDingusBunt)
    WriteAsm(sRemoveDingus);

  if (hazardless)
    WriteAsm(sHazardless);

  if (gameModeration)
  {
    PowerPC::HostWrite_U32(0x60000000, aPitchClock_1);
    PowerPC::HostWrite_U32(0x60000000, aPitchClock_2);
    PowerPC::HostWrite_U32(0x60000000, aPitchClock_3);
    WriteAsm(sPitchClock);

    WriteAsm(sRestrictBatterPausing);
  }

  if (defaultNineInnings)
    PowerPC::Write_U32(0x38000004, aDefaultNineInnings);

  if (defaultDropSpotsOff)
  {
    PowerPC::Write_U32(0x98C70048, aDefaultDropSpotsOff);
    PowerPC::Write_U32(0x98C7004C, aDefaultDropSpotsOff_1);
  }

  if (duplicateCharacters)
  {
    WriteAsm(sDuplicateCharacters);

    for (int i = 0; i <= 0x35; i++)
      PowerPC::HostWrite_U8(0x0, aDuplicate_1 + i);

    for (int i = 0; i <= 0x23; i++)
      PowerPC::HostWrite_U8(0xff, aDuplicate_1 + i);

    PowerPC::HostWrite_U32(0x6000000, aDuplicate_3);
    PowerPC::HostWrite_U32(0x6000000, aDuplicate_4);
    PowerPC::HostWrite_U32(0x6000000, aDuplicate_5);
    PowerPC::HostWrite_U32(0x6000000, aDuplicate_6);
    PowerPC::HostWrite_U32(0x6000000, aDuplicate_7);
    PowerPC::HostWrite_U32(0x6000000, aDuplicate_8);
    PowerPC::HostWrite_U32(0x6000000, aDuplicate_9);
    PowerPC::HostWrite_U32(0x6000000, aDuplicate_10);
    PowerPC::HostWrite_U32(0x6000000, aDuplicate_11);
    PowerPC::HostWrite_U32(0x6000000, aDuplicate_12);

    PowerPC::HostWrite_U32(0x2C0000FF, aDuplicate_13);
    PowerPC::HostWrite_U32(0x2C0000FF, aDuplicate_14);
  }

  if (duplicateChem)
  {
    for (int i = 0; i < sizeof(aDuplicateChem); i++)
      PowerPC::HostWrite_U8(0x63, aDuplicateChem[i]);
  }
}

void DefaultGeckoCodes::AddOptionalCodes()
{
  if (g_ActiveConfig.bTrainingModeOverlay && !TagSetActive)
    WriteAsm(sEasyBattingZ);


  // Rest of this is netplay-only codes
  if (!NetPlay::IsNetPlayRunning())
    return;
  
  if (IsNight)
  {
    WriteAsm(sNightStadium);
  }

  if (DisableReplays)
  {
    if (PowerPC::HostRead_U32(aDisableReplays) == 0x38000001)
      PowerPC::HostWrite_U32(0x38000000, aDisableReplays);
  }

  // TODO: uncomment these lines once rng syncing is added in a future update
  //if (Config::Get(Config::NETPLAY_DISABLE_MUSIC))
  //{
  //  PowerPC::HostWrite_U32(0x38000000, aDisableMusic_1);
  //  PowerPC::HostWrite_U32(0x38000000, aDisableMusic_2);
  //}

  //if (Config::Get(Config::NETPLAY_HIGHLIGHT_BALL_SHADOW))
  //{
  //  WriteAsm(sHighlightBallShadow);
  //}

  // if (Config::Get(Config::NETPLAY_NEVER_CULL))
  //{
  //   PowerPC::HostWrite_U32(0x38000007, aNeverCull_1);
  //   PowerPC::HostWrite_U32(0x38000001, aNeverCull_2);
  //   PowerPC::HostWrite_U32(0x38000001, aNeverCull_3);
  //   if (PowerPC::HostRead_U32(aNeverCull_4) == 0x881a0093)
  //     PowerPC::HostWrite_U32(0x38000003, aNeverCull_4);
  // }
}


// calls this each time you want to write a code
void DefaultGeckoCodes::WriteAsm(DefaultGeckoCode CodeBlock)
{
  // METHODOLOGY:
  // use aWriteAddr as starting asm write addr
  // we compute a value, branchAmount, which tells how far we have to branch to get from the injection to aWriteAddr
  // do fancy bit wise math to formulate the hex value of the desired branch instruction
  // writes in first instruction in code block to aWriteAddr, increment aWriteAddr by 4, repeat for all code lines
  // once code block is finished, compute another branch instruction back to injection addr and write it in
  // repeat for all codes
  u32 branchToCode = 0x48000000;
  u32 baseAddr = aWriteAddr & 0x03ffffff;
  u32 codeAddr = CodeBlock.addr & 0x03ffffff;
  u32 branchAmount = (baseAddr - codeAddr) & 0x03ffffff;

  branchToCode += branchAmount;

  // write asm to free memory
  for (int i = 0; i < CodeBlock.codeLines.size(); i++)
  {
    PowerPC::HostWrite_U32(CodeBlock.codeLines[i], aWriteAddr);
    aWriteAddr += 4;
  }

  // write branches
  u32 branchFromCode = 0x48000000;
  baseAddr = aWriteAddr & 0x03ffffff;
  codeAddr = CodeBlock.addr & 0x03ffffff;
  branchAmount = (codeAddr - baseAddr) & 0x03ffffff;

  // branch at the end of the gecko code
  branchFromCode += branchAmount + 4;
  PowerPC::HostWrite_U32(branchFromCode, aWriteAddr);
  aWriteAddr += 4;

  if (CodeBlock.conditionalVal != 0 && PowerPC::HostRead_U32(CodeBlock.addr) != CodeBlock.conditionalVal)
    return;
  // branch at injection location
  PowerPC::HostWrite_U32(branchToCode, CodeBlock.addr);
}

// end
