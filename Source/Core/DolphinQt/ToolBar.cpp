// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <vector>

#include <QAction>
#include <QIcon>

#include "Core/Core.h"
#include "Core/NetPlayProto.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/ToolBar.h"



static QSize ICON_SIZE(32, 32);

ToolBar::ToolBar(QWidget* parent) : QToolBar(parent)
{
  setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  setMovable(!Settings::Instance().AreWidgetsLocked());
  setFloatable(false);
  setIconSize(ICON_SIZE);
  setVisible(Settings::Instance().IsToolBarVisible());

  setWindowTitle(tr("Toolbar"));
  setObjectName(QStringLiteral("toolbar"));

  MakeActions();
  connect(&Settings::Instance(), &Settings::ThemeChanged, this, &ToolBar::UpdateIcons);
  UpdateIcons();

  connect(&Settings::Instance(), &Settings::EmulationStateChanged, this,
          [this](Core::State state) { OnEmulationStateChanged(state); });

  connect(Host::GetInstance(), &Host::UpdateDisasmDialog, this,
          [this] { OnEmulationStateChanged(Core::GetState()); });

  connect(&Settings::Instance(), &Settings::DebugModeToggled, this, &ToolBar::OnDebugModeToggled);

  connect(&Settings::Instance(), &Settings::ToolBarVisibilityChanged, this, &ToolBar::setVisible);
  connect(this, &ToolBar::visibilityChanged, &Settings::Instance(), &Settings::SetToolBarVisible);

  connect(&Settings::Instance(), &Settings::WidgetLockChanged, this,
          [this](bool locked) { setMovable(!locked); });

  connect(&Settings::Instance(), &Settings::GameListRefreshRequested, this,
          [this] { m_refresh_action->setEnabled(false); });
  connect(&Settings::Instance(), &Settings::GameListRefreshStarted, this,
          [this] { m_refresh_action->setEnabled(true); });

  OnEmulationStateChanged(Core::GetState());
  OnDebugModeToggled(Settings::Instance().IsDebugModeEnabled());
}

void ToolBar::OnEmulationStateChanged(Core::State state)
{
  bool running = state != Core::State::Uninitialized;
  m_stop_action->setEnabled(running);
  m_fullscreen_action->setEnabled(running);
  m_screenshot_action->setEnabled(running);
  m_controllers_action->setEnabled(NetPlay::IsNetPlayRunning() ? !running : true);

  bool playing = running && state != Core::State::Paused;
  UpdatePausePlayButtonState(playing);

  bool paused = Core::GetState() == Core::State::Paused;
  m_step_action->setEnabled(paused);
  m_step_over_action->setEnabled(paused);
  m_step_out_action->setEnabled(paused);
  m_skip_action->setEnabled(paused);
  m_set_pc_action->setEnabled(paused);
}

void ToolBar::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetToolBarVisible(false);
}

void ToolBar::OnDebugModeToggled(bool enabled)
{
  m_step_action->setVisible(enabled);
  m_step_over_action->setVisible(enabled);
  m_step_out_action->setVisible(enabled);
  m_skip_action->setVisible(enabled);
  m_show_pc_action->setVisible(enabled);
  m_set_pc_action->setVisible(enabled);
  m_open_action->setVisible(enabled);
  m_refresh_action->setVisible(enabled);
  m_pause_play_action->setVisible(enabled);
  m_stop_action->setVisible(enabled);

  bool paused = Core::GetState() == Core::State::Paused;
  m_step_action->setEnabled(paused);
  m_step_over_action->setEnabled(paused);
  m_step_out_action->setEnabled(paused);
  m_skip_action->setEnabled(paused);
  m_set_pc_action->setEnabled(paused);
}

void ToolBar::MakeActions()
{
  // i18n: Here, "Step" is a verb. This feature is used for
  // going through code step by step.
  m_step_action = addAction(tr("Step"), this, &ToolBar::StepPressed);
  // i18n: Here, "Step" is a verb. This feature is used for
  // going through code step by step.
  m_step_over_action = addAction(tr("Step Over"), this, &ToolBar::StepOverPressed);
  // i18n: Here, "Step" is a verb. This feature is used for
  // going through code step by step.
  m_step_out_action = addAction(tr("Step Out"), this, &ToolBar::StepOutPressed);
  m_skip_action = addAction(tr("Skip"), this, &ToolBar::SkipPressed);
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  m_show_pc_action = addAction(tr("Show PC"), this, &ToolBar::ShowPCPressed);
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  m_set_pc_action = addAction(tr("Set PC"), this, &ToolBar::SetPCPressed);

  m_open_action = addAction(tr("Open"), this, &ToolBar::OpenPressed);
  m_refresh_action = addAction(tr("Refresh"), [this] { emit RefreshPressed(); });
  m_refresh_action->setEnabled(false);

  // addSeparator();

  m_pause_play_action = addAction(tr("Play"), this, &ToolBar::PlayPressed);

  m_stop_action = addAction(tr("Stop"), this, &ToolBar::StopPressed);
  m_fullscreen_action = addAction(tr("FullScr"), this, &ToolBar::FullScreenPressed);
  m_screenshot_action = addAction(tr("ScrShot"), this, &ToolBar::ScreenShotPressed);

  addSeparator();

  m_start_netplay_action = addAction(tr("Online Play"), this, &ToolBar::StartNetPlayPressed);

  addSeparator();

  m_config_action = addAction(tr("Config"), this, &ToolBar::SettingsPressed);
  m_graphics_action = addAction(tr("Graphics"), this, &ToolBar::GraphicsPressed);
  m_controllers_action = addAction(tr("Controllers"), this, &ToolBar::ControllersPressed);
  m_controllers_action->setEnabled(true);

  addSeparator();

  m_view_gecko_codes_action = addAction(tr("Gecko Codes"), this, &ToolBar::ViewGeckoCodes);
  m_local_players_action = addAction(tr("Local Players"), this, &ToolBar::ViewLocalPlayers);

  // Ensure every button has about the same width
  std::vector<QWidget*> items;
  for (const auto& action :
       {m_open_action, m_pause_play_action, m_stop_action, m_stop_action, m_fullscreen_action,
        m_screenshot_action, m_config_action, m_graphics_action, m_controllers_action,
        m_step_action, m_step_over_action, m_step_out_action, m_skip_action, m_show_pc_action,
        m_set_pc_action})
  {
    items.emplace_back(widgetForAction(action));
  }

  std::vector<int> widths;
  std::transform(items.begin(), items.end(), std::back_inserter(widths),
                 [](QWidget* item) { return item->sizeHint().width(); });

  const int min_width = *std::max_element(widths.begin(), widths.end()) * 0.85;
  for (QWidget* widget : items)
    widget->setMinimumWidth(min_width);
}

void ToolBar::UpdatePausePlayButtonState(const bool playing_state)
{
  if (playing_state)
  {
    disconnect(m_pause_play_action, 0, 0, 0);
    m_pause_play_action->setText(tr("Pause"));
    m_pause_play_action->setIcon(Resources::GetScaledThemeIcon("pause"));
    connect(m_pause_play_action, &QAction::triggered, this, &ToolBar::PausePressed);
  }
  else
  {
    disconnect(m_pause_play_action, 0, 0, 0);
    m_pause_play_action->setText(tr("Play"));
    m_pause_play_action->setIcon(Resources::GetScaledThemeIcon("play"));
    connect(m_pause_play_action, &QAction::triggered, this, &ToolBar::PlayPressed);
  }
}

void ToolBar::UpdateIcons()
{
  m_step_action->setIcon(Resources::GetScaledThemeIcon("debugger_step_in"));
  m_step_over_action->setIcon(Resources::GetScaledThemeIcon("debugger_step_over"));
  m_step_out_action->setIcon(Resources::GetScaledThemeIcon("debugger_step_out"));
  m_skip_action->setIcon(Resources::GetScaledThemeIcon("debugger_skip"));
  m_show_pc_action->setIcon(Resources::GetScaledThemeIcon("debugger_show_pc"));
  m_set_pc_action->setIcon(Resources::GetScaledThemeIcon("debugger_set_pc"));

  m_open_action->setIcon(Resources::GetScaledThemeIcon("open"));
  m_refresh_action->setIcon(Resources::GetScaledThemeIcon("refresh"));

  const Core::State state = Core::GetState();
  const bool playing = state != Core::State::Uninitialized && state != Core::State::Paused;
  if (!playing)
    m_pause_play_action->setIcon(Resources::GetScaledThemeIcon("play"));
  else
    m_pause_play_action->setIcon(Resources::GetScaledThemeIcon("pause"));

  m_stop_action->setIcon(Resources::GetScaledThemeIcon("stop"));
  m_fullscreen_action->setIcon(Resources::GetScaledThemeIcon("fullscreen"));
  m_screenshot_action->setIcon(Resources::GetScaledThemeIcon("screenshot"));
  m_config_action->setIcon(Resources::GetScaledThemeIcon("config"));
  m_controllers_action->setIcon(Resources::GetScaledThemeIcon("classic"));
  m_graphics_action->setIcon(Resources::GetScaledThemeIcon("graphics"));
  m_start_netplay_action->setIcon(Resources::GetScaledThemeIcon("wifi"));
  m_view_gecko_codes_action->setIcon(Resources::GetScaledThemeIcon("debugger_add_breakpoint@2x"));
  m_local_players_action->setIcon(Resources::GetScaledThemeIcon("browse"));
}
