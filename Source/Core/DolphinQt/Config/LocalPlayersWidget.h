// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <array>

#include "Common/HttpRequest.h"
#include "Core/LocalPlayers.h"

class QComboBox;
class QHBoxLayout;
class QGridLayout;
class QGroupBox;
class QPushButton;
class QListWidget;
class QTextEdit;

class LocalPlayersWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit LocalPlayersWidget(QWidget* parent);

private:
  void SetPlayers();
  void CreateLayout();
  void OnAddPlayers();
  void OnRemovePlayers();
  void SavePlayers();
  void UpdatePlayers();

  void SetPortInfo();
  void PopulateTagsetCombobox();
  void SetTagSet();

  bool IsValidUser(LocalPlayers::LocalPlayers::Player player);

  void ConnectWidgets();

  QGroupBox* m_player_box;
  QGroupBox* m_options_box;
  QGridLayout* m_player_layout;
  QListWidget* m_player_list;

  QComboBox* m_player_list_0;
  QComboBox* m_player_list_1;
  QComboBox* m_player_list_2;
  QComboBox* m_player_list_3;
  QComboBox* m_player_list_4;
  std::array<QComboBox*, 5> m_port_array;

  QComboBox* m_local_tagset;
  QTextEdit* m_game_mode_description;
  Common::HttpRequest m_http{std::chrono::minutes{3}};

  QPushButton* m_add_button;
  QPushButton* m_remove_button;
  std::array<QHBoxLayout*, 4> m_player_groups;

  std::vector<LocalPlayers::LocalPlayers::Player> m_local_players; // vector of player objects
  std::map<std::string, std::string> m_player_map; // maps player key to username
  std::map<std::string, int> m_player_index_map; // maps player key to vector index
};
