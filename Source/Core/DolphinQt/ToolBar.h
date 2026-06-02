// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QToolBar>

class QAction;

namespace Core
{
enum class State;
}

class ToolBar final : public QToolBar
{
  Q_OBJECT

public:
  explicit ToolBar(QWidget* parent = nullptr);

  void closeEvent(QCloseEvent*) override;
signals:
  void FullScreenPressed();

  void SettingsPressed();
  void ControllersPressed();
  void GraphicsPressed();

  void StartNetPlayPressed();
  void ViewGeckoCodes();
  void ViewTexturePacks();
  void ViewLocalPlayers();
  void DiscordPressed();

private:
  void OnEmulationStateChanged(Core::State state);

  void MakeActions();

  QAction* m_fullscreen_action;
  QAction* m_config_action;
  QAction* m_start_netplay_action;
  QAction* m_view_gecko_codes_action;
  QAction* m_view_texture_packs_action;
  QAction* m_controllers_action;
  QAction* m_graphics_action;
  QAction* m_local_play_action;
  QAction* m_discord_action;
};
