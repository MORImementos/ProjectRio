// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Controller.h"

#include "Core/Config/MainSettings.h"
#include "Core/HW/GBAPadEmu.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/GBAPad.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/InputConfig.h"

namespace API
{

BaseManip::BaseManip(API::EventHub& event_hub,
          const std::vector<ControllerEmu::EmulatedController*> controllers)
    : m_event_hub(event_hub), m_controllers(controllers)
{
  m_frame_advanced_listener = m_event_hub.ListenEvent<API::Events::FrameAdvance>(
      [&](const API::Events::FrameAdvance&) { NotifyFrameAdvanced(); });
  for (auto i = 0; i < m_controllers.size(); i++)
  {
    // TODO felk: find a more robust way to set the input override functions.
    //   This way scripting breaks once the TAS input window is opened,
    //   and vice versa, the TAS window breaks once scripting starts.
    m_controllers[i]->SetInputOverrideFunction([=](const std::string_view group_name,
                                                   const std::string_view control_name,
                                                   ControlState orig_state) {
      const InputKey input_key = {group_name, control_name};
      std::optional<ControlState> manip = this->PerformInputManip(i, input_key, orig_state);
      m_last_seen_input[{i, input_key}] = manip.value_or(orig_state);
      return manip;
    });
  }
}

BaseManip::~BaseManip()
{
  m_event_hub.UnlistenEvent(m_frame_advanced_listener);
  // TODO felk: find a proper place to hook up unregistering
  /*for (const auto controller : m_controllers)
  {
    controller->ClearInputOverrideFunction();
  }*/
}

void BaseManip::NotifyFrameAdvanced()
{
  std::erase_if(m_overrides, [](const auto& kvp) {
    return kvp.second.clear_on == ClearOn::NextFrame && kvp.second.used;
  });
}

std::optional<ControlState>
BaseManip::PerformInputManip(int controller_id, const InputKey& input_key, ControlState orig_state)
{
  auto iter = m_overrides.find({controller_id, input_key});
  if (iter == m_overrides.end())
  {
    return std::nullopt;
  }
  InputOverride& input_override = iter->second;
  input_override.used = true;
  if (input_override.clear_on == ClearOn::NextPoll)
  {
    m_overrides.erase({controller_id, input_key});
  }

  return std::make_optional(input_override.state);
}

void BaseManip::Set(int controller_id, InputKey input_key, ControlState state, ClearOn clear_on)
{
  m_overrides[{controller_id, input_key}] = {state, clear_on, /* used: */ false};
}

ControlState BaseManip::Get(const int controller_id, const InputKey& input_key)
{
  auto iter = m_last_seen_input.find({controller_id, input_key});
  if (iter == m_last_seen_input.end())
  {
    return 0; // TODO felk: more sensible default?
  }
  return iter->second;
}

BaseManip& GetGCManip()
{
  const InputConfig* controller_config = Pad::GetConfig();
  const int num_controllers = controller_config->GetControllerCount();
  std::vector<ControllerEmu::EmulatedController*> controllers;
  for (int i = 0; i < num_controllers; i++)
  {
    controllers.push_back(controller_config->GetController(i));
  }

  static BaseManip manip(GetEventHub(), std::move(controllers));
  return manip;
}

BaseManip& GetWiiManip()
{
  const InputConfig* controller_config = Wiimote::GetConfig();
  const int num_controllers = controller_config->GetControllerCount();
  std::vector<ControllerEmu::EmulatedController*> controllers;
  for (int i = 0; i < num_controllers; i++)
  {
    controllers.push_back(controller_config->GetController(i));
  }

  static BaseManip manip(GetEventHub(), std::move(controllers));
  return manip;
}

BaseManip& GetWiiClassicManip()
{
  const InputConfig* controller_config = Wiimote::GetConfig();
  const int num_controllers = controller_config->GetControllerCount();
  std::vector<ControllerEmu::EmulatedController*> controllers;
  for (int i = 0; i < num_controllers; i++)
  {
    const auto* wiimote = static_cast<WiimoteEmu::Wiimote*>(controller_config->GetController(i));
    const auto* attachments_group = static_cast<ControllerEmu::Attachments*>(
        wiimote->GetWiimoteGroup(WiimoteEmu::WiimoteGroup::Attachments));
    const auto& attachments = attachments_group->GetAttachmentList();
    ControllerEmu::EmulatedController* classic_controller =
        attachments[WiimoteEmu::ExtensionNumber::CLASSIC].get();
    controllers.push_back(classic_controller);
  }

  static BaseManip manip(GetEventHub(), std::move(controllers));
  return manip;
}

BaseManip& GetWiiNunchukManip()
{
  const InputConfig* controller_config = Wiimote::GetConfig();
  const int num_controllers = controller_config->GetControllerCount();
  std::vector<ControllerEmu::EmulatedController*> controllers;
  for (int i = 0; i < num_controllers; i++)
  {
    const auto* wiimote = static_cast<WiimoteEmu::Wiimote*>(controller_config->GetController(i));
    const auto* attachments_group = static_cast<ControllerEmu::Attachments*>(
        wiimote->GetWiimoteGroup(WiimoteEmu::WiimoteGroup::Attachments));
    const auto& attachments = attachments_group->GetAttachmentList();
    ControllerEmu::EmulatedController* nunchuk_controller =
        attachments[WiimoteEmu::ExtensionNumber::NUNCHUK].get();
    controllers.push_back(nunchuk_controller);
  }

  static BaseManip manip(GetEventHub(), std::move(controllers));
  return manip;
}

BaseManip& GetGBAManip()
{
  const InputConfig* controller_config = Pad::GetGBAConfig();
  const int num_controllers = controller_config->GetControllerCount();
  std::vector<ControllerEmu::EmulatedController*> controllers;
  for (int i = 0; i < num_controllers; i++)
  {
    controllers.push_back(controller_config->GetController(i));
  }

  static BaseManip manip(GetEventHub(), std::move(controllers));
  return manip;
}

using XYInput = ControllerEmu::ReshapableInput;
using Wii = WiimoteEmu::Wiimote;
using WiiClassic = WiimoteEmu::Classic;
using WiiNunchuk = WiimoteEmu::Nunchuk;
using GBA = GBAPad;

const InputKey InputKey::GC_A = {GCPad::BUTTONS_GROUP, GCPad::A_BUTTON};
const InputKey InputKey::GC_B = {GCPad::BUTTONS_GROUP, GCPad::B_BUTTON};
const InputKey InputKey::GC_X = {GCPad::BUTTONS_GROUP, GCPad::X_BUTTON};
const InputKey InputKey::GC_Y = {GCPad::BUTTONS_GROUP, GCPad::Y_BUTTON};
const InputKey InputKey::GC_Z = {GCPad::BUTTONS_GROUP, GCPad::Z_BUTTON};
const InputKey InputKey::GC_START = {GCPad::BUTTONS_GROUP, GCPad::START_BUTTON};
const InputKey InputKey::GC_UP = {GCPad::DPAD_GROUP, DIRECTION_UP};
const InputKey InputKey::GC_DOWN = {GCPad::DPAD_GROUP, DIRECTION_DOWN};
const InputKey InputKey::GC_LEFT = {GCPad::DPAD_GROUP, DIRECTION_LEFT};
const InputKey InputKey::GC_RIGHT = {GCPad::DPAD_GROUP, DIRECTION_RIGHT};
const InputKey InputKey::GC_L = {GCPad::TRIGGERS_GROUP, GCPad::L_DIGITAL};
const InputKey InputKey::GC_R = {GCPad::TRIGGERS_GROUP, GCPad::R_DIGITAL};
const InputKey InputKey::GC_L_ANALOG = {GCPad::TRIGGERS_GROUP, GCPad::L_ANALOG};
const InputKey InputKey::GC_R_ANALOG = {GCPad::TRIGGERS_GROUP, GCPad::R_ANALOG};
const InputKey InputKey::GC_STICK_X = {GCPad::MAIN_STICK_GROUP, XYInput::X_INPUT_OVERRIDE};
const InputKey InputKey::GC_STICK_Y = {GCPad::MAIN_STICK_GROUP, XYInput::Y_INPUT_OVERRIDE};
const InputKey InputKey::GC_C_STICK_X = {GCPad::C_STICK_GROUP, XYInput::X_INPUT_OVERRIDE};
const InputKey InputKey::GC_C_STICK_Y = {GCPad::C_STICK_GROUP, XYInput::Y_INPUT_OVERRIDE};

const InputKey InputKey::WII_A = {Wii::BUTTONS_GROUP, Wii::A_BUTTON};
const InputKey InputKey::WII_B = {Wii::BUTTONS_GROUP, Wii::B_BUTTON};
const InputKey InputKey::WII_ONE = {Wii::BUTTONS_GROUP, Wii::ONE_BUTTON};
const InputKey InputKey::WII_TWO = {Wii::BUTTONS_GROUP, Wii::TWO_BUTTON};
const InputKey InputKey::WII_PLUS = {Wii::BUTTONS_GROUP, Wii::PLUS_BUTTON};
const InputKey InputKey::WII_MINUS = {Wii::BUTTONS_GROUP, Wii::MINUS_BUTTON};
const InputKey InputKey::WII_HOME = {Wii::BUTTONS_GROUP, Wii::HOME_BUTTON};
const InputKey InputKey::WII_UP = {Wii::DPAD_GROUP, DIRECTION_UP};
const InputKey InputKey::WII_DOWN = {Wii::DPAD_GROUP, DIRECTION_DOWN};
const InputKey InputKey::WII_LEFT = {Wii::DPAD_GROUP, DIRECTION_LEFT};
const InputKey InputKey::WII_RIGHT = {Wii::DPAD_GROUP, DIRECTION_RIGHT};
const InputKey InputKey::WII_IR_X = {Wii::IR_GROUP, XYInput::X_INPUT_OVERRIDE};
const InputKey InputKey::WII_IR_Y = {Wii::IR_GROUP, XYInput::Y_INPUT_OVERRIDE};
const InputKey InputKey::WII_ACCELERATION_X = {Wii::ACCELEROMETER_GROUP, XYInput::X_INPUT_OVERRIDE};
const InputKey InputKey::WII_ACCELERATION_Y = {Wii::ACCELEROMETER_GROUP, XYInput::Y_INPUT_OVERRIDE};
const InputKey InputKey::WII_ACCELERATION_Z = {Wii::ACCELEROMETER_GROUP, XYInput::Z_INPUT_OVERRIDE};
const InputKey InputKey::WII_ANGULAR_VELOCITY_X = {Wii::GYROSCOPE_GROUP, XYInput::X_INPUT_OVERRIDE};
const InputKey InputKey::WII_ANGULAR_VELOCITY_Y = {Wii::GYROSCOPE_GROUP, XYInput::Y_INPUT_OVERRIDE};
const InputKey InputKey::WII_ANGULAR_VELOCITY_Z = {Wii::GYROSCOPE_GROUP, XYInput::Z_INPUT_OVERRIDE};

const InputKey InputKey::WII_CLASSIC_A = {WiiClassic::BUTTONS_GROUP, WiiClassic::A_BUTTON};
const InputKey InputKey::WII_CLASSIC_B = {WiiClassic::BUTTONS_GROUP, WiiClassic::B_BUTTON};
const InputKey InputKey::WII_CLASSIC_X = {WiiClassic::BUTTONS_GROUP, WiiClassic::X_BUTTON};
const InputKey InputKey::WII_CLASSIC_Y = {WiiClassic::BUTTONS_GROUP, WiiClassic::Y_BUTTON};
const InputKey InputKey::WII_CLASSIC_ZL = {WiiClassic::BUTTONS_GROUP, WiiClassic::ZL_BUTTON};
const InputKey InputKey::WII_CLASSIC_ZR = {WiiClassic::BUTTONS_GROUP, WiiClassic::ZR_BUTTON};
const InputKey InputKey::WII_CLASSIC_PLUS = {WiiClassic::BUTTONS_GROUP, WiiClassic::PLUS_BUTTON};
const InputKey InputKey::WII_CLASSIC_MINUS = {WiiClassic::BUTTONS_GROUP, WiiClassic::MINUS_BUTTON};
const InputKey InputKey::WII_CLASSIC_HOME = {WiiClassic::BUTTONS_GROUP, WiiClassic::HOME_BUTTON};
const InputKey InputKey::WII_CLASSIC_UP = {WiiClassic::DPAD_GROUP, DIRECTION_UP};
const InputKey InputKey::WII_CLASSIC_DOWN = {WiiClassic::DPAD_GROUP, DIRECTION_DOWN};
const InputKey InputKey::WII_CLASSIC_LEFT = {WiiClassic::DPAD_GROUP, DIRECTION_LEFT};
const InputKey InputKey::WII_CLASSIC_RIGHT = {WiiClassic::DPAD_GROUP, DIRECTION_RIGHT};
const InputKey InputKey::WII_CLASSIC_L = {WiiClassic::TRIGGERS_GROUP, WiiClassic::L_DIGITAL};
const InputKey InputKey::WII_CLASSIC_R = {WiiClassic::TRIGGERS_GROUP, WiiClassic::R_DIGITAL};
const InputKey InputKey::WII_CLASSIC_L_ANALOG = {WiiClassic::TRIGGERS_GROUP, WiiClassic::L_ANALOG};
const InputKey InputKey::WII_CLASSIC_R_ANALOG = {WiiClassic::TRIGGERS_GROUP, WiiClassic::R_ANALOG};
const InputKey InputKey::WII_CLASSIC_LEFT_STICK_X = {WiiClassic::LEFT_STICK_GROUP, XYInput::X_INPUT_OVERRIDE};
const InputKey InputKey::WII_CLASSIC_LEFT_STICK_Y = {WiiClassic::LEFT_STICK_GROUP, XYInput::Y_INPUT_OVERRIDE};
const InputKey InputKey::WII_CLASSIC_RIGHT_STICK_X = {WiiClassic::RIGHT_STICK_GROUP, XYInput::X_INPUT_OVERRIDE};
const InputKey InputKey::WII_CLASSIC_RIGHT_STICK_Y = {WiiClassic::RIGHT_STICK_GROUP, XYInput::Y_INPUT_OVERRIDE};

const InputKey InputKey::WII_NUNCHUK_C = {WiiNunchuk::BUTTONS_GROUP, WiiNunchuk::C_BUTTON};
const InputKey InputKey::WII_NUNCHUK_Z = {WiiNunchuk::BUTTONS_GROUP, WiiNunchuk::Z_BUTTON};
const InputKey InputKey::WII_NUNCHUK_STICK_X = {WiiNunchuk::STICK_GROUP, XYInput::X_INPUT_OVERRIDE};
const InputKey InputKey::WII_NUNCHUK_STICK_Y = {WiiNunchuk::STICK_GROUP, XYInput::Y_INPUT_OVERRIDE};
const InputKey InputKey::WII_NUNCHUCK_ACCELERATION_X = {WiiNunchuk::ACCELEROMETER_GROUP, XYInput::X_INPUT_OVERRIDE};
const InputKey InputKey::WII_NUNCHUCK_ACCELERATION_Y = {WiiNunchuk::ACCELEROMETER_GROUP, XYInput::Y_INPUT_OVERRIDE};
const InputKey InputKey::WII_NUNCHUCK_ACCELERATION_Z = {WiiNunchuk::ACCELEROMETER_GROUP, XYInput::Z_INPUT_OVERRIDE};

const InputKey InputKey::GBA_A = {GBA::BUTTONS_GROUP, GBA::A_BUTTON};
const InputKey InputKey::GBA_B = {GBA::BUTTONS_GROUP, GBA::B_BUTTON};
const InputKey InputKey::GBA_L = {GBA::BUTTONS_GROUP, GBA::L_BUTTON};
const InputKey InputKey::GBA_R = {GBA::BUTTONS_GROUP, GBA::R_BUTTON};
const InputKey InputKey::GBA_START = {GBA::BUTTONS_GROUP, GBA::START_BUTTON};
const InputKey InputKey::GBA_SELECT = {GBA::BUTTONS_GROUP, GBA::SELECT_BUTTON};
const InputKey InputKey::GBA_UP = {GBA::DPAD_GROUP, DIRECTION_UP};
const InputKey InputKey::GBA_DOWN = {GBA::DPAD_GROUP, DIRECTION_DOWN};
const InputKey InputKey::GBA_LEFT = {GBA::DPAD_GROUP, DIRECTION_LEFT};
const InputKey InputKey::GBA_RIGHT = {GBA::DPAD_GROUP, DIRECTION_RIGHT};

}  // namespace API
