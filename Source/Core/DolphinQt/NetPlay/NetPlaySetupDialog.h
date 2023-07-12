// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "DolphinQt/GameList/GameListModel.h"
#include "UICommon/NetPlayIndex.h"
#include "Core/LocalPlayers.h"
//#include "Common/TagSet.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QGridLayout;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QGroupBox;
class QTableWidget;
class QRadioButton;
class QTextEdit;

namespace UICommon
{
class GameFile;
}

namespace Tag {
class TagSet;
}

class NetPlaySetupDialog : public QDialog
{
  Q_OBJECT
public:
  explicit NetPlaySetupDialog(const GameListModel& game_list_model, QWidget* parent);
  ~NetPlaySetupDialog();

  void accept() override;
  void show();
  std::map<int, Tag::TagSet>& assignOnlineAccount(LocalPlayers::LocalPlayers::Player online_player);
  std::optional<Tag::TagSet> GetTagSet();

signals:
  bool Join();
  void JoinBrowser();
  bool Host(const UICommon::GameFile& game);
  void UpdateStatusRequestedBrowser(const QString& status);
  void UpdateListRequestedBrowser(std::vector<NetPlaySession> sessions);

private:
  void CreateMainLayout();
  void ConnectWidgets();
  void PopulateGameList();
  void ResetTraversalHost();
  void OpenInternetTest();
  std::string LobbyNameString();
  void UpdateGameModeDescription();

  // Browser Stuff
  void RefreshBrowser();
  void RefreshLoopBrowser();
  void UpdateListBrowser();
  void OnSelectionChangedBrowser();
  void OnUpdateStatusRequestedBrowser(const QString& status);
  void OnUpdateListRequestedBrowser(std::vector<NetPlaySession> sessions);
  void acceptBrowser();

  void SaveSettings();
  //void SaveLobbySettings();

  void OnConnectionTypeChanged(int index);

  // Main Widget
  QDialogButtonBox* m_button_box;
  QComboBox* m_connection_type;
  QGridLayout* m_main_layout;
  QTabWidget* m_tab_widget;
  QPushButton* m_reset_traversal_button;
  QPushButton* m_latency_test;
  QLabel* m_account_name;
  QGroupBox* m_account_box;

  // Connection Widget
  QLabel* m_ip_label;
  QLineEdit* m_ip_edit;
  QLabel* m_connect_port_label;
  QSpinBox* m_connect_port_box;
  QPushButton* m_connect_button;

  // Host Widget
  QLabel* m_host_port_label;
  QSpinBox* m_host_port_box;
  QComboBox* m_host_games;
  std::map<int, UICommon::GameFile> host_games_map;
  QPushButton* m_host_button;
  QCheckBox* m_host_force_port_check;
  QSpinBox* m_host_force_port_box;
  QCheckBox* m_host_chunked_upload_limit_check;
  QSpinBox* m_host_chunked_upload_limit_box;
  QCheckBox* m_host_server_browser;
  QLineEdit* m_host_server_name;
  QLineEdit* m_host_server_password;
  QComboBox* m_host_server_region;
  QComboBox* m_host_game_mode;
  QTextEdit* m_game_mode_description;

  // Browser Tab
  QTableWidget* m_table_widget;
  QComboBox* m_region_combo;
  QLabel* m_status_label;
  QDialogButtonBox* m_b_button_box;
  QPushButton* m_button_refresh;
  QLineEdit* m_edit_name;
  QCheckBox* m_check_hide_ingame;
  QRadioButton* m_radio_all;
  QRadioButton* m_radio_private;
  QRadioButton* m_radio_public;
  QLabel* m_online_count;

  std::vector<NetPlaySession> m_sessions;
  LocalPlayers::LocalPlayers::Player m_active_account;

  std::thread m_refresh_thread;
  std::optional<std::map<std::string, std::string>> m_refresh_filters;
  std::mutex m_refresh_filters_mutex;
  Common::Flag m_refresh_run;
  Common::Event m_refresh_event;

#ifdef USE_UPNP
  QCheckBox* m_host_upnp;
#endif

  std::map<int, Tag::TagSet> user_tagsets;
  std::map<int, std::optional<Tag::TagSet>> tagset_map; // maps the index of the tagset combo box to the tagset id
  const GameListModel& m_game_list_model;
  Common::HttpRequest m_http{std::chrono::minutes{3}};
};
