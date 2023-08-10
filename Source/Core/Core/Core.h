// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Core

// The external interface to the emulator core. Plus some extras.
// This is another part of the emu that needs cleaning - Core.cpp really has
// too much random junk inside.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <optional>
#include <map>

#include "Common/CommonTypes.h"

#include "Core/HW/Memmap.h"

struct BootParameters;
struct WindowSystemInfo;

namespace Tag {
  class TagSet;
};

//enum class GameName
//{
//  MarioBaseball,
//  MarioGolf,
//  Unknown
//};

enum class GameName : u8
{
  UnknownGame = 0,
  MarioBaseball = 1,
  ToadstoolTour = 2,
};

//GameName GetGameIDEnum(const std::string& gameName);

namespace Core
{
class System;

bool GetIsThrottlerTempDisabled();
void SetIsThrottlerTempDisabled(bool disable);

// Returns the latest emulation speed (1 is full speed) (swings a lot)
double GetActualEmulationSpeed();

void Callback_FramePresented(double actual_emulation_speed = 1.0);
void Callback_NewField(Core::System& system);

enum class State
{
  Uninitialized,
  Paused,
  Running,
  Stopping,
  Starting,
};

enum class GameName : u8 {
  UnknownGame = 0,
  MarioBaseball = 1,
  ToadstoolTour = 2,
};

// Console type values based on:
//  - YAGCD 4.2.1.1.2
//  - OSInit (GameCube ELF from Finding Nemo)
//  - OSReportInfo (Wii ELF from Rayman Raving Rabbids)
enum class ConsoleType : u32
{
  // 0x0XXXXXXX Retail units - Gamecube
  HW1 = 1,
  HW2 = 2,
  LatestProductionBoard = 3,
  Reserved = 4,

  // 0x0XXXXXXX Retail units - Wii
  PreProductionBoard0 = 0x10,    // Pre-production board 0
  PreProductionBoard1 = 0x11,    // Pre-production board 1
  PreProductionBoard2_1 = 0x12,  // Pre-production board 2-1
  PreProductionBoard2_2 = 0x20,  // Pre-production board 2-2
  RVL_Retail1 = 0x21,
  RVL_Retail2 = 0x22,
  RVL_Retail3 = 0x23,
  RVA1 = 0x100,  // Revolution Arcade

  // 0x1XXXXXXX Devkits - Gamecube
  // Emulators
  MacEmulator = 0x10000000,  // Mac Emulator
  PcEmulator = 0x10000001,   // PC Emulator

  // Embedded PowerPC series
  Arthur = 0x10000002,  // EPPC Arthur
  Minnow = 0x10000003,  // EPPC Minnow

  // Development HW
  // Version = (console_type & 0x0fffffff) - 3
  FirstDevkit = 0x10000004,
  SecondDevkit = 0x10000005,
  LatestDevkit = 0x10000006,
  ReservedDevkit = 0x10000007,

  // 0x1XXXXXXX Devkits - Wii
  RevolutionEmulator = 0x10000008,  // Revolution Emulator
  NDEV1_0 = 0x10000010,             // NDEV 1.0
  NDEV1_1 = 0x10000011,             // NDEV 1.1
  NDEV1_2 = 0x10000012,             // NDEV 1.2
  NDEV2_0 = 0x10000020,             // NDEV 2.0
  NDEV2_1 = 0x10000021,             // NDEV 2.1

  // 0x2XXXXXXX TDEV-based emulation HW
  // Version = (console_type & 0x0fffffff) - 3
  HW2TDEVSystem = 0x20000005,
  LatestTDEVSystem = 0x20000006,
  ReservedTDEVSystem = 0x20000007,
};

// Run a function as the CPU thread. This is an RAII alternative to the RunAsCPUThread function.
//
// If constructed from the Host thread, the CPU thread is paused and the current thread temporarily
// becomes the CPU thread.
// If constructed from the CPU thread, nothing special happens.
//
// This should only be constructed from the CPU thread or the host thread.
//
// Some functions use a parameter of this type to indicate that the function should only be called
// from the CPU thread. If the parameter is a pointer, the function has a fallback for being called
// from the wrong thread (with the argument being set to nullptr).
class CPUThreadGuard final
{
public:
  explicit CPUThreadGuard(Core::System& system);
  ~CPUThreadGuard();

  CPUThreadGuard(const CPUThreadGuard&) = delete;
  CPUThreadGuard(CPUThreadGuard&&) = delete;
  CPUThreadGuard& operator=(const CPUThreadGuard&) = delete;
  CPUThreadGuard& operator=(CPUThreadGuard&&) = delete;

  Core::System& GetSystem() const { return m_system; }

private:
  Core::System& m_system;
  const bool m_was_cpu_thread;
  bool m_was_unpaused = false;
};

bool Init(std::unique_ptr<BootParameters> boot, const WindowSystemInfo& wsi);
void Stop();
void Shutdown();

void DeclareAsCPUThread();
void UndeclareAsCPUThread();
void DeclareAsGPUThread();
void UndeclareAsGPUThread();
void DeclareAsHostThread();
void UndeclareAsHostThread();

std::string StopMessage(bool main_thread, std::string_view message);

bool IsRunning();
bool IsRunningAndStarted();       // is running and the CPU loop has been entered
bool IsRunningInCurrentThread();  // this tells us whether we are running in the CPU thread.
bool IsCPUThread();               // this tells us whether we are the CPU thread.
bool IsGPUThread();
bool IsHostThread();

bool WantsDeterminism();

// [NOT THREADSAFE] For use by Host only
void SetState(State state);
State GetState();

void SaveScreenShot();
void SaveScreenShot(std::string_view name);

// This displays messages in a user-visible way.
void DisplayMessage(std::string message, int time_in_ms);
void CheckCurrentGame(const std::string& gameID);

void RunRioFunctions(const Core::CPUThreadGuard& guard);
void FrameUpdateOnCPUThread();
void OnFrameEnd();
bool IsGolfMode();

// Run a function as the CPU thread.
//
// If called from the Host thread, the CPU thread is paused and the current thread temporarily
// becomes the CPU thread while running the function.
// If called from the CPU thread, the function will be run directly.
//
// This should only be called from the CPU thread or the host thread.
void RunAsCPUThread(std::function<void()> function);

// Run a function on the CPU thread, asynchronously.
// This is only valid to call from the host thread, since it uses PauseAndLock() internally.
void RunOnCPUThread(std::function<void()> function, bool wait_for_completion);

// for calling back into UI code without introducing a dependency on it in core
using StateChangedCallbackFunc = std::function<void(Core::State)>;
// Returns a handle
int AddOnStateChangedCallback(StateChangedCallbackFunc callback);
// Also invalidates the handle
bool RemoveOnStateChangedCallback(int* handle);
void CallOnStateChangedCallbacks(Core::State state);

// Run on the Host thread when the factors change. [NOT THREADSAFE]
void UpdateWantDeterminism(bool initial = false);

// Queue an arbitrary function to asynchronously run once on the Host thread later.
// Threadsafe. Can be called by any thread, including the Host itself.
// Jobs will be executed in RELATIVE order. If you queue 2 jobs from the same thread
// then they will be executed in the order they were queued; however, there is no
// global order guarantee across threads - jobs from other threads may execute in
// between.
// NOTE: Make sure the jobs check the global state instead of assuming everything is
//   still the same as when the job was queued.
// NOTE: Jobs that are not set to run during stop will be discarded instead.
void QueueHostJob(std::function<void()> job, bool run_during_stop = false);

// Should be called periodically by the Host to run pending jobs.
// WMUserJobDispatch will be sent when something is added to the queue.
void HostDispatchJobs();

void DoFrameStep();

void UpdateInputGate(bool require_focus, bool require_full_focus = false);

void UpdateTitle();
float u32ToFloat(u32 value);
float ms_to_mph(float MetersPerSecond);
float vectorMagnitude(float x, float y, float z);
float RoundZ(float num);
bool isNight();
bool isDisableReplays();

void AutoGolfMode(const Core::CPUThreadGuard& guard, GameName currentGame);
void TrainingMode(const Core::CPUThreadGuard& guard);
void DisplayBatterFielder(const Core::CPUThreadGuard& guard);
void SetAvgPing(const Core::CPUThreadGuard& guard);
void SetNetplayerUserInfo();
void RunDraftTimer(const Core::CPUThreadGuard& guard);

//enum class GameMode
//{
//  StarsOff,
//  StarsOn,
//  Custom,
//};

// auto GameMode = GameMode::Custom;

using namespace Tag;

void SetGameID(u32 gameID);
std::optional<TagSet> GetActiveTagSet(bool netplay);
void SetTagSet(std::optional<TagSet> tagset, bool netplay);
bool isTagSetActive(std::optional<bool> netplay = std::nullopt);
std::optional<std::vector<std::string>> GetTagSetGeckoString();

union{
  u32 num;
  float fnum;
} float_converter;

// mssb addresses
static const u32 aOpponentPort = 0x802EBF92;
static const u32 aFielderPort = 0x802EBF94;
static const u32 aBatterPort = 0x802EBF95;
static const u32 aIsField = 0x8089389B;
static const u32 aIsInGame = 0x80871A6D;
static const u32 aContactMade = 0x808909A1;
static const u32 aContactFrame = 0x80890976;
static const u32 aTypeOfContact = 0x808909A2;
static const u32 aChargeUp = 0x80890968;
static const u32 aChargeDown = 0x8089096C;
static const u32 aBallAngle = 0x808926D4;
static const u32 aBallPosition_X = 0x80890B38;
static const u32 aBallPosition_Y = 0x80890B3C;
static const u32 aBallPosition_Z = 0x80890B40;
static const u32 aBallVelocity_X = 0x80890E50;
static const u32 aBallVelocity_Y = 0x80890E54;
static const u32 aBallVelocity_Z = 0x80890E58;
static const u32 aPitchedBallVelocity_X = 0x808909D8;
static const u32 aPitchedBallVelocity_Y = 0x808909DC;
static const u32 aPitchedBallVelocity_Z = 0x808909E0;
static const u32 aBarrelBatterPort = 0x80890971; // port of character at bat in barrel batter & bom-omb derby
static const u32 aWallBallPort = 0x80890AD9; // port of character pitching in wall ball
static const u32 aMinigameID = 0x808980DE;  // 3 == Barrel Batter; 2 == Wall Ball; 1 == Bom-omb Derby; 4 == Chain Chomp Sprint; 5 == Piranha Panic; 6 == Star Dash; 7 == Grand Prix
static const u32 aWhoPaused = 0x8039D7D3; // 2 == fielder, 1 == batter
//static const u32 aMatchStarted = 0x8036F3B8;  // bool for if a game is in session
static const u32 aSceneId = 0x800E877F;



// toadstooltour addresses
static const u32 aDistanceRemainingToHole = 0x802D7368;
static const u32 aShotAccuracy = 0x804ECD30;
static const u32 aPowerMeterDistance = 0x804ECD54;
static const u32 aCurrentShotAimAngle = 0x804ECD5C; // radians
static const u32 aSimLineEndpointX = 0x804ECD70;
static const u32 aSimLineEndpointZ = 0x804ECD74;
static const u32 aSimLineEndpointY = 0x804ECD78;
static const u32 aPreShotVerticalAdjustment = 0x804ECDA0;
static const u32 aPreShotHorizontalAdjustment = 0x804ECDA4;
static const u32 aActiveShotVerticalAdjustment = 0x804ECDA8;
static const u32 aActiveShotHorizontalAdjustment = 0x804ECDAC;

static const u32 aCurrentGolfer = 0x804E68FB;  // 0-3, indicates current golfing player
static const u32 aPlayerCount = 0x804E68FA; // indicates total player count
}  // namespace Core
