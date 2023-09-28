// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/API/Events.h"
#include "Core/HW/WiimoteCommon/DataReport.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/InputConfig.h"

namespace API
{

enum class ClearOn
{
  NextPoll = 0,
  NextFrame = 1,
  NextOverride = 2,
};

struct InputKey
{
  static const InputKey GC_A;
  static const InputKey GC_B;
  static const InputKey GC_X;
  static const InputKey GC_Y;
  static const InputKey GC_Z;
  static const InputKey GC_START;
  static const InputKey GC_UP;
  static const InputKey GC_DOWN;
  static const InputKey GC_LEFT;
  static const InputKey GC_RIGHT;
  static const InputKey GC_L;
  static const InputKey GC_R;
  static const InputKey GC_L_ANALOG;
  static const InputKey GC_R_ANALOG;
  static const InputKey GC_STICK_X;
  static const InputKey GC_STICK_Y;
  static const InputKey GC_C_STICK_X;
  static const InputKey GC_C_STICK_Y;

  static const InputKey WII_A;
  static const InputKey WII_B;
  static const InputKey WII_ONE;
  static const InputKey WII_TWO;
  static const InputKey WII_PLUS;
  static const InputKey WII_MINUS;
  static const InputKey WII_HOME;
  static const InputKey WII_UP;
  static const InputKey WII_DOWN;
  static const InputKey WII_LEFT;
  static const InputKey WII_RIGHT;
  static const InputKey WII_IR_X;
  static const InputKey WII_IR_Y;
  static const InputKey WII_ACCELERATION_X;
  static const InputKey WII_ACCELERATION_Y;
  static const InputKey WII_ACCELERATION_Z;
  static const InputKey WII_ANGULAR_VELOCITY_X;
  static const InputKey WII_ANGULAR_VELOCITY_Y;
  static const InputKey WII_ANGULAR_VELOCITY_Z;

  static const InputKey WII_CLASSIC_A;
  static const InputKey WII_CLASSIC_B;
  static const InputKey WII_CLASSIC_X;
  static const InputKey WII_CLASSIC_Y;
  static const InputKey WII_CLASSIC_ZL;
  static const InputKey WII_CLASSIC_ZR;
  static const InputKey WII_CLASSIC_PLUS;
  static const InputKey WII_CLASSIC_MINUS;
  static const InputKey WII_CLASSIC_HOME;
  static const InputKey WII_CLASSIC_UP;
  static const InputKey WII_CLASSIC_DOWN;
  static const InputKey WII_CLASSIC_LEFT;
  static const InputKey WII_CLASSIC_RIGHT;
  static const InputKey WII_CLASSIC_L;
  static const InputKey WII_CLASSIC_R;
  static const InputKey WII_CLASSIC_L_ANALOG;
  static const InputKey WII_CLASSIC_R_ANALOG;
  static const InputKey WII_CLASSIC_LEFT_STICK_X;
  static const InputKey WII_CLASSIC_LEFT_STICK_Y;
  static const InputKey WII_CLASSIC_RIGHT_STICK_X;
  static const InputKey WII_CLASSIC_RIGHT_STICK_Y;

  static const InputKey WII_NUNCHUK_C;
  static const InputKey WII_NUNCHUK_Z;
  static const InputKey WII_NUNCHUK_STICK_X;
  static const InputKey WII_NUNCHUK_STICK_Y;
  static const InputKey WII_NUNCHUCK_ACCELERATION_X;
  static const InputKey WII_NUNCHUCK_ACCELERATION_Y;
  static const InputKey WII_NUNCHUCK_ACCELERATION_Z;

  static const InputKey GBA_A;
  static const InputKey GBA_B;
  static const InputKey GBA_L;
  static const InputKey GBA_R;
  static const InputKey GBA_START;
  static const InputKey GBA_SELECT;
  static const InputKey GBA_UP;
  static const InputKey GBA_DOWN;
  static const InputKey GBA_LEFT;
  static const InputKey GBA_RIGHT;

  std::string_view group_name;
  std::string_view control_name;

  bool operator==(const InputKey& o) const
  {
    return group_name == o.group_name && control_name == o.control_name;
  }

  bool operator<(const InputKey& o) const
  {
    return group_name < o.group_name ||
           (group_name == o.group_name && control_name < o.control_name);
  }
};

struct InputOverride
{
  ControlState state;
  ClearOn clear_on;
  bool used;
};

class BaseManip
{
public:
  BaseManip(API::EventHub& event_hub,
            const std::vector<ControllerEmu::EmulatedController*> controllers);
  ~BaseManip();
  ControlState Get(int controller_id, const InputKey& input_key);
  void Set(int controller_id, InputKey input_key, ControlState state, ClearOn clear_on);
  void Clear() { m_overrides.clear(); }
  void NotifyFrameAdvanced();
  std::optional<ControlState> PerformInputManip(int controller_id, const InputKey& input_key,
                                                ControlState orig_state);

private:
  std::map<std::tuple<int, InputKey>, InputOverride> m_overrides;
  std::map<std::tuple<int, InputKey>, ControlState> m_last_seen_input;
  EventHub& m_event_hub;
  ListenerID<Events::FrameAdvance> m_frame_advanced_listener;
  std::vector<ControllerEmu::EmulatedController*> m_controllers;
};

// global instances
BaseManip& GetGCManip();
BaseManip& GetWiiManip();
BaseManip& GetWiiClassicManip();
BaseManip& GetWiiNunchukManip();
BaseManip& GetGBAManip();

}  // namespace API
