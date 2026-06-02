// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/ToolBar.h"

#include <algorithm>
#include <vector>

#include <QAction>
#include <QIcon>

#include "Core/Core.h"
#include "Core/NetPlayProto.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"



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

  connect(&Settings::Instance(), &Settings::ToolBarVisibilityChanged, this,
          &ToolBar::setVisible);

  OnEmulationStateChanged(Core::GetState());
}

void ToolBar::OnEmulationStateChanged(Core::State state)
{
  bool running = state != Core::State::Uninitialized;
  m_fullscreen_button->setEnabled(running);
}

void ToolBar::closeEvent(QCloseEvent*)
{
  Settings::Instance().SetToolBarVisible(false);
}

void ToolBar::MakeActions()
{
  m_local_play_action = addAction(tr("Rio Config"), this, &ToolBar::ViewLocalPlayers);
  m_start_netplay_action = addAction(tr("Online Play"), this, &ToolBar::StartNetPlayPressed);

  addSeparator();

  m_config_action = addAction(tr("Config"), this, &ToolBar::SettingsPressed);
  m_graphics_action = addAction(tr("Graphics"), this, &ToolBar::GraphicsPressed);
  m_controllers_action = addAction(tr("Controllers"), this, &ToolBar::ControllersPressed);

  m_view_gecko_codes_action = addAction(tr("Mods"), this, &ToolBar::ViewGeckoCodes);
  m_view_texture_packs_action = addAction(tr("Textures"), this, &ToolBar::ViewTexturePacks);

  addSeparator();

  m_discord_action = addAction(tr("Discord"), this, &ToolBar::DiscordPressed);
  m_fullscreen_action = addAction(tr("FullScr"), this, &ToolBar::FullScreenPressed);
// m_screenshot_action = addAction(tr("ScrShot"), this, &ToolBar::ScreenShotPressed);

  // Ensure every button has about the same width
  std::vector<QWidget*> items;
  for (const auto& action : {
        m_open_action, m_pause_play_action, m_stop_action, m_stop_action, m_local_play_action,
        m_start_netplay_action, m_discord_action, m_fullscreen_action,
        m_config_action, m_graphics_action, m_controllers_action,
        m_step_action, m_step_over_action, m_step_out_action, m_skip_action, m_show_pc_action,
        m_set_pc_action})
  {
    items.emplace_back(widgetForAction(action));
  }

  std::vector<int> widths;
  std::transform(items.begin(), items.end(), std::back_inserter(widths),
                 [](QWidget* item) { return item->sizeHint().width(); });
  std::vector<int> heights;
  std::transform(items.begin(), items.end(), std::back_inserter(heights),
                 [](QWidget* item) { return item->sizeHint().height(); });

  const int min_width = *std::max_element(widths.begin(), widths.end());
  const int min_height = *std::max_element(heights.begin(), heights.end());
  for (QWidget* widget : items)
  {
    widget->setMinimumWidth(min_width);
    widget->setMinimumHeight(min_height);
  }
}

void ToolBar::UpdateIcons()
{
  m_fullscreen_action->setIcon(Resources::GetThemeIcon("fullscreen"));
  m_config_action->setIcon(Resources::GetThemeIcon("config"));
  m_controllers_action->setIcon(Resources::GetThemeIcon("classic"));
  m_graphics_action->setIcon(Resources::GetThemeIcon("graphics"));
  m_start_netplay_action->setIcon(Resources::GetThemeIcon("wifi"));
  m_view_gecko_codes_action->setIcon(Resources::GetThemeIcon("debugger_add_breakpoint@2x"));
  m_view_texture_packs_action->setIcon(Resources::GetThemeIcon("brush"));
  m_local_play_action->setIcon(Resources::GetThemeIcon("rio"));
  m_discord_action->setIcon(Resources::GetThemeIcon("discord"));
}
