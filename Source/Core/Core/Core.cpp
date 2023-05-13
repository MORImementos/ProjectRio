// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Core.h"

#include <algorithm>
#include <atomic>
#include <cstring>
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

#include "Core/HW/AddressSpace.h"

#include "Common/CPUDetect.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FPURoundMode.h"
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

#include "Core/API/Events.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
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
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"
#include "Core/System.h"
#include "Core/WiiRoot.h"

//#include "Core/LocalPlayers.h"
#include "Core/LocalPlayersConfig.h"
#include "Core/DefaultGeckoCodes.h"


#ifdef USE_MEMORYWATCHER
#include "Core/MemoryWatcher.h"
#endif

#include "DiscIO/RiivolutionPatcher.h"
#include "Core/MSB_StatTracker.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCAdapter.h"

#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#include "Common/TagSet.h"

#ifdef ANDROID
#include "jni/AndroidCommon/IDCache.h"
#endif

namespace Core
{
static bool s_wants_determinism;

// Declarations and definitions
static Common::Timer s_timer;
static std::atomic<u32> s_drawn_frame;
static std::atomic<u32> s_drawn_video;

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

#ifdef USE_MEMORYWATCHER
static std::unique_ptr<MemoryWatcher> s_memory_watcher;
#endif

void OnFrameBegin()
{
  API::GetEventHub().EmitEvent(API::Events::FrameAdvance{});
}

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

static void EmuThread(std::unique_ptr<BootParameters> boot, WindowSystemInfo wsi);

DefaultGeckoCodes CodeWriter;

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
    if (s_stat_tracker)
    {
      // Figure out if client is hosting via netplay settings. Could use local player as well
      bool is_hosting = NetPlay::GetNetSettings().m_IsHosting;
      std::string opponent_name = "";
      /*
      for (auto player : NetPlay::NetPlayClient::GetPlayers()){
        if (!NetPlay::NetPlayClient::IsLocalPlayer(player.pid)){
          opponent_name = player.name;
          break;
        }
      }*/
      s_stat_tracker->setNetplaySession(true, is_hosting, opponent_name);
    }
  }
  else
  {
    if (s_stat_tracker)
    {
      s_stat_tracker->setNetplaySession(false);
    }
  }
}

// this function is called from PatchEngine.cpp (ApplyFramePatches()) safely
// we can do memory reads/writes without worrying
// anything that needs to read or write to memory should be getting run from here
void RunRioFunctions()
{
  if (s_stat_tracker)
  {
    s_stat_tracker->Run();
  }

  if (PowerPC::HostRead_U32(aGameId) == 0)
  {
    runNetplayGameFunctions = true;
  }

  if (NetPlay::IsNetPlayRunning())
  {
    // send checksum for desync detection
    u64 frame = Movie::GetCurrentFrame();
    if (frame % 60)
    {
      u8 checksumId = (frame / 60) & 0xF;
      NetPlay::NetPlayClient::SendChecksum(checksumId, frame);
    }
    if (runNetplayGameFunctions)
    {
      SetNetplayerUserInfo();
      NetPlay::NetPlayClient::SendGameID(PowerPC::HostRead_U32(aGameId));
      runNetplayGameFunctions = false;
    }

  }

  CodeWriter.RunCodeInject();
  AutoGolfMode();
  TrainingMode();
  DisplayBatterFielder();
  SetAvgPing();
  RunDraftTimer();
}

void OnFrameEnd()
{
#ifdef USE_MEMORYWATCHER
  if (s_memory_watcher)
    s_memory_watcher->Step();
#endif
}

void AutoGolfMode()
{
  if (IsGolfMode())
  {
    u8 BatterPort = PowerPC::HostRead_U8(aBatterPort);      
    u8 FielderPort = PowerPC::HostRead_U8(aFielderPort);
    bool isField = PowerPC::HostRead_U8(aIsField) == 1;

    if (BatterPort == 0)
      return;  // means game hasn't started yet

    // makes the player who paused the golfer
    if (PowerPC::HostRead_U8(aWhoPaused) == 2)
      isField = true;

    // add minigame functionality
    int minigameId = PowerPC::HostRead_U8(aMinigameID);
    if (minigameId == 3 || minigameId == 1)
    {
      BatterPort = PowerPC::HostRead_U8(aBarrelBatterPort) + 1;
      isField = false;
    }
    else if (minigameId == 2)
    {
      FielderPort = PowerPC::HostRead_U8(aWallBallPort) + 1;
      isField = true;
    }

    NetPlay::NetPlayClient::AutoGolfMode(isField, BatterPort, FielderPort);
  }
}

bool IsGolfMode()
{
  bool out = false;
  if (NetPlay::IsNetPlayRunning())
    out = NetPlay::GetNetSettings().m_HostInputAuthority;

  return out;
}

// TODO: add stats for the following: base runner coordinates; ball coords frame before being caught, character coords after diving/jumping/wall jumping
void TrainingMode()
{
  // if training mode config is on and not ranked netplay
  // using this feature on ranked can be considered an unfair advantage
  if (!g_ActiveConfig.bTrainingModeOverlay || isTagSetActive())
    return;

  //bool isPitchThrown = PowerPC::HostRead_U8(0x80895D6C) == 1 ? true : false;
  bool isField = PowerPC::HostRead_U8(aIsField) == 1 ? true : false;
  bool isInGame = PowerPC::HostRead_U8(aIsInGame) == 1 ? true : false;
  bool ContactMade = PowerPC::HostRead_U8(aContactMade) == 1 ? true : false;

  // Batting Training Mode stats
  if (ContactMade && !previousContactMade)
  {
    u8 BatterPort = PowerPC::HostRead_U8(aBatterPort);
    if (BatterPort > 0)
      BatterPort--;
    u32 stickDirectionAddr = 0x8089392D + (0x10 * BatterPort);

    u16 contactFrame = PowerPC::HostRead_U16(aContactFrame);
    u8 typeOfContact_Value = PowerPC::HostRead_U8(aTypeOfContact);
    std::string typeOfContact;
    u8 inputDirection_Value = PowerPC::HostRead_U8(stickDirectionAddr) & 0xF;
    std::string inputDirection;
    int chargeUp = static_cast<int>(roundf(u32ToFloat(PowerPC::HostRead_U32(aChargeUp)) * 100)); 
    int chargeDown = static_cast<int>(roundf(u32ToFloat(PowerPC::HostRead_U32(aChargeDown)) * 100));

    float angle = roundf((float)PowerPC::HostRead_U16(aBallAngle) * 36000 / 4096) / 100; // 0x400 == 90°, 0x800 == 180°, 0x1000 == 360°
    float xVelocity = roundf(u32ToFloat(PowerPC::HostRead_U32(aBallVelocity_X)) * 6000) / 100;  // * 60 cause default units are meters per frame
    float yVelocity = roundf(u32ToFloat(PowerPC::HostRead_U32(aBallVelocity_Y)) * 6000) / 100;
    float zVelocity = roundf(u32ToFloat(PowerPC::HostRead_U32(aBallVelocity_Z)) * 6000) / 100;
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

    OSD::AddTypedMessage(OSD::MessageType::TrainingModeBatting, fmt::format(
      "Batting Data:                    \n"
      "Contact Frame:  {}\n"
      "Type of Contact:  {}\n"
      "Input Direction:  {}\n"
      "Charge Percent:  {}%\n"
      "Ball Angle:  {}°\n\n"
      "Exit Velocities:  \n"
      "X :  {} m/s  -->  {} mph\n"
      "Y:  {} m/s  -->  {} mph\n"
      "Z :  {} m/s  -->  {} mph\n"
      "Net:  {} m/s  -->  {} mph",
      contactFrame,
      typeOfContact,
      inputDirection,
      totalCharge,
      angle,
      xVelocity, ms_to_mph(xVelocity),
      yVelocity, ms_to_mph(yVelocity),
      zVelocity, ms_to_mph(zVelocity),
      netVelocity, ms_to_mph(netVelocity)),
      8000); // message time & color
    }


  // Coordinate data
  if (isInGame)
  {
    float BallPos_X = roundf(u32ToFloat(PowerPC::HostRead_U32(aBallPosition_X)) * 100) / 100;
    float BallPos_Y = roundf(u32ToFloat(PowerPC::HostRead_U32(aBallPosition_Y)) * 100) / 100;
    float BallPos_Z = roundf(u32ToFloat(PowerPC::HostRead_U32(aBallPosition_Z)) * 100) / 100;
    float BallVel_X = isField ? roundf(u32ToFloat(PowerPC::HostRead_U32(aBallVelocity_X)) * 6000) / 100 :
                                roundf(u32ToFloat(PowerPC::HostRead_U32(aPitchedBallVelocity_X)) * 6000) / 100;
    float BallVel_Y = isField ? RoundZ(u32ToFloat(PowerPC::HostRead_U32(aBallVelocity_Y)) * 6000) / 100 :// floor small decimal to prevent weirdness
                                RoundZ(u32ToFloat(PowerPC::HostRead_U32(aPitchedBallVelocity_Y)) * 6000) / 100;
    float BallVel_Z = isField ? roundf(u32ToFloat(PowerPC::HostRead_U32(aBallVelocity_Z)) * 6000) / 100 :
                                roundf(u32ToFloat(PowerPC::HostRead_U32(aPitchedBallVelocity_Z)) * 6000) / 100;
    float BallVel_Net = roundf(vectorMagnitude(BallVel_X, BallVel_Y, BallVel_Z) * 100) / 100;

    int baseOffset= 0x268 * PowerPC::HostRead_U8(0x80892801);  // used to get offsed for baseFielderAddr
    u32 baseFielderAddr = 0x8088F368 + baseOffset;        // 0x0 == x; 0x8 == y; 0xc == z

    float FielderPos_X = roundf(u32ToFloat(PowerPC::HostRead_U32(baseFielderAddr)) * 100) / 100;
    float FielderPos_Y = roundf(u32ToFloat(PowerPC::HostRead_U32(baseFielderAddr + 0xc)) * 100) / 100;
    float FielderPos_Z = roundf(u32ToFloat(PowerPC::HostRead_U32(baseFielderAddr + 0x8)) * 100) / 100;
    float FielderVel_X = roundf(u32ToFloat(PowerPC::HostRead_U32(baseFielderAddr + 0x30)) * 6000) / 100;
    //float FielderVel_Y = roundf(u32ToFloat(PowerPC::HostRead_U32(baseFielderAddr + 0x15C)) * 6000) / 100; // this addr is wrong
    float FielderVel_Z = roundf(u32ToFloat(PowerPC::HostRead_U32(baseFielderAddr + 0x34)) * 6000) / 100;
    float FielderVel_Net = roundf(vectorMagnitude(FielderVel_X, 0 /*FielderVel_Y*/, FielderVel_Z) * 100) / 100;

    OSD::AddTypedMessage(OSD::MessageType::TrainingModeBallCoordinates, fmt::format(
      "Ball Coordinates:                \n"
      "X:  {}\n"
      "Y:  {}\n"
      "Z:  {}\n\n"
      "Ball Velocity:  \n"
      "X:  {} m/s  -->  {} mph\n"
      "Y:  {} m/s  -->  {} mph\n"
      "Z:  {} m/s  -->  {} mph\n"
      "Net:  {} m/s  -->  {} mph\n",
      BallPos_X,
      BallPos_Y,
      BallPos_Z,
      BallVel_X, ms_to_mph(BallVel_X),
      BallVel_Y, ms_to_mph(BallVel_Y),
      BallVel_Z, ms_to_mph(BallVel_Z),
      BallVel_Net, ms_to_mph(BallVel_Net)
    ), 200, OSD::Color::CYAN);  // short time cause we don't want this info to linger

    OSD::AddTypedMessage(OSD::MessageType::TrainingModeFielderCoordinates, fmt::format(
      "Fielder Coordinates:             \n"
      "X:  {}\n"
      "Y:  {}\n"
      "Z:  {}\n\n"
      "Fielder Velocity: \n"
      "X:  {} m/s  -->  {} mph\n"
      //"Y:  {} m/s  -->  {} mph\n"
      "Z:  {} m/s  -->  {} mph\n"
      "Net:  {} m/s  -->  {} mph",
      FielderPos_X,
      FielderPos_Y,
      FielderPos_Z,
      FielderVel_X, ms_to_mph(FielderVel_X),
      //FielderVel_Y, ms_to_mph(FielderVel_Y),
      FielderVel_Z, ms_to_mph(FielderVel_Z),
      FielderVel_Net, ms_to_mph(FielderVel_Net)
    ), 200, OSD::Color::CYAN);
  }

  previousContactMade = ContactMade;
}

void DisplayBatterFielder()
{
  if (!g_ActiveConfig.bShowBatterFielder)
    return;

  u8 BatterPort = PowerPC::HostRead_U8(aBatterPort);
  u8 FielderPort = PowerPC::HostRead_U8(aFielderPort); 
  if (BatterPort == 0 || FielderPort == 0) // game hasn't started yet; do not continue func
    return;

  // Run using NetPlay Nicknames
  if (NetPlay::IsNetPlayRunning())
    NetPlay::NetPlayClient::DisplayBatterFielder(BatterPort, FielderPort);

  // Run using Local Players
  else
  {    
    std::string P1 = LocalPlayers::m_local_player_1.GetUsername();
    std::string P2 = LocalPlayers::m_local_player_2.GetUsername();
    std::string P3 = LocalPlayers::m_local_player_3.GetUsername();
    std::string P4 = LocalPlayers::m_local_player_4.GetUsername();
    std::vector<std::string> LocalPlayerList{P1, P2, P3, P4};
    std::array<u32, 4> portColor = {
        {OSD::Color::RED, OSD::Color::BLUE, OSD::Color::YELLOW, OSD::Color::GREEN}};

    // subtract 1 from each port so they can be used as indeces in the arrays
    if (BatterPort < 5)
      BatterPort--;
    if (FielderPort < 5)
      FielderPort--;

    if (BatterPort < 4)
    {
      if (LocalPlayerList[BatterPort] != "")  // check for valid user & port
      {
        OSD::AddTypedMessage(OSD::MessageType::CurrentBatter,
                             fmt::format("Batter: {}", LocalPlayerList[BatterPort]),
                             OSD::Duration::SHORT, portColor[BatterPort]);
      }
    }
    if (FielderPort < 4)
    {
      if (LocalPlayerList[FielderPort] != "")  // check for valid user & port
      {
        OSD::AddTypedMessage(OSD::MessageType::CurrentFielder,
                             fmt::format("Fielder: {}", LocalPlayerList[FielderPort]),
                             OSD::Duration::SHORT, portColor[FielderPort]);
      }
    }
  }
}

void RunDraftTimer()
{
  // if they have the config off or if it's not 1st frame of second
  if (Movie::GetCurrentFrame() % 60 != 0)
    return;

  u8 scene = PowerPC::HostRead_U8(aSceneId);

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

// rounds to 2 decimal places
float u32ToFloat(u32 value)
{
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

bool isNight()
{
  if (!NetPlay::IsNetPlayRunning())
    return false;
  return NetPlay::NetPlayClient::isNight();
}

bool isDisableReplays()
{
  if (!NetPlay::IsNetPlayRunning())
    return false;
  return NetPlay::NetPlayClient::isDisableReplays();
}

void SetAvgPing()
{
  if (!NetPlay::IsNetPlayRunning())
    return;

  // checks if GameID is set and that the end game flag hasn't been hit yet
  bool inGame = PowerPC::HostRead_U32(aGameId) != 0 /*&& PowerPC::HostRead_U8(aEndOfGameFlag) == 0*/ ?
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
  if (!std::all_of(message.begin(), message.end(), IsPrintableCharacter))
    return;

  Host_UpdateTitle(message);
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

bool WantsDeterminism()
{
  return s_wants_determinism;
}

// This is called from the GUI thread. See the booting call schedule in
// BootManager.cpp
bool Init(std::unique_ptr<BootParameters> boot, const WindowSystemInfo& wsi)
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

  INFO_LOG_FMT(BOOT, "Starting core = {} mode", SConfig::GetInstance().bWii ? "Wii" : "GameCube");
  INFO_LOG_FMT(BOOT, "CPU Thread separate = {}",
               Core::System::GetInstance().IsDualCoreMode() ? "Yes" : "No");

  Host_UpdateMainFrame();  // Disable any menus or buttons at boot

  // Manually reactivate the video backend in case a GameINI overrides the video backend setting.
  VideoBackendBase::PopulateBackendInfo();

  // Issue any API calls which must occur on the main thread for the graphics backend.
  WindowSystemInfo prepared_wsi(wsi);
  g_video_backend->PrepareWindow(prepared_wsi);

  // Start the emu thread
  s_is_booting.Set();
  s_emu_thread = std::thread(EmuThread, std::move(boot), prepared_wsi);

  std::optional<std::vector<ClientCode>> client_codes =
      GetActiveTagSet(NetPlay::IsNetPlayRunning()).has_value() ?
      GetActiveTagSet(NetPlay::IsNetPlayRunning()).value().client_codes_vector() :
      std::nullopt;

  CodeWriter.Init(client_codes, isTagSetActive(), isNight(), isDisableReplays());

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

  s_is_stopping = true;

  s_timer.Stop();

  CallOnStateChangedCallbacks(State::Stopping);

  // Dump left over jobs
  HostDispatchJobs();

  Fifo::EmulatorState(false);

  INFO_LOG_FMT(CONSOLE, "Stop [Main Thread]\t\t---- Shutting down ----");

  // Stop the CPU
  INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Stop CPU"));
  CPU::Stop();

  if (Core::System::GetInstance().IsDualCoreMode())
  {
    // Video_EnterLoop() should now exit so that EmuThread()
    // will continue concurrently with the rest of the commands
    // in this function. We no longer rely on Postmessage.
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Wait for Video Loop to exit ..."));

    g_video_backend->Video_ExitLoop();
  }

  s_last_actual_emulation_speed = 1.0;

  if (s_stat_tracker) {
    s_stat_tracker->dumpGame();
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
static void CpuThread(const std::optional<std::string>& savestate_path, bool delete_savestate)
{
  DeclareAsCPUThread();

  if (Core::System::GetInstance().IsDualCoreMode())
    Common::SetCurrentThreadName("CPU thread");
  else
    Common::SetCurrentThreadName("CPU-GPU thread");

  // This needs to be delayed until after the video backend is ready.
  DolphinAnalytics::Instance().ReportGameStart();

#ifdef ANDROID
  // For some reason, calling the JNI function AttachCurrentThread from the CPU thread after a
  // certain point causes a crash if fastmem is enabled. Let's call it early to avoid that problem.
  static_cast<void>(IDCache::GetEnvForThread());
#endif

  const bool fastmem_enabled = Config::Get(Config::MAIN_FASTMEM);
  if (fastmem_enabled)
    EMM::InstallExceptionHandler();  // Let's run under memory watch

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
  CPU::Run();

#ifdef USE_MEMORYWATCHER
  s_memory_watcher.reset();
#endif

  s_is_started = false;

  if (fastmem_enabled)
    EMM::UninstallExceptionHandler();

  if (GDBStub::IsActive())
  {
    GDBStub::Deinit();
    INFO_LOG_FMT(GDB_STUB, "Killed by CPU shutdown");
    return;
  }
}

static void FifoPlayerThread(const std::optional<std::string>& savestate_path,
                             bool delete_savestate)
{
  DeclareAsCPUThread();

  if (Core::System::GetInstance().IsDualCoreMode())
    Common::SetCurrentThreadName("FIFO player thread");
  else
    Common::SetCurrentThreadName("FIFO-GPU thread");

  // Enter CPU run loop. When we leave it - we are done.
  if (auto cpu_core = FifoPlayer::GetInstance().GetCPUCore())
  {
    PowerPC::InjectExternalCPUCore(cpu_core.get());
    s_is_started = true;

    CPUSetInitialExecutionState();
    CPU::Run();

    s_is_started = false;
    PowerPC::InjectExternalCPUCore(nullptr);
    FifoPlayer::GetInstance().Close();
  }
  else
  {
    // FIFO log does not contain any frames, cannot continue.
    PanicAlertFmt("FIFO file is invalid, cannot playback.");
    FifoPlayer::GetInstance().Close();
    return;
  }
}

// Initialize and create emulation thread
// Call browser: Init():s_emu_thread().
// See the BootManager.cpp file description for a complete call schedule.
static void EmuThread(std::unique_ptr<BootParameters> boot, WindowSystemInfo wsi)
{
  const Core::System& system = Core::System::GetInstance();
  const SConfig& core_parameter = SConfig::GetInstance();
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

  // The frontend will likely have initialized the controller interface, as it needs
  // it to provide the configuration dialogs. In this case, instead of re-initializing
  // entirely, we switch the window used for inputs to the render window. This way, the
  // cursor position is relative to the render window, instead of the main window.
  bool init_controllers = false;
  if (!g_controller_interface.IsInit())
  {
    g_controller_interface.Initialize(wsi);
    Pad::Initialize();
    Pad::InitializeGBA();
    Keyboard::Initialize();
    init_controllers = true;
  }
  else
  {
    g_controller_interface.ChangeWindow(wsi.render_window);
    Pad::LoadConfig();
    Pad::LoadGBAConfig();
    Keyboard::LoadConfig();
  }

  BootSessionData boot_session_data = std::move(boot->boot_session_data);
  const std::optional<std::string>& savestate_path = boot_session_data.GetSavestatePath();
  const bool delete_savestate =
      boot_session_data.GetDeleteSavestate() == DeleteSavestateAfterBoot::Yes;

  // Load and Init Wiimotes - only if we are booting in Wii mode
  bool init_wiimotes = false;
  if (core_parameter.bWii && !Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED))
  {
    if (init_controllers)
    {
      Wiimote::Initialize(savestate_path ? Wiimote::InitializeMode::DO_WAIT_FOR_WIIMOTES :
                                           Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
      init_wiimotes = true;
    }
    else
    {
      Wiimote::LoadConfig();
    }

    if (NetPlay::IsNetPlayRunning())
      NetPlay::SetupWiimotes();
  }

  if (init_controllers)
  {
    FreeLook::Initialize();
  }
  else
  {
    FreeLook::LoadInputConfig();
  }

  Common::ScopeGuard controller_guard{[init_controllers, init_wiimotes] {
    if (!init_controllers)
      return;

    if (init_wiimotes)
    {
      Wiimote::ResetAllWiimotes();
      Wiimote::Shutdown();
    }

    FreeLook::Shutdown();

    ResetRumble();

    Keyboard::Shutdown();
    Pad::Shutdown();
    Pad::ShutdownGBA();
    g_controller_interface.Shutdown();
  }};

  Movie::Init(*boot);
  Common::ScopeGuard movie_guard{&Movie::Shutdown};

  AudioCommon::InitSoundStream();
  Common::ScopeGuard audio_guard{&AudioCommon::ShutdownSoundStream};

  HW::Init();

  Common::ScopeGuard hw_guard{[] {
    // We must set up this flag before executing HW::Shutdown()
    s_hardware_initialized = false;
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(false, "Shutting down HW"));
    HW::Shutdown();
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(false, "HW shutdown"));

    // Clear on screen messages that haven't expired
    OSD::ClearMessages();

    // The config must be restored only after the whole HW has shut down,
    // not when it is still running.
    BootManager::RestoreConfig();

    PatchEngine::Shutdown();
    HLE::Clear();
    PowerPC::debug_interface.Clear();
  }};

  VideoBackendBase::PopulateBackendInfo();

  if (!g_video_backend->Initialize(wsi))
  {
    PanicAlertFmt("Failed to initialize video backend!");
    return;
  }
  Common::ScopeGuard video_guard{[] { g_video_backend->Shutdown(); }};

  // Render a single frame without anything on it to clear the screen.
  // This avoids the game list being displayed while the core is finishing initializing.
  g_renderer->BeginUIFrame();
  g_renderer->EndUIFrame();

  if (cpu_info.HTT)
    Config::SetBaseOrCurrent(Config::MAIN_DSP_THREAD, cpu_info.num_cores > 4);
  else
    Config::SetBaseOrCurrent(Config::MAIN_DSP_THREAD, cpu_info.num_cores > 2);

  if (!DSP::GetDSPEmulator()->Initialize(core_parameter.bWii, Config::Get(Config::MAIN_DSP_THREAD)))
  {
    PanicAlertFmt("Failed to initialize DSP emulation!");
    return;
  }

  // Inputs loading may have generated custom dynamic textures
  // it's now ok to initialize any custom textures
  HiresTexture::Update();

  AudioCommon::PostInitSoundStream();

  // The hardware is initialized.
  s_hardware_initialized = true;
  s_is_booting.Clear();

  // Set execution state to known values (CPU/FIFO/Audio Paused)
  CPU::Break();

  // Load GCM/DOL/ELF whatever ... we boot with the interpreter core
  PowerPC::SetMode(PowerPC::CoreMode::Interpreter);

  // Determine the CPU thread function
  void (*cpuThreadFunc)(const std::optional<std::string>& savestate_path, bool delete_savestate);
  if (std::holds_alternative<BootParameters::DFF>(boot->parameters))
    cpuThreadFunc = FifoPlayerThread;
  else
    cpuThreadFunc = CpuThread;

  std::optional<DiscIO::Riivolution::SavegameRedirect> savegame_redirect = std::nullopt;
  if (SConfig::GetInstance().bWii)
    savegame_redirect = DiscIO::Riivolution::ExtractSavegameRedirect(boot->riivolution_patches);

  if (!CBoot::BootUp(std::move(boot)))
    return;

  // Initialise Wii filesystem contents.
  // This is done here after Boot and not in BootManager to ensure that we operate
  // with the correct title context since save copying requires title directories to exist.
  Common::ScopeGuard wiifs_guard{[&boot_session_data] {
    Core::CleanUpWiiFileSystemContents(boot_session_data);
    boot_session_data.InvokeWiiSyncCleanup();
  }};
  if (SConfig::GetInstance().bWii)
    Core::InitializeWiiFileSystemContents(savegame_redirect, boot_session_data);
  else
    wiifs_guard.Dismiss();

  // This adds the SyncGPU handler to CoreTiming, so now CoreTiming::Advance might block.
  Fifo::Prepare();

  // Setup our core
  if (Config::Get(Config::MAIN_CPU_CORE) != PowerPC::CPUCore::Interpreter)
  {
    PowerPC::SetMode(PowerPC::CoreMode::JIT);
  }
  else
  {
    PowerPC::SetMode(PowerPC::CoreMode::Interpreter);
  }

  // ENTER THE VIDEO THREAD LOOP
  if (system.IsDualCoreMode())
  {
    // This thread, after creating the EmuWindow, spawns a CPU
    // thread, and then takes over and becomes the video thread
    Common::SetCurrentThreadName("Video thread");
    UndeclareAsCPUThread();
    FPURoundMode::LoadDefaultSIMDState();

    // Spawn the CPU thread. The CPU thread will signal the event that boot is complete.
    s_cpu_thread = std::thread(cpuThreadFunc, savestate_path, delete_savestate);

    // become the GPU thread
    Fifo::RunGpuLoop();

    // We have now exited the Video Loop
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(false, "Video Loop Ended"));

    // Join with the CPU thread.
    s_cpu_thread.join();
    INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "CPU thread stopped."));
  }
  else  // SingleCore mode
  {
    // Become the CPU thread
    cpuThreadFunc(savestate_path, delete_savestate);
  }

  INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "Stopping GDB ..."));
  GDBStub::Deinit();
  INFO_LOG_FMT(CONSOLE, "{}", StopMessage(true, "GDB stopped."));
}

// Set or get the running state

void SetState(State state)
{
  // State cannot be controlled until the CPU Thread is operational
  if (!IsRunningAndStarted())
    return;

  switch (state)
  {
  case State::Paused:
    // NOTE: GetState() will return State::Paused immediately, even before anything has
    //   stopped (including the CPU).
    CPU::EnableStepping(true);  // Break
    Wiimote::Pause();
    ResetRumble();
    s_timer.Update();
    break;
  case State::Running:
    CPU::EnableStepping(false);
    Wiimote::Resume();
    if (!s_timer.IsRunning())
    {
      s_timer.Start();
    }
    else
    {
      // Add time difference from the last pause
      s_timer.AddTimeDifference();
    }
    break;
  default:
    PanicAlertFmt("Invalid state");
    break;
  }

  CallOnStateChangedCallbacks(GetState());
}

State GetState()
{
  if (s_is_stopping)
    return State::Stopping;

  if (s_hardware_initialized)
  {
    if (CPU::IsStepping() || s_frame_step)
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
      fmt::format("{}_{:%Y-%m-%d_%H-%M-%S}", path_prefix, fmt::localtime(cur_time));

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
  Core::RunAsCPUThread([] { g_renderer->SaveScreenshot(GenerateScreenshotName()); });
}

void SaveScreenShot(std::string_view name)
{
  Core::RunAsCPUThread([&name] {
    g_renderer->SaveScreenshot(fmt::format("{}{}.png", GenerateScreenshotFolderPath(), name));
  });
}

static bool PauseAndLock(bool do_lock, bool unpause_on_unlock)
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
    was_unpaused = CPU::PauseAndLock(true);
  }

  ExpansionInterface::PauseAndLock(do_lock, false);

  // audio has to come after CPU, because CPU thread can wait for audio thread (m_throttle).
  DSP::GetDSPEmulator()->PauseAndLock(do_lock, false);

  // video has to come after CPU, because CPU thread can wait for video thread
  // (s_efbAccessRequested).
  Fifo::PauseAndLock(do_lock, false);

  ResetRumble();

  // CPU is unlocked last because CPU::PauseAndLock contains the synchronization
  // mechanism that prevents CPU::Break from racing.
  if (!do_lock)
  {
    // The CPU is responsible for managing the Audio and FIFO state so we use its
    // mechanism to unpause them. If we unpaused the systems above when releasing
    // the locks then they could call CPU::Break which would require detecting it
    // and re-pausing with CPU::EnableStepping.
    was_unpaused = CPU::PauseAndLock(false, unpause_on_unlock, true);
  }

  return was_unpaused;
}

void RunAsCPUThread(std::function<void()> function)
{
  const bool is_cpu_thread = IsCPUThread();
  bool was_unpaused = false;
  if (!is_cpu_thread)
    was_unpaused = PauseAndLock(true, true);

  function();

  if (!is_cpu_thread)
    PauseAndLock(false, was_unpaused);
}

void RunOnCPUThread(std::function<void()> function, bool wait_for_completion)
{
  // If the CPU thread is not running, assume there is no active CPU thread we can race against.
  if (!IsRunning() || IsCPUThread())
  {
    function();
    return;
  }

  // Pause the CPU (set it to stepping mode).
  const bool was_running = PauseAndLock(true, true);

  // Queue the job function.
  if (wait_for_completion)
  {
    // Trigger the event after executing the function.
    s_cpu_thread_job_finished.Reset();
    CPU::AddCPUThreadJob([&function]() {
      function();
      s_cpu_thread_job_finished.Set();
    });
  }
  else
  {
    CPU::AddCPUThreadJob(std::move(function));
  }

  // Release the CPU thread, and let it execute the callback.
  PauseAndLock(false, was_running);

  // If we're waiting for completion, block until the event fires.
  if (wait_for_completion)
  {
    // Periodically yield to the UI thread, so we don't deadlock.
    while (!s_cpu_thread_job_finished.WaitFor(std::chrono::milliseconds(10)))
      Host_YieldToUI();
  }
}

// Display FPS info
// This should only be called from VI
void VideoThrottle()
{
  // Update info per second
  u32 ElapseTime = (u32)s_timer.GetTimeElapsed();
  if ((ElapseTime >= 1000 && s_drawn_video.load() > 0) || s_frame_step)
  {
    s_timer.Start();

    UpdateTitle(ElapseTime);

    s_drawn_frame.store(0);
    s_drawn_video.store(0);
  }

  s_drawn_video++;
}

// --- Callbacks for backends / engine ---

// Called from Renderer::Swap (GPU thread) when a new (non-duplicate)
// frame is presented to the host screen
void Callback_FramePresented(double actual_emulation_speed)
{
  s_last_actual_emulation_speed = actual_emulation_speed;

  s_drawn_frame++;
  s_stop_frame_step.store(true);
}

// Called from VideoInterface::Update (CPU thread) at emulated field boundaries
void Callback_NewField()
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
      CPU::Break();
      CallOnStateChangedCallbacks(Core::GetState());
    }
  }
}

void UpdateTitle(u32 ElapseTime)
{
  if (ElapseTime == 0)
    ElapseTime = 1;

  float FPS = (float)(s_drawn_frame.load() * 1000.0 / ElapseTime);
  float VPS = (float)(s_drawn_video.load() * 1000.0 / ElapseTime);
  float Speed = (float)(s_drawn_video.load() * (100 * 1000.0) /
                        (VideoInterface::GetTargetRefreshRate() * ElapseTime));

  // Settings are shown the same for both extended and summary info
  const std::string SSettings = fmt::format(
      "{} {} | {} | {}", PowerPC::GetCPUName(),
      Core::System::GetInstance().IsDualCoreMode() ? "DC" : "SC", g_video_backend->GetDisplayName(),
      Config::Get(Config::MAIN_DSP_HLE) ? "HLE" : "LLE");

  std::string SFPS;
  if (Movie::IsPlayingInput())
  {
    SFPS = fmt::format("Input: {}/{} - VI: {}/{} - FPS: {:.0f} - VPS: {:.0f} - {:.0f}%",
                       Movie::GetCurrentInputCount(), Movie::GetTotalInputCount(),
                       Movie::GetCurrentFrame(), Movie::GetTotalFrames(), FPS, VPS, Speed);
  }
  else if (Movie::IsRecordingInput())
  {
    SFPS = fmt::format("Input: {} - VI: {} - FPS: {:.0f} - VPS: {:.0f} - {:.0f}%",
                       Movie::GetCurrentInputCount(), Movie::GetCurrentFrame(), FPS, VPS, Speed);
  }
  else
  {
    SFPS = fmt::format("FPS: {:.0f} - VPS: {:.0f} - {:.0f}%", FPS, VPS, Speed);
    if (Config::Get(Config::MAIN_EXTENDED_FPS_INFO))
    {
      // Use extended or summary information. The summary information does not print the ticks data,
      // that's more of a debugging interest, it can always be optional of course if someone is
      // interested.
      static u64 ticks = 0;
      static u64 idleTicks = 0;
      u64 newTicks = CoreTiming::GetTicks();
      u64 newIdleTicks = CoreTiming::GetIdleTicks();

      u64 diff = (newTicks - ticks) / 1000000;
      u64 idleDiff = (newIdleTicks - idleTicks) / 1000000;

      ticks = newTicks;
      idleTicks = newIdleTicks;

      float TicksPercentage =
          (float)diff / (float)(SystemTimers::GetTicksPerSecond() / 1000000) * 100;

      SFPS += fmt::format(" | CPU: ~{} MHz [Real: {} + IdleSkip: {}] / {} MHz (~{:3.0f}%)", diff,
                          diff - idleDiff, idleDiff, SystemTimers::GetTicksPerSecond() / 1000000,
                          TicksPercentage);
    }
  }

  std::string message = fmt::format("{} | {} | {}", Common::GetScmRevStr(), SSettings, SFPS);
  if (Config::Get(Config::MAIN_SHOW_ACTIVE_TITLE))
  {
    const std::string& title = SConfig::GetInstance().GetTitleDescription();
    if (!title.empty())
      message += " | " + title;
  }

  // Update the audio timestretcher with the current speed
  if (g_sound_stream)
  {
    Mixer* pMixer = g_sound_stream->GetMixer();
    pMixer->UpdateSpeed((float)Speed / 100);
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
  bool new_want_determinism = Movie::IsMovieActive() || NetPlay::IsNetPlayRunning();
  if (new_want_determinism != s_wants_determinism || initial)
  {
    NOTICE_LOG_FMT(COMMON, "Want determinism <- {}", new_want_determinism ? "true" : "false");

    RunAsCPUThread([&] {
      s_wants_determinism = new_want_determinism;
      const auto ios = IOS::HLE::GetIOS();
      if (ios)
        ios->UpdateWantDeterminism(new_want_determinism);
      Fifo::UpdateWantDeterminism(new_want_determinism);
      // We need to clear the cache because some parts of the JIT depend on want_determinism,
      // e.g. use of FMA.
      JitInterface::ClearCache();
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
  if (GetState() == State::Paused)
  {
    // if already paused, frame advance for 1 frame
    s_stop_frame_step = false;
    s_frame_step = true;
    SetState(State::Running);
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
  if (!netplay.has_value())
    netplay = NetPlay::IsNetPlayRunning();

  bool isActive = false;
  if (netplay.value())
    isActive = tagset_netplay.has_value();
  else
    isActive = tagset_local.has_value();
  return isActive;
}

std::optional<std::vector<std::string>> GetTagSetGeckoString()
{
  std::optional<TagSet> tagset_active = GetActiveTagSet(NetPlay::IsNetPlayRunning());

  if (!tagset_active.has_value())
    return std::nullopt;

  return tagset_active.value().gecko_codes_string();
}

}  // namespace Core
