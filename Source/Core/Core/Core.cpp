// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Core.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>
#include <queue>
#include <utility>
#include <variant>

#include <fmt/chrono.h>
#include <fmt/format.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "AudioCommon/AudioCommon.h"

#include "Common/Assert.h"
#include "Common/TimeUtil.h"
#include "Core/HW/AddressSpace.h"
#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FPURoundMode.h"
#include "Common/FatFsUtil.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Common/Version.h"

#include "Core/AchievementManager.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/CPUThreadConfigCallback.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/DSPEmulator.h"
#include "Core/DolphinAnalytics.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FreeLookManager.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GBAPad.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/HW.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/IOS/IOS.h"
#include "Core/MemTools.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/GDBStub.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"
#include "Core/System.h"
#include "Core/WiiRoot.h"

//#include "Core/LocalPlayers.h"
#include "Core/LocalPlayersConfig.h"


#ifdef USE_MEMORYWATCHER
#include "Core/MemoryWatcher.h"
#endif

#include "DiscIO/RiivolutionPatcher.h"
#include "Core/MSB_StatTracker.h"
#include "Core/MSB_GenerateQuickMatchSetupGeckoCode.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCAdapter.h"

#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FrameDumper.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PerformanceMetrics.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoEvents.h"
#include "VideoCommon/VideoConfig.h"

#include "Common/TagSet.h"
#include "Core/GeckoCodeConfig.h"

namespace Core
{
static bool s_wants_determinism;

// Declarations and definitions
static bool s_is_stopping = false;
static bool s_hardware_initialized = false;
static bool s_is_started = false;
static Common::Flag s_is_booting;
static std::thread s_emu_thread;
static std::vector<StateChangedCallbackFunc> s_on_state_changed_callbacks;

static std::thread s_cpu_thread;
static bool s_is_throttler_temp_disabled = false;
static std::atomic<double> s_last_actual_emulation_speed{1.0};
static bool s_frame_step = false;
static std::atomic<bool> s_stop_frame_step;

static std::optional<TagSet> tagset_local = std::nullopt;
static std::optional<TagSet> tagset_netplay = std::nullopt;
static bool previousContactMade = false;
static bool runNetplayGameFunctions = true;

static int avgPing = 0;
static int nPing = 0;
static int nLagSpikes = 0;
static int previousPing = 50;

static int draftTimer = 0;
static int nextGolferID = 0;

#ifdef USE_MEMORYWATCHER
static std::unique_ptr<MemoryWatcher> s_memory_watcher;
#endif

static std::unique_ptr<StatTracker> s_stat_tracker;

struct HostJob
{
  std::function<void()> job;
  bool run_after_stop;
};
static std::mutex s_host_jobs_lock;
static std::queue<HostJob> s_host_jobs_queue;
static Common::Event s_cpu_thread_job_finished;

static thread_local bool tls_is_cpu_thread = false;
static thread_local bool tls_is_gpu_thread = false;
static thread_local bool tls_is_host_thread = false;

static void EmuThread(Core::System& system, std::unique_ptr<BootParameters> boot,
                      WindowSystemInfo wsi);

static Common::EventHook s_frame_presented = AfterPresentEvent::Register(
    [](auto& present_info) {
      const double last_speed_denominator = g_perf_metrics.GetLastSpeedDenominator();
      // The denominator should always be > 0 but if it's not, just return 1
      const double last_speed = last_speed_denominator > 0.0 ? (1.0 / last_speed_denominator) : 1.0;

      if (present_info.reason != PresentInfo::PresentReason::VideoInterfaceDuplicate)
        Core::Callback_FramePresented(last_speed);
    },
    "Core Frame Presented");

static GameName mGameBeingPlayed = GameName::UnknownGame;
const std::map<std::string, GameName> mGameMap = {{"GYQE01", GameName::MarioBaseball},
                                                  {"GFTE01", GameName::ToadstoolTour}};


bool GetIsThrottlerTempDisabled()
{
  return s_is_throttler_temp_disabled;
}

void SetIsThrottlerTempDisabled(bool disable)
{
  s_is_throttler_temp_disabled = disable;
}

double GetActualEmulationSpeed()
{
  return s_last_actual_emulation_speed;
}

void FrameUpdateOnCPUThread()
{
  if (NetPlay::IsNetPlayRunning() && Core::IsRunningAndStarted())
  {
    if (mGameBeingPlayed != GameName::MarioBaseball)
      NetPlay::NetPlayClient::SendTimeBase();

    else if (s_stat_tracker)
    {
      // Figure out if client is hosting via netplay settings. Could use local player as well
      //bool is_hosting = NetPlay::GetNetSettings().m_IsHosting;
      std::string opponent_name = "";
      /*
      for (auto player : NetPlay::NetPlayClient::GetPlayers()){
        if (!NetPlay::NetPlayClient::IsLocalPlayer(player.pid)){
          opponent_name = player.name;
          break;
        }
      }*/
      s_stat_tracker->setNetplaySession(true, opponent_name);
    }
  }
  else if (s_stat_tracker && mGameBeingPlayed == GameName::MarioBaseball)
  {
    s_stat_tracker->setNetplaySession(false);
  }
}

// this function is called from PatchEngine.cpp (ApplyFramePatches()) safely
// we can do memory reads/writes without worrying
// anything that needs to read or write to memory should be getting run from here
void RunRioFunctions(const Core::CPUThreadGuard& guard)
{
  auto& system = Core::System::GetInstance();
  u64 frame = system.GetMovie().GetCurrentFrame();

  if (mGameBeingPlayed == GameName::MarioBaseball)
  {
    s_stat_tracker->Run(guard);

    if (PowerPC::MMU::HostRead_U32(guard, aGameId) == 0)
    {
      runNetplayGameFunctions = true;
    }

    if (NetPlay::IsNetPlayRunning())
    {
      // send checksum for desync detection
      if (frame % 60)
      {
        u8 checksumId = (frame / 60) & 0xF;
        u32 checksum = PowerPC::MMU::HostRead_U32(guard, 0x802EBFB8);
        NetPlay::NetPlayClient::SendChecksum(checksumId, frame, checksum);
      }
      if (runNetplayGameFunctions)
      {
        SetNetplayerUserInfo();
        NetPlay::NetPlayClient::SendGameID(PowerPC::MMU::HostRead_U32(guard, aGameId));
        runNetplayGameFunctions = false;
      }
    }
    SetAvgPing(guard);
    if (frame % 60 == 0) // if it's the 1st frame of second
      RunDraftTimer(guard);
  }

  DisplayPlayerNames(guard);
  if (Gecko::isLoadingFromHUD)
    MSBQuickMatchBattingOrderMsg(guard, Gecko::HUDState);
  AutoGolfMode(guard);
  TrainingMode(guard);
}

void OnFrameEnd()
{
#ifdef USE_MEMORYWATCHER
  if (s_memory_watcher)
  {
    ASSERT(IsCPUThread());
    CPUThreadGuard guard(Core::System::GetInstance());

    s_memory_watcher->Step(guard);
  }
#endif
}

void AutoGolfMode(const Core::CPUThreadGuard& guard)
{
  switch (mGameBeingPlayed) {
  case GameName::MarioBaseball:
    MSSBCalculateNextGolfer(guard, nextGolferID);
    break;
  case GameName::ToadstoolTour:
    MGTTCalculateNextGolfer(guard, nextGolferID);
    break;
  }
}

void MSSBCalculateNextGolfer(const Core::CPUThreadGuard& guard, int& nextGolfer)
{
  u8 BatterPort = PowerPC::MMU::HostRead_U8(guard, aBatterPort);
  u8 FielderPort = PowerPC::MMU::HostRead_U8(guard, aFielderPort);
  bool isField = PowerPC::MMU::HostRead_U8(guard, aIsField) == 1;

  // means game hasn't started yet
  if (BatterPort == 0)
    return;

  // makes the player who paused the golfer
  if (PowerPC::MMU::HostRead_U8(guard, aWhoPaused) == 2)
    isField = true;

  // add minigame functionality
  int minigameId = PowerPC::MMU::HostRead_U8(guard, aMinigameID);
  if (minigameId == 3 || minigameId == 1)
  {
    BatterPort = PowerPC::MMU::HostRead_U8(guard, aBarrelBatterPort) + 1;
    isField = false;
  }
  else if (minigameId == 2)
  {
    FielderPort = PowerPC::MMU::HostRead_U8(guard, aWallBallPort) + 1;
    isField = true;
  }

  // evaluate which player should be golfer here
  nextGolfer = isField ? FielderPort - 1 : BatterPort - 1;  // subtract 1 since m_pad_map uses 0->3 instead of 1->4
}

void MGTTCalculateNextGolfer(const Core::CPUThreadGuard& guard, int& nextGolfer)
{
  u8 golferIndex = PowerPC::MMU::HostRead_U8(guard, aCurrentGolfer);
  switch(golferIndex) {
    case 0:
      nextGolfer = PowerPC::MMU::HostRead_U8(guard, aPlayer1Port);
      break;
    case 1: 
      nextGolfer = PowerPC::MMU::HostRead_U8(guard, aPlayer2Port);
      break;
    case 2:
      nextGolfer = PowerPC::MMU::HostRead_U8(guard, aPlayer3Port);
      break;
    case 3:
      nextGolfer = PowerPC::MMU::HostRead_U8(guard, aPlayer4Port);
      break;
    default:
      break;
  }
}

bool IsGolfMode()
{
  bool out = false;
  if (NetPlay::IsNetPlayRunning())
    out = NetPlay::NetPlayClient::isGolfMode();

  return out;
}

// TODO: add stats for the following: base runner coordinates; ball coords frame before being caught, character coords after diving/jumping/wall jumping
void TrainingMode(const Core::CPUThreadGuard& guard)
{
  // if training mode config is on and not ranked netplay
  // using this feature on ranked can be considered an unfair advantage
  if (!g_ActiveConfig.bTrainingModeOverlay || isTagSetActive())
    return;

  if (mGameBeingPlayed == GameName::MarioBaseball)
  {
    // bool isPitchThrown = PowerPC::MMU::HostRead_U8(0x80895D6C) == 1 ? true : false;
    bool isField = PowerPC::MMU::HostRead_U8(guard, aIsField) == 1 ? true : false;
    bool isInGame = PowerPC::MMU::HostRead_U8(guard, aIsInGame) == 1 ? true : false;
    bool ContactMade = PowerPC::MMU::HostRead_U8(guard, aContactMade) == 1 ? true : false;

    // Batting Training Mode stats
    if (ContactMade && !previousContactMade)
    {
      u8 BatterPort = PowerPC::MMU::HostRead_U8(guard, aBatterPort);
      if (BatterPort > 0)
        BatterPort--;
      u32 stickDirectionAddr = 0x8089392D + (0x10 * BatterPort);
      float contactQuality = PowerPC::MMU::HostRead_F32(guard, aAB_ContactQuality);
      u16 contactFrame = PowerPC::MMU::HostRead_U16(guard, aContactFrame);
      u8 typeOfContact_Value = PowerPC::MMU::HostRead_U8(guard, aTypeOfContact);
      std::string typeOfContact;
      u8 inputDirection_Value = PowerPC::MMU::HostRead_U8(guard, stickDirectionAddr) & 0xF;
      std::string inputDirection;
      int chargeUp =
          static_cast<int>(roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aChargeUp)) * 100));
      int chargeDown = static_cast<int>(
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aChargeDown)) * 100));

      float angle = roundf((float)PowerPC::MMU::HostRead_U16(guard, aBallAngle) * 36000 / 4096) /
                    100;  // 0x400 == 90°, 0x800 == 180°, 0x1000 == 360°
      float xVelocity =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aBallVelocity_X)) * 6000) /
          100;  // * 60 cause default units are meters per frame
      float yVelocity =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aBallVelocity_Y)) * 6000) / 100;
      float zVelocity =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aBallVelocity_Z)) * 6000) / 100;
      float netVelocity = vectorMagnitude(xVelocity, yVelocity, zVelocity);

      // convert type of contact to string
      if (typeOfContact_Value == 0)
        typeOfContact = "Sour - Left";
      else if (typeOfContact_Value == 1)
        typeOfContact = "Nice - Left";
      else if (typeOfContact_Value == 2)
        typeOfContact = "Perfect";
      else if (typeOfContact_Value == 3)
        typeOfContact = "Nice - Right";
      else  // typeOfContact_Value == 4
        typeOfContact = "Sour - Right";

      // convert input direction to string
      if (inputDirection_Value == 0)
        inputDirection = "None";
      else if (inputDirection_Value == 1)
        inputDirection = "Left";
      else if (inputDirection_Value == 2)
        inputDirection = "Right";
      else if (inputDirection_Value == 4)
        inputDirection = "Down";
      else if (inputDirection_Value == 8)
        inputDirection = "Up";
      else if (inputDirection_Value == 5)
        inputDirection = "Down/Left";
      else if (inputDirection_Value == 9)
        inputDirection = "Up/Left";
      else if (inputDirection_Value == 6)
        inputDirection = "Down/Right";
      else if (inputDirection_Value == 10)
        inputDirection = "Up/Right";
      else if (inputDirection_Value == 3)
        inputDirection = "Left/Right";
      else if (inputDirection_Value == 12)
        inputDirection = "Up/Down";
      else
        inputDirection = "Unknown";

      int totalCharge;
      chargeUp == 100 ? totalCharge = chargeDown : totalCharge = chargeUp;

      OSD::AddTypedMessage(OSD::MessageType::TrainingModeBatting,
                           fmt::format("Batting Data:                    \n"
                                       "Contact Frame:  {}\n"
                                       "Type of Contact:  {}\n"
                                       "Contact Quality: {}\n"
                                       "Input Direction:  {}\n"
                                       "Charge Percent:  {}%\n"
                                       "Ball Angle:  {}°\n\n"
                                       "Exit Velocities:  \n"
                                       "X :  {} m/s  -->  {} mph\n"
                                       "Y:  {} m/s  -->  {} mph\n"
                                       "Z :  {} m/s  -->  {} mph\n"
                                       "Net:  {} m/s  -->  {} mph",
                                       contactFrame, typeOfContact, contactQuality, inputDirection,
                                       totalCharge, angle, xVelocity, ms_to_mph(xVelocity),
                                       yVelocity, ms_to_mph(yVelocity), zVelocity,
                                       ms_to_mph(zVelocity), netVelocity, ms_to_mph(netVelocity)),
                           8000);  // message time & color
    }

    // Coordinate data
    if (isInGame)
    {
      float BallPos_X =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aBallPosition_X)) * 100) / 100;
      float BallPos_Y =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aBallPosition_Y)) * 100) / 100;
      float BallPos_Z =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aBallPosition_Z)) * 100) / 100;
      float BallVel_X =
          isField ?
              roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aBallVelocity_X)) * 6000) / 100 :
              roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aPitchedBallVelocity_X)) * 6000) /
                  100;
      float BallVel_Y =
          isField ? RoundZ(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aBallVelocity_Y)) * 6000) /
                        100 :  // floor small decimal to prevent weirdness
              RoundZ(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aPitchedBallVelocity_Y)) * 6000) /
                  100;
      float BallVel_Z =
          isField ?
              roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aBallVelocity_Z)) * 6000) / 100 :
              roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, aPitchedBallVelocity_Z)) * 6000) /
                  100;
      float BallVel_Net = roundf(vectorMagnitude(BallVel_X, BallVel_Y, BallVel_Z) * 100) / 100;

      int baseOffset = 0x268 * PowerPC::MMU::HostRead_U8(
                                   guard, 0x80892801);  // used to get offsed for baseFielderAddr
      u32 baseFielderAddr = 0x8088F368 + baseOffset;    // 0x0 == x; 0x8 == y; 0xc == z

      float FielderPos_X =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, baseFielderAddr)) * 100) / 100;
      float FielderPos_Y =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, baseFielderAddr + 0xc)) * 100) / 100;
      float FielderPos_Z =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, baseFielderAddr + 0x8)) * 100) / 100;
      float FielderVel_X =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, baseFielderAddr + 0x30)) * 6000) /
          100;
      // float FielderVel_Y = roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(baseFielderAddr + 0x15C))
      // * 6000) / 100; // this addr is wrong
      float FielderVel_Z =
          roundf(u32ToFloat(PowerPC::MMU::HostRead_U32(guard, baseFielderAddr + 0x34)) * 6000) /
          100;
      float FielderVel_Net =
          roundf(vectorMagnitude(FielderVel_X, 0 /*FielderVel_Y*/, FielderVel_Z) * 100) / 100;

      OSD::AddTypedMessage(
          OSD::MessageType::TrainingModeBallCoordinates,
          fmt::format("Ball Coordinates:                \n"
                      "X:  {}\n"
                      "Y:  {}\n"
                      "Z:  {}\n\n"
                      "Ball Velocity:  \n"
                      "X:  {} m/s  -->  {} mph\n"
                      "Y:  {} m/s  -->  {} mph\n"
                      "Z:  {} m/s  -->  {} mph\n"
                      "Net:  {} m/s  -->  {} mph\n",
                      BallPos_X, BallPos_Y, BallPos_Z, BallVel_X, ms_to_mph(BallVel_X), BallVel_Y,
                      ms_to_mph(BallVel_Y), BallVel_Z, ms_to_mph(BallVel_Z), BallVel_Net,
                      ms_to_mph(BallVel_Net)),
          200, OSD::Color::CYAN);  // short time cause we don't want this info to linger

      OSD::AddTypedMessage(OSD::MessageType::TrainingModeFielderCoordinates,
                           fmt::format("Fielder Coordinates:             \n"
                                       "X:  {}\n"
                                       "Y:  {}\n"
                                       "Z:  {}\n\n"
                                       "Fielder Velocity: \n"
                                       "X:  {} m/s  -->  {} mph\n"
                                       //"Y:  {} m/s  -->  {} mph\n"
                                       "Z:  {} m/s  -->  {} mph\n"
                                       "Net:  {} m/s  -->  {} mph",
                                       FielderPos_X, FielderPos_Y, FielderPos_Z, FielderVel_X,
                                       ms_to_mph(FielderVel_X),
                                       // FielderVel_Y, ms_to_mph(FielderVel_Y),
                                       FielderVel_Z, ms_to_mph(FielderVel_Z), FielderVel_Net,
                                       ms_to_mph(FielderVel_Net)),
                           200, OSD::Color::CYAN);
    }

    previousContactMade = ContactMade;
  }
  else if (mGameBeingPlayed == GameName::ToadstoolTour)
  {
    float DistanceRemainingToHole = PowerPC::MMU::HostRead_F32(guard, aDistanceRemainingToHole);
    int ShotAccuracy = PowerPC::MMU::HostRead_U32(guard, aShotAccuracy);
    u32 PowerMeterDistance = PowerPC::MMU::HostRead_U32(guard, aPowerMeterDistance);
    float CurrentShotAimAngle = PowerPC::MMU::HostRead_F32(guard, aCurrentShotAimAngle);
    float SimLineEndpointX = PowerPC::MMU::HostRead_F32(guard, aSimLineEndpointX);
    float SimLineEndpointZ = PowerPC::MMU::HostRead_F32(guard, aSimLineEndpointZ);
    float SimLineEndpointY = PowerPC::MMU::HostRead_F32(guard, aSimLineEndpointY);
    int PreShotVerticalAdjustment = PowerPC::MMU::HostRead_U32(guard, aPreShotVerticalAdjustment);
    int PreShotHorizontalAdjustment =
        PowerPC::MMU::HostRead_U32(guard, aPreShotHorizontalAdjustment);
    int ActiveShotVerticalAdjustment =
        PowerPC::MMU::HostRead_U32(guard, aActiveShotVerticalAdjustment);
    int ActiveShotHorizontalAdjustment =
        PowerPC::MMU::HostRead_U32(guard, aActiveShotHorizontalAdjustment);

    OSD::AddTypedMessage(OSD::MessageType::TrainingModeGolfing,
                         fmt::format("Golf Training Mode:                    \n"
                                     "Distance to Hole:  {}\n"
                                     "Shot Aim Angle:  {}\n"
                                     "Vertical Adj:  {} / {}\n"
                                     "Horizontal Adj:  {} / {}\n"
                                     "Shot Accuracy:  {}\n"
                                     "Power Meter Accuracy:  {}\n"
                                     "Ball Sim Aim X:  {}\n"
                                     "Ball Sim Aim Y:  {}\n"
                                     "Ball Sim Aim Z:  {}\n",
                                     DistanceRemainingToHole, CurrentShotAimAngle,
                                     PreShotVerticalAdjustment, ActiveShotVerticalAdjustment,
                                     PreShotHorizontalAdjustment, ActiveShotHorizontalAdjustment,
                                     ShotAccuracy, PowerMeterDistance, SimLineEndpointX,
                                     SimLineEndpointY, SimLineEndpointZ),
                         8000);  // message time & color
  }
}

void DisplayPlayerNames(const Core::CPUThreadGuard& guard)
{
  if (!g_ActiveConfig.bShowPlayerNames)
    return;

  std::string P1 = LocalPlayers::m_local_player_1.GetUsername();
  std::string P2 = LocalPlayers::m_local_player_2.GetUsername();
  std::string P3 = LocalPlayers::m_local_player_3.GetUsername();
  std::string P4 = LocalPlayers::m_local_player_4.GetUsername();
  std::vector<std::string> LocalPlayerList{P1, P2, P3, P4};
  std::array<u32, 4> portColor = {
      {OSD::Color::RED, OSD::Color::BLUE, OSD::Color::YELLOW, OSD::Color::GREEN}};

  switch (mGameBeingPlayed)
  {
  case GameName::MarioBaseball:
  {
    u8 BatterPort = PowerPC::MMU::HostRead_U8(guard, aBatterPort);
    u8 FielderPort = PowerPC::MMU::HostRead_U8(guard, aFielderPort);
    if (BatterPort == 0 || FielderPort == 0)  // game hasn't started yet; do not continue func
      break;

    // subtract 1 from each port so they can be used as indeces in the arrays
    BatterPort--;
    FielderPort--;

    std::string batterName = "";
    std::string fielderName = "";

    // Run using NetPlay Nicknames
    if (NetPlay::IsNetPlayRunning())
    {
      batterName = NetPlay::NetPlayClient::GetNetplayNames(BatterPort);
      fielderName = NetPlay::NetPlayClient::GetNetplayNames(FielderPort);
    }
    // Run using Local Players
    else
    {
      if (BatterPort < 4)
        batterName = LocalPlayerList[BatterPort];
      if (FielderPort < 4)
        fielderName = LocalPlayerList[FielderPort];
    }

    // check for valid user
    if (batterName != "")
    {
      OSD::AddTypedMessage(OSD::MessageType::CurrentBatter, fmt::format("Batter: {}", batterName),
                           OSD::Duration::SHORT, portColor[BatterPort]);
    }

    // check for valid user
    if (fielderName != "")
    {
      OSD::AddTypedMessage(OSD::MessageType::CurrentFielder,
                           fmt::format("Fielder: {}", fielderName), OSD::Duration::SHORT,
                           portColor[FielderPort]);
    }

    break;
  }
  case GameName::ToadstoolTour:
  {
    if (PowerPC::MMU::HostRead_U8(guard, aIsGolfMatch) == 0)
      break;

    u8 GolferPort = PowerPC::MMU::HostRead_U8(guard, aCurrentGolfer);
    switch (GolferPort)
    {
    case 0:
      GolferPort = PowerPC::MMU::HostRead_U8(guard, aPlayer1Port);
      break;
    case 1:
      GolferPort = PowerPC::MMU::HostRead_U8(guard, aPlayer2Port);
      break;
    case 2:
      GolferPort = PowerPC::MMU::HostRead_U8(guard, aPlayer3Port);
      break;
    case 3:
      GolferPort = PowerPC::MMU::HostRead_U8(guard, aPlayer4Port);
      break;
    default:
      break;
    }
    std::string GolferName = "";

    if (NetPlay::IsNetPlayRunning())
    {
      GolferName = NetPlay::NetPlayClient::GetNetplayNames(GolferPort);
    }
    else if (GolferPort < 4)
    {
      GolferName = LocalPlayerList[GolferPort];
    }

    // check for valid user
    if (GolferName != "")
    {
      OSD::AddTypedMessage(OSD::MessageType::CurrentBatter, fmt::format("Golfer: {}", GolferName),
                           OSD::Duration::SHORT, portColor[GolferPort]);
    }
    break;
  }
  }
}

void RunDraftTimer(const Core::CPUThreadGuard& guard)
{
  u8 scene = PowerPC::MMU::HostRead_U8(guard, aSceneId);

  if (scene < 0x9)
    draftTimer = 0;

  else if (scene < 0xC) // pause clock after draft
  {
    draftTimer++;
    u32 draftMinutes = draftTimer / 60;
    u32 draftSeconds = draftTimer % 60;
    if (g_ActiveConfig.bDraftTimer)
    {
      if (draftSeconds >= 10)
      {
        OSD::AddTypedMessage(OSD::MessageType::DraftTimer,
                             fmt::format("Draft:  {}:{}", draftMinutes, draftSeconds), 2000);
      }
      else
      {
        OSD::AddTypedMessage(OSD::MessageType::DraftTimer,
                             fmt::format("Draft:  {}:0{}", draftMinutes, draftSeconds), 2000);
      }
    }
  }
}

void MSBQuickMatchBattingOrderMsg(const Core::CPUThreadGuard& guard, MSBQuickMatchGameState& state)
{
  // Validate state has been initialized
  if (!state.firstBatter.has_value())
  {
    INFO_LOG_FMT(COMMON, "State.firstBatter doesn't have value - no message");
    return;
  }

  // make batting order message if not in-game
  RelNumber rel = static_cast<RelNumber>(PowerPC::MMU::HostRead_U16(guard, aRelNumber));
  if (rel == RelNumber::MainMenu)
  {
    // create message
    OSD::AddTypedMessage
    (
      OSD::MessageType::QuickMatchBattingOrder,
      fmt::format
      (
        "Quick Match Setup V1 \n"
        "The match will load to 1 pitch before the crash.\n"
        "Everything will be set automatically.\n"
        "Just press A and ignore the graphics.\n"
      ), 
      OSD::Duration::NORMAL,
      OSD::Color::BLUE
    );
  }
}

// rounds to 2 decimal places
float u32ToFloat(u32 value)
{
  union{
    u32 num;
    float fnum;
  } float_converter;
  
  float_converter.num = value;
  return float_converter.fnum;
}

float ms_to_mph(float MetersPerSecond)
{
  return roundf(MetersPerSecond * 223.7) / 100;
}

float vectorMagnitude(float x, float y, float z)
{
  float sum = pow(x, 2) + pow(y, 2) + pow(z, 2);
  return roundf(sqrt(sum) * 100) / 100;
}

float RoundZ(float num)
{
  if (num < 50 && num > -50)
    num = 0;
  return roundf(num);
}

void SetAvgPing(const Core::CPUThreadGuard& guard)
{
  if (!NetPlay::IsNetPlayRunning())
    return;

  // checks if GameID is set and that the end game flag hasn't been hit yet
  bool inGame = PowerPC::MMU::HostRead_U32(guard, aGameId) != 0 /*&& PowerPC::MMU::HostRead_U8(aEndOfGameFlag) == 0*/ ?
                    true :
                    false;
  if (!inGame) {
    avgPing = 0;
    nPing = 0;
    nLagSpikes = 0;
    previousPing = 50;
    return;
  }
  int currentPing = NetPlay::NetPlayClient::sGetPlayersMaxPing();
  nPing += 1;
  avgPing = ((avgPing * (nPing - 1)) + currentPing) / nPing;

  // "Lag Spike" definition; currently just checks if ping is more than 150
  // should probably make a better definition in the future
  if (currentPing >= avgPing * 2 && currentPing >= 40 && previousPing <= avgPing * 1.2) {
    nLagSpikes += 1;
  }
  previousPing = currentPing;

  // tell the stat tracker what the new avg ping is
  if (s_stat_tracker)
  {
    s_stat_tracker->setAvgPing(avgPing);
    s_stat_tracker->setLagSpikes(nLagSpikes);
  }
}

void SetNetplayerUserInfo()
{
  // tell the stat tracker who the players are
  if (s_stat_tracker)
  {
    s_stat_tracker->setNetplayerUserInfo(NetPlay::NetPlayClient::getNetplayerUserInfo());
  }
  else {
    s_stat_tracker = std::make_unique<StatTracker>();
    s_stat_tracker->init();
    s_stat_tracker->setNetplayerUserInfo(NetPlay::NetPlayClient::getNetplayerUserInfo());
  }
}


// Display messages and return values

// Formatted stop message
std::string StopMessage(bool main_thread, std::string_view message)
{
  return fmt::format("Stop [{} {}]\t{}", main_thread ? "Main Thread" : "Video Thread",
                     Common::CurrentThreadId(), message);
}

void DisplayMessage(std::string message, int time_in_ms)
{
  if (!IsRunning())
    return;

  // Actually displaying non-ASCII could cause things to go pear-shaped
  if (!std::all_of(message.begin(), message.end(), Common::IsPrintableCharacter))
    return;

  OSD::AddMessage(std::move(message), time_in_ms);
}

bool IsRunning()
{
  return (GetState() != State::Uninitialized || s_hardware_initialized) && !s_is_stopping;
}

bool IsRunningAndStarted()
{
  return s_is_started && !s_is_stopping;
}

bool IsRunningInCurrentThread()
{
  return IsRunning() && IsCPUThread();
}

bool IsCPUThread()
{
  return tls_is_cpu_thread;
}

bool IsGPUThread()
{
  return tls_is_gpu_thread;
}

bool IsHostThread()
{
  return tls_is_host_thread;
}

bool WantsDeterminism()
{
  return s_wants_determinism;
}

// This is called from the GUI thread. See the booting call schedule in
// BootManager.cpp
bool Init(Core::System& system, std::unique_ptr<BootParameters> boot, const WindowSystemInfo& wsi)
{
  if (s_emu_thread.joinable())
  {
    if (IsRunning())
    {
      PanicAlertFmtT("Emu Thread already running");
      return false;
    }

    // The Emu Thread was stopped, synchronize with it.
    s_emu_thread.join();
  }

  // Drain any left over jobs
  HostDispatchJobs();

  INFO_LOG_FMT(BOOT, "Starting core = {} mode", system.IsWii() ? "Wii" : "GameCube");
  INFO_LOG_FMT(BOOT, "CPU Thread separate = {}", system.IsDualCoreMode() ? "Yes" : "No");

  Host_UpdateMainFrame();  // Disable any menus or buttons at boot

  // Manually reactivate the video backend in case a GameINI overrides the video backend setting.
  VideoBackendBase::PopulateBackendInfo(wsi);

  // Issue any API calls which must occur on the main thread for the graphics backend.
  WindowSystemInfo prepared_wsi(wsi);
  g_video_backend->PrepareWindow(prepared_wsi);

  // Start the emu thread
  s_is_booting.Set();
  s_emu_thread = std::thread(EmuThread, std::ref(system), std::move(boot), prepared_wsi);

  // initialize current game variable
  std::string game_id = SConfig::GetInstance().GetGameID();
  if (Core::mGameMap.find(game_id) == mGameMap.end())
    mGameBeingPlayed = GameName::UnknownGame;
  else
    mGameBeingPlayed = mGameMap.at(game_id);

  return true;
}

static void ResetRumble()
{
#if defined(__LIBUSB__)
  GCAdapter::ResetRumble();
#endif
  if (!Pad::IsInitialized())
    return;
  for (int i = 0; i < 4; ++i)
    Pad::ResetRumble(i);
}

// Called from GUI thread
void Stop()  // - Hammertime!
{
  if (GetState() == State::Stopping || GetState() == State::Uninitialized)
    return;

#ifdef USE_RETRO_ACHIEVEMENTS
  AchievementManager::GetInstance().CloseGame();
#endif  // USE_RETRO_ACHIEVEMENTS

  s_is_stopping = true;

  CallOnStateChangedCallbacks(State::Stopping);

  // Dump left over jobs
  HostDispatchJobs();

  auto& system = Core::System::GetInstance();

  system.GetFifo().EmulatorState(false);

  INFO_LOG_FMT(CONSOLE, "Stop [Main Thread]\t\t---- Shutting down ----");

  // Stop the CPU
  INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Stop CPU"));
  system.GetCPU().Stop();

  if (system.IsDualCoreMode())
  {
    // Video_EnterLoop() should now exit so that EmuThread()
    // will continue concurrently with the rest of the commands
    // in this function. We no longer rely on Postmessage.
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Wait for Video Loop to exit ..."));

    g_video_backend->Video_ExitLoop();
  }

  s_last_actual_emulation_speed = 1.0;

  // ASSERT(Core::IsCPUThread());
  if (mGameBeingPlayed == GameName::MarioBaseball)
  {
    Core::CPUThreadGuard guard(system);

    s_stat_tracker->dumpGame(guard);
    std::cout << "Emulation stopped. Dumping game." << std::endl;
    s_stat_tracker->init();
  }
}

void DeclareAsCPUThread()
{
  tls_is_cpu_thread = true;
}

void UndeclareAsCPUThread()
{
  tls_is_cpu_thread = false;
}

void DeclareAsGPUThread()
{
  tls_is_gpu_thread = true;
}

void UndeclareAsGPUThread()
{
  tls_is_gpu_thread = false;
}

void DeclareAsHostThread()
{
  tls_is_host_thread = true;
}

void UndeclareAsHostThread()
{
  tls_is_host_thread = false;
}

// For the CPU Thread only.
static void CPUSetInitialExecutionState(bool force_paused = false)
{
  // The CPU starts in stepping state, and will wait until a new state is set before executing.
  // SetState must be called on the host thread, so we defer it for later.
  QueueHostJob([force_paused]() {
    bool paused = SConfig::GetInstance().bBootToPause || force_paused;
    SetState(paused ? State::Paused : State::Running);
    Host_UpdateDisasmDialog();
    Host_UpdateMainFrame();
    Host_Message(HostMessageID::WMUserCreate);
  });
}

// Create the CPU thread, which is a CPU + Video thread in Single Core mode.
static void CpuThread(Core::System& system, const std::optional<std::string>& savestate_path,
                      bool delete_savestate)
{
  DeclareAsCPUThread();

  if (system.IsDualCoreMode())
    Common::SetCurrentThreadName("CPU thread");
  else
    Common::SetCurrentThreadName("CPU-GPU thread");

  // This needs to be delayed until after the video backend is ready.
  DolphinAnalytics::Instance().ReportGameStart();

  // Clear performance data collected from previous threads.
  g_perf_metrics.Reset();

  // The JIT need to be able to intercept faults, both for fastmem and for the BLR optimization.
  const bool exception_handler = EMM::IsExceptionHandlerSupported();
  if (exception_handler)
    EMM::InstallExceptionHandler();

#ifdef USE_MEMORYWATCHER
  s_memory_watcher = std::make_unique<MemoryWatcher>();
#endif

  if (!s_stat_tracker) {
    s_stat_tracker = std::make_unique<StatTracker>();
    s_stat_tracker->init();
    std::cout << "Init stat tracker" << std::endl;
  }

  if (savestate_path)
  {
    ::State::LoadAs(*savestate_path);
    if (delete_savestate)
      File::Delete(*savestate_path);
  }

  s_is_started = true;
  {
#ifndef _WIN32
    std::string gdb_socket = Config::Get(Config::MAIN_GDB_SOCKET);
    if (!gdb_socket.empty())
    {
      GDBStub::InitLocal(gdb_socket.data());
      CPUSetInitialExecutionState(true);
    }
    else
#endif
    {
      int gdb_port = Config::Get(Config::MAIN_GDB_PORT);
      if (gdb_port > 0)
      {
        GDBStub::Init(gdb_port);
        CPUSetInitialExecutionState(true);
      }
      else
      {
        CPUSetInitialExecutionState();
      }
    }
  }

  // Enter CPU run loop. When we leave it - we are done.
  system.GetCPU().Run();

#ifdef USE_MEMORYWATCHER
  s_memory_watcher.reset();
#endif

  s_is_started = false;

  if (exception_handler)
    EMM::UninstallExceptionHandler();

  if (GDBStub::IsActive())
  {
    GDBStub::Deinit();
    INFO_LOG_FMT(GDB_STUB, "Killed by CPU shutdown");
    return;
  }
}

static void FifoPlayerThread(Core::System& system, const std::optional<std::string>& savestate_path,
                             bool delete_savestate)
{
  DeclareAsCPUThread();

  if (system.IsDualCoreMode())
    Common::SetCurrentThreadName("FIFO player thread");
  else
    Common::SetCurrentThreadName("FIFO-GPU thread");

  // Enter CPU run loop. When we leave it - we are done.
  if (auto cpu_core = system.GetFifoPlayer().GetCPUCore())
  {
    system.GetPowerPC().InjectExternalCPUCore(cpu_core.get());
    s_is_started = true;

    CPUSetInitialExecutionState();
    system.GetCPU().Run();

    s_is_started = false;
    system.GetPowerPC().InjectExternalCPUCore(nullptr);
    system.GetFifoPlayer().Close();
  }
  else
  {
    // FIFO log does not contain any frames, cannot continue.
    PanicAlertFmt("FIFO file is invalid, cannot playback.");
    system.GetFifoPlayer().Close();
    return;
  }
}

// Initialize and create emulation thread
// Call browser: Init():s_emu_thread().
// See the BootManager.cpp file description for a complete call schedule.
static void EmuThread(Core::System& system, std::unique_ptr<BootParameters> boot,
                      WindowSystemInfo wsi)
{
  CallOnStateChangedCallbacks(State::Starting);
  Common::ScopeGuard flag_guard{[] {
    s_is_booting.Clear();
    s_is_started = false;
    s_is_stopping = false;
    s_wants_determinism = false;

    CallOnStateChangedCallbacks(State::Uninitialized);

    INFO_LOG_FMT(CONSOLE, "Stop\t\t---- Shutdown complete ----");
  }};

  Common::SetCurrentThreadName("Emuthread - Starting");

  DeclareAsGPUThread();

  // For a time this acts as the CPU thread...
  DeclareAsCPUThread();
  s_frame_step = false;

  // If settings have changed since the previous run, notify callbacks.
  CPUThreadConfigCallback::CheckForConfigChanges();

  // Switch the window used for inputs to the render window. This way, the cursor position
  // is relative to the render window, instead of the main window.
  ASSERT(g_controller_interface.IsInit());
  g_controller_interface.ChangeWindow(wsi.render_window);

  Pad::LoadConfig();
  Pad::LoadGBAConfig();
  Keyboard::LoadConfig();

  BootSessionData boot_session_data = std::move(boot->boot_session_data);
  const std::optional<std::string>& savestate_path = boot_session_data.GetSavestatePath();
  const bool delete_savestate =
      boot_session_data.GetDeleteSavestate() == DeleteSavestateAfterBoot::Yes;

  bool sync_sd_folder = system.IsWii() && Config::Get(Config::MAIN_WII_SD_CARD) &&
                        Config::Get(Config::MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC);
  if (sync_sd_folder)
  {
    sync_sd_folder =
        Common::SyncSDFolderToSDImage([]() { return false; }, Core::WantsDeterminism());
  }

  Common::ScopeGuard sd_folder_sync_guard{[sync_sd_folder] {
    if (sync_sd_folder && Config::Get(Config::MAIN_ALLOW_SD_WRITES))
    {
      const bool sync_ok = Common::SyncSDImageToSDFolder([]() { return false; });
      if (!sync_ok)
      {
        PanicAlertFmtT(
            "Failed to sync SD card with folder. All changes made this session will be "
            "discarded on next boot if you do not manually re-issue a resync in Config > "
            "Wii > SD Card Settings > Convert File to Folder Now!");
      }
    }
  }};

  // Load Wiimotes - only if we are booting in Wii mode
  if (system.IsWii() && !Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED))
  {
    Wiimote::LoadConfig();
  }

  FreeLook::LoadInputConfig();

  system.GetCustomAssetLoader().Init();
  Common::ScopeGuard asset_loader_guard([&system] { system.GetCustomAssetLoader().Shutdown(); });

  system.GetMovie().Init(*boot);
  Common::ScopeGuard movie_guard([&system] { system.GetMovie().Shutdown(); });

  AudioCommon::InitSoundStream(system);
  Common::ScopeGuard audio_guard([&system] { AudioCommon::ShutdownSoundStream(system); });

  HW::Init(system,
           NetPlay::IsNetPlayRunning() ? &(boot_session_data.GetNetplaySettings()->sram) : nullptr);

  Common::ScopeGuard hw_guard{[&system] {
    // We must set up this flag before executing HW::Shutdown()
    s_hardware_initialized = false;
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(false, "Shutting down HW"));
    HW::Shutdown(system);
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(false, "HW shutdown"));

    // The config must be restored only after the whole HW has shut down,
    // not when it is still running.
    BootManager::RestoreConfig();

    PatchEngine::Shutdown();
    HLE::Clear();

    CPUThreadGuard guard(system);
    system.GetPowerPC().GetDebugInterface().Clear(guard);
  }};

  VideoBackendBase::PopulateBackendInfo(wsi);

  if (!g_video_backend->Initialize(wsi))
  {
    PanicAlertFmt("Failed to initialize video backend!");
    return;
  }
  Common::ScopeGuard video_guard{[] {
    // Clear on screen messages that haven't expired
    OSD::ClearMessages();

    g_video_backend->Shutdown();
  }};

  if (cpu_info.HTT)
    Config::SetBaseOrCurrent(Config::MAIN_DSP_THREAD, cpu_info.num_cores > 4);
  else
    Config::SetBaseOrCurrent(Config::MAIN_DSP_THREAD, cpu_info.num_cores > 2);

  if (!system.GetDSP().GetDSPEmulator()->Initialize(system.IsWii(),
                                                    Config::Get(Config::MAIN_DSP_THREAD)))
  {
    PanicAlertFmt("Failed to initialize DSP emulation!");
    return;
  }

  AudioCommon::PostInitSoundStream(system);

  // The hardware is initialized.
  s_hardware_initialized = true;
  s_is_booting.Clear();

  // Set execution state to known values (CPU/FIFO/Audio Paused)
  system.GetCPU().Break();

  // Load GCM/DOL/ELF whatever ... we boot with the interpreter core
  system.GetPowerPC().SetMode(PowerPC::CoreMode::Interpreter);

  // Determine the CPU thread function
  void (*cpuThreadFunc)(Core::System & system, const std::optional<std::string>& savestate_path,
                        bool delete_savestate);
  if (std::holds_alternative<BootParameters::DFF>(boot->parameters))
    cpuThreadFunc = FifoPlayerThread;
  else
    cpuThreadFunc = CpuThread;

  std::optional<DiscIO::Riivolution::SavegameRedirect> savegame_redirect = std::nullopt;
  if (system.IsWii())
    savegame_redirect = DiscIO::Riivolution::ExtractSavegameRedirect(boot->riivolution_patches);

  {
    ASSERT(IsCPUThread());
    CPUThreadGuard guard(system);
    if (!CBoot::BootUp(system, guard, std::move(boot)))
      return;
  }

  // Initialise Wii filesystem contents.
  // This is done here after Boot and not in BootManager to ensure that we operate
  // with the correct title context since save copying requires title directories to exist.
  Common::ScopeGuard wiifs_guard{[&boot_session_data] {
    Core::CleanUpWiiFileSystemContents(boot_session_data);
    boot_session_data.InvokeWiiSyncCleanup();
  }};
  if (system.IsWii())
    Core::InitializeWiiFileSystemContents(savegame_redirect, boot_session_data);
  else
    wiifs_guard.Dismiss();

  // This adds the SyncGPU handler to CoreTiming, so now CoreTiming::Advance might block.
  system.GetFifo().Prepare();

  // Setup our core
  if (Config::Get(Config::MAIN_CPU_CORE) != PowerPC::CPUCore::Interpreter)
  {
    system.GetPowerPC().SetMode(PowerPC::CoreMode::JIT);
  }
  else
  {
    system.GetPowerPC().SetMode(PowerPC::CoreMode::Interpreter);
  }

  UpdateTitle();

  // ENTER THE VIDEO THREAD LOOP
  if (system.IsDualCoreMode())
  {
    // This thread, after creating the EmuWindow, spawns a CPU
    // thread, and then takes over and becomes the video thread
    Common::SetCurrentThreadName("Video thread");
    UndeclareAsCPUThread();
    Common::FPU::LoadDefaultSIMDState();

    // Spawn the CPU thread. The CPU thread will signal the event that boot is complete.
    s_cpu_thread =
        std::thread(cpuThreadFunc, std::ref(system), std::ref(savestate_path), delete_savestate);

    // become the GPU thread
    system.GetFifo().RunGpuLoop();

    // We have now exited the Video Loop
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(false, "Video Loop Ended"));

    // Join with the CPU thread.
    s_cpu_thread.join();
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "CPU thread stopped."));

    // Redeclare this thread as the CPU thread, so that the code running in the scope guards doesn't
    // think we're doing anything unsafe by doing stuff that could race with the CPU thread.
    DeclareAsCPUThread();
  }
  else  // SingleCore mode
  {
    // Become the CPU thread
    cpuThreadFunc(system, savestate_path, delete_savestate);
  }

  INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Stopping GDB ..."));
  GDBStub::Deinit();
  INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "GDB stopped."));
}

// Set or get the running state

void SetState(State state, bool report_state_change)
{
  // State cannot be controlled until the CPU Thread is operational
  if (!IsRunningAndStarted())
    return;

  auto& system = Core::System::GetInstance();
  switch (state)
  {
  case State::Paused:
    // NOTE: GetState() will return State::Paused immediately, even before anything has
    //   stopped (including the CPU).
    system.GetCPU().EnableStepping(true);  // Break
    Wiimote::Pause();
    ResetRumble();
    break;
  case State::Running:
  {
    system.GetCPU().EnableStepping(false);
    Wiimote::Resume();
    break;
  }
  default:
    PanicAlertFmt("Invalid state");
    break;
  }

  // Certain callers only change the state momentarily. Sending a callback for them causes
  // unwanted updates, such as the Pause/Play button flickering between states on frame advance.
  if (report_state_change)
    CallOnStateChangedCallbacks(GetState());
}

State GetState()
{
  if (s_is_stopping)
    return State::Stopping;

  if (s_hardware_initialized)
  {
    auto& system = Core::System::GetInstance();
    if (system.GetCPU().IsStepping())
      return State::Paused;

    return State::Running;
  }

  if (s_is_booting.IsSet())
    return State::Starting;

  return State::Uninitialized;
}

static std::string GenerateScreenshotFolderPath()
{
  const std::string& gameId = SConfig::GetInstance().GetGameID();
  std::string path = File::GetUserPath(D_SCREENSHOTS_IDX) + gameId + DIR_SEP_CHR;

  if (!File::CreateFullPath(path))
  {
    // fallback to old-style screenshots, without folder.
    path = File::GetUserPath(D_SCREENSHOTS_IDX);
  }

  return path;
}

static std::string GenerateScreenshotName()
{
  // append gameId, path only contains the folder here.
  const std::string path_prefix =
      GenerateScreenshotFolderPath() + SConfig::GetInstance().GetGameID();

  const std::time_t cur_time = std::time(nullptr);
  const std::string base_name =
      fmt::format("{}_{:%Y-%m-%d_%H-%M-%S}", path_prefix, Common::LocalTime(cur_time).value_or(std::tm{}));

  // First try a filename without any suffixes, if already exists then append increasing numbers
  std::string name = fmt::format("{}.png", base_name);
  if (File::Exists(name))
  {
    for (u32 i = 1; File::Exists(name = fmt::format("{}_{}.png", base_name, i)); ++i)
      ;
  }

  return name;
}

void SaveScreenShot()
{
  Core::RunAsCPUThread([] { g_frame_dumper->SaveScreenshot(GenerateScreenshotName()); });
}

void SaveScreenShot(std::string_view name)
{
  Core::RunAsCPUThread([&name] {
    g_frame_dumper->SaveScreenshot(fmt::format("{}{}.png", GenerateScreenshotFolderPath(), name));
  });
}

static bool PauseAndLock(Core::System& system, bool do_lock, bool unpause_on_unlock)
{
  // WARNING: PauseAndLock is not fully threadsafe so is only valid on the Host Thread

  if (!IsRunningAndStarted())
    return true;

  bool was_unpaused = true;
  if (do_lock)
  {
    // first pause the CPU
    // This acquires a wrapper mutex and converts the current thread into
    // a temporary replacement CPU Thread.
    was_unpaused = system.GetCPU().PauseAndLock(true);
  }

  // audio has to come after CPU, because CPU thread can wait for audio thread (m_throttle).
  system.GetDSP().GetDSPEmulator()->PauseAndLock(do_lock);

  // video has to come after CPU, because CPU thread can wait for video thread
  // (s_efbAccessRequested).
  system.GetFifo().PauseAndLock(do_lock, false);

  ResetRumble();

  // CPU is unlocked last because CPU::PauseAndLock contains the synchronization
  // mechanism that prevents CPU::Break from racing.
  if (!do_lock)
  {
    // The CPU is responsible for managing the Audio and FIFO state so we use its
    // mechanism to unpause them. If we unpaused the systems above when releasing
    // the locks then they could call CPU::Break which would require detecting it
    // and re-pausing with CPU::EnableStepping.
    was_unpaused = system.GetCPU().PauseAndLock(false, unpause_on_unlock, true);
  }

  return was_unpaused;
}

void RunAsCPUThread(std::function<void()> function)
{
  auto& system = Core::System::GetInstance();
  const bool is_cpu_thread = IsCPUThread();
  bool was_unpaused = false;
  if (!is_cpu_thread)
    was_unpaused = PauseAndLock(system, true, true);

  function();

  if (!is_cpu_thread)
    PauseAndLock(system, false, was_unpaused);
}

void RunOnCPUThread(std::function<void()> function, bool wait_for_completion)
{
  // If the CPU thread is not running, assume there is no active CPU thread we can race against.
  if (!IsRunning() || IsCPUThread())
  {
    function();
    return;
  }

  auto& system = Core::System::GetInstance();

  // Pause the CPU (set it to stepping mode).
  const bool was_running = PauseAndLock(system, true, true);

  // Queue the job function.
  if (wait_for_completion)
  {
    // Trigger the event after executing the function.
    s_cpu_thread_job_finished.Reset();
    system.GetCPU().AddCPUThreadJob([&function]() {
      function();
      s_cpu_thread_job_finished.Set();
    });
  }
  else
  {
    system.GetCPU().AddCPUThreadJob(std::move(function));
  }

  // Release the CPU thread, and let it execute the callback.
  PauseAndLock(system, false, was_running);

  // If we're waiting for completion, block until the event fires.
  if (wait_for_completion)
  {
    // Periodically yield to the UI thread, so we don't deadlock.
    while (!s_cpu_thread_job_finished.WaitFor(std::chrono::milliseconds(10)))
      Host_YieldToUI();
  }
}

// --- Callbacks for backends / engine ---

// Called from Renderer::Swap (GPU thread) when a new (non-duplicate)
// frame is presented to the host screen
void Callback_FramePresented(double actual_emulation_speed)
{
  g_perf_metrics.CountFrame();

  s_last_actual_emulation_speed = actual_emulation_speed;
  s_stop_frame_step.store(true);
}

// Called from VideoInterface::Update (CPU thread) at emulated field boundaries
void Callback_NewField(Core::System& system)
{
  if (s_frame_step)
  {
    // To ensure that s_stop_frame_step is up to date, wait for the GPU thread queue to empty,
    // since it is may contain a swap event (which will call Callback_FramePresented). This hurts
    // the performance a little, but luckily, performance matters less when using frame stepping.
    AsyncRequests::GetInstance()->WaitForEmptyQueue();

    // Only stop the frame stepping if a new frame was displayed
    // (as opposed to the previous frame being displayed for another frame).
    if (s_stop_frame_step.load())
    {
      s_frame_step = false;
      system.GetCPU().Break();
      CallOnStateChangedCallbacks(Core::GetState());
    }
  }

#ifdef USE_RETRO_ACHIEVEMENTS
  AchievementManager::GetInstance().DoFrame();
#endif  // USE_RETRO_ACHIEVEMENTS
}

void UpdateTitle()
{
  // Settings are shown the same for both extended and summary info
  const std::string SSettings = fmt::format(
      "{} {} | {} | {}", Core::System::GetInstance().GetPowerPC().GetCPUName(),
      Core::System::GetInstance().IsDualCoreMode() ? "DC" : "SC", g_video_backend->GetDisplayName(),
      Config::Get(Config::MAIN_DSP_HLE) ? "HLE" : "LLE");

  std::string message = fmt::format("{} | {}", Common::GetScmRevStr(), SSettings);
  if (Config::Get(Config::MAIN_SHOW_ACTIVE_TITLE))
  {
    const std::string& title = SConfig::GetInstance().GetTitleDescription();
    if (!title.empty())
      message += " | " + title;
  }

  Host_UpdateTitle(message);
}

void Shutdown()
{
  // During shutdown DXGI expects us to handle some messages on the UI thread.
  // Therefore we can't immediately block and wait for the emu thread to shut
  // down, so we join the emu thread as late as possible when the UI has already
  // shut down.
  // For more info read "DirectX Graphics Infrastructure (DXGI): Best Practices"
  // on MSDN.
  if (s_emu_thread.joinable())
    s_emu_thread.join();

  // Make sure there's nothing left over in case we're about to exit.
  HostDispatchJobs();
}

int AddOnStateChangedCallback(StateChangedCallbackFunc callback)
{
  for (size_t i = 0; i < s_on_state_changed_callbacks.size(); ++i)
  {
    if (!s_on_state_changed_callbacks[i])
    {
      s_on_state_changed_callbacks[i] = std::move(callback);
      return int(i);
    }
  }
  s_on_state_changed_callbacks.emplace_back(std::move(callback));
  return int(s_on_state_changed_callbacks.size()) - 1;
}

bool RemoveOnStateChangedCallback(int* handle)
{
  if (handle && *handle >= 0 && s_on_state_changed_callbacks.size() > static_cast<size_t>(*handle))
  {
    s_on_state_changed_callbacks[*handle] = StateChangedCallbackFunc();
    *handle = -1;
    return true;
  }
  return false;
}

void CallOnStateChangedCallbacks(Core::State state)
{
  for (const StateChangedCallbackFunc& on_state_changed_callback : s_on_state_changed_callbacks)
  {
    if (on_state_changed_callback)
      on_state_changed_callback(state);
  }
}

void UpdateWantDeterminism(bool initial)
{
  // For now, this value is not itself configurable.  Instead, individual
  // settings that depend on it, such as GPU determinism mode. should have
  // override options for testing,
  auto& system = Core::System::GetInstance();
  bool new_want_determinism = system.GetMovie().IsMovieActive() || NetPlay::IsNetPlayRunning();
  if (new_want_determinism != s_wants_determinism || initial)
  {
    NOTICE_LOG_FMT(COMMON, "Want determinism <- {}", new_want_determinism ? "true" : "false");

    RunAsCPUThread([&] {
      s_wants_determinism = new_want_determinism;
      const auto ios = IOS::HLE::GetIOS();
      if (ios)
        ios->UpdateWantDeterminism(new_want_determinism);

      system.GetFifo().UpdateWantDeterminism(new_want_determinism);

      // We need to clear the cache because some parts of the JIT depend on want_determinism,
      // e.g. use of FMA.
      system.GetJitInterface().ClearCache();
    });
  }
}

void QueueHostJob(std::function<void()> job, bool run_during_stop)
{
  if (!job)
    return;

  bool send_message = false;
  {
    std::lock_guard guard(s_host_jobs_lock);
    send_message = s_host_jobs_queue.empty();
    s_host_jobs_queue.emplace(HostJob{std::move(job), run_during_stop});
  }
  // If the the queue was empty then kick the Host to come and get this job.
  if (send_message)
    Host_Message(HostMessageID::WMUserJobDispatch);
}

void HostDispatchJobs()
{
  // WARNING: This should only run on the Host Thread.
  // NOTE: This function is potentially re-entrant. If a job calls
  //   Core::Stop for instance then we'll enter this a second time.
  std::unique_lock guard(s_host_jobs_lock);
  while (!s_host_jobs_queue.empty())
  {
    HostJob job = std::move(s_host_jobs_queue.front());
    s_host_jobs_queue.pop();

    // NOTE: Memory ordering is important. The booting flag needs to be
    //   checked first because the state transition is:
    //   Core::State::Uninitialized: s_is_booting -> s_hardware_initialized
    //   We need to check variables in the same order as the state
    //   transition, otherwise we race and get transient failures.
    if (!job.run_after_stop && !s_is_booting.IsSet() && !IsRunning())
      continue;

    guard.unlock();
    job.job();
    guard.lock();
  }
}

// NOTE: Host Thread
void DoFrameStep()
{
#ifdef USE_RETRO_ACHIEVEMENTS
  if (AchievementManager::GetInstance().IsHardcoreModeActive())
  {
    OSD::AddMessage("Frame stepping is disabled in RetroAchievements hardcore mode");
    return;
  }
#endif  // USE_RETRO_ACHIEVEMENTS
  if (GetState() == State::Paused)
  {
    // if already paused, frame advance for 1 frame
    s_stop_frame_step = false;
    s_frame_step = true;
    SetState(State::Running, false);
  }
  else if (!s_frame_step)
  {
    // if not paused yet, pause immediately instead
    SetState(State::Paused);
  }
}

void UpdateInputGate(bool require_focus, bool require_full_focus)
{
  // If the user accepts background input, controls should pass even if an on screen interface is on
  const bool focus_passes =
      !require_focus || (Host_RendererHasFocus() && !Host_UIBlocksControllerState());
  // Ignore full focus if we don't require basic focus
  const bool full_focus_passes =
      !require_focus || !require_full_focus || (focus_passes && Host_RendererHasFullFocus());
  ControlReference::SetInputGate(focus_passes && full_focus_passes);
}

CPUThreadGuard::CPUThreadGuard(Core::System& system)
    : m_system(system), m_was_cpu_thread(IsCPUThread())
{
  if (!m_was_cpu_thread)
    m_was_unpaused = PauseAndLock(system, true, true);
}

CPUThreadGuard::~CPUThreadGuard()
{
  if (!m_was_cpu_thread)
    PauseAndLock(m_system, false, m_was_unpaused);
}

void SetGameID(u32 gameID)
{
  if (!s_stat_tracker)
  {
    s_stat_tracker = std::make_unique<StatTracker>();
    s_stat_tracker->init();
  }

  s_stat_tracker->setGameID(gameID);
}

std::optional<TagSet> GetActiveTagSet(bool netplay)
{
  return netplay ? tagset_netplay : tagset_local;
}

void SetTagSet(std::optional<TagSet> tagset, bool netplay)
{
  if (netplay)
    tagset_netplay = tagset;
  else
    tagset_local = tagset;

  if (!s_stat_tracker)
  {
    s_stat_tracker = std::make_unique<StatTracker>();
    s_stat_tracker->init();
  }

  if (tagset.has_value())
  {
    s_stat_tracker->setTagSetId(std::move(tagset.value()), netplay);
  }
  else
  {
    s_stat_tracker->clearTagSetId(netplay);
  }
}

bool isTagSetActive(std::optional<bool> netplay)
{
  if (!GameSupportsTagSets())
    return false;

  if (!netplay.has_value())
    netplay = NetPlay::IsNetPlayRunning();

  bool isActive = false;
  if (netplay.value())
    isActive = tagset_netplay.has_value();
  else
    isActive = tagset_local.has_value();
  return isActive;
}

bool isNetplay()
{
  return NetPlay::IsNetPlayRunning();
}

std::optional<std::vector<std::string>> GetTagSetGeckoString()
{
  if (!GameSupportsTagSets())
    return std::nullopt;

  std::optional<TagSet> tagset_active = GetActiveTagSet(NetPlay::IsNetPlayRunning());

  if (!tagset_active.has_value())
    return std::nullopt;

  return tagset_active.value().gecko_codes_string();
}

bool GameSupportsTagSets()
{
  if (mGameBeingPlayed == GameName::MarioBaseball)
    return true;
  else
    return false;
}

std::optional<std::pair<u32,u32>> getGameFreeMemory()
{
  switch (mGameBeingPlayed) {
  case GameName::MarioBaseball:
    //return std::make_pair(0x802ED200, 0x802EE764);
    return std::make_pair(0x802D5100, 0x802D9500);
  case GameName::ToadstoolTour:
    return std::nullopt;
  case GameName::UnknownGame:
    return std::nullopt;
  default:
    return std::nullopt;
  }
}

int GetNextGolferID()
{
  return nextGolferID;
}

}  // namespace Core
