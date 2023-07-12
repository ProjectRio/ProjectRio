// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/NetPlaySetupDialog.h"

#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QRadioButton>
#include <QHeaderView>
#include <QInputDialog>
#include <QUrl>
#include <QTextEdit>
#include <QScrollBar>

#include "Core/Config/NetplaySettings.h"
#include "Core/NetPlayProto.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/UTF8CodePointCountValidator.h"
#include "DolphinQt/Settings.h"

#include "UICommon/GameFile.h"
#include "UICommon/NetPlayIndex.h"
#include "DolphinQt/NetPlay/NetPlayBrowser.h"
#include "Common/Version.h"
#include <qdesktopservices.h>
#include "Core/Core.h"
//#include "Core/NetPlayServer.h"
#include "Common/TagSet.h"


NetPlaySetupDialog::NetPlaySetupDialog(const GameListModel& game_list_model, QWidget* parent)
    : QDialog(parent), m_game_list_model(game_list_model)
{
  setWindowTitle(tr("NetPlay Setup"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateMainLayout();

  bool use_index = Config::Get(Config::NETPLAY_USE_INDEX);
  std::string index_region = Config::Get(Config::NETPLAY_INDEX_REGION);
  std::string index_name = Config::LobbyNameVector(Config::Get(Config::NETPLAY_INDEX_NAME))[0];
  std::string index_password = Config::Get(Config::NETPLAY_INDEX_PASSWORD);
  std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  int connect_port = Config::Get(Config::NETPLAY_CONNECT_PORT);
  int host_port = Config::Get(Config::NETPLAY_HOST_PORT);
  int host_listen_port = Config::Get(Config::NETPLAY_LISTEN_PORT);
  bool enable_chunked_upload_limit = Config::Get(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT);
  u32 chunked_upload_limit = Config::Get(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT);
#ifdef USE_UPNP
  bool use_upnp = Config::Get(Config::NETPLAY_USE_UPNP);

  m_host_upnp->setChecked(use_upnp);
#endif

  m_connection_type->setCurrentIndex(traversal_choice == "direct" ? 1 : 0);
  m_connect_port_box->setValue(connect_port);
  m_host_port_box->setValue(host_port);

  m_host_force_port_box->setValue(host_listen_port);
  m_host_force_port_box->setEnabled(false);

  m_host_server_browser->setChecked(use_index);

  m_host_server_region->setEnabled(use_index);
  m_host_server_region->setCurrentIndex(
      m_host_server_region->findData(QString::fromStdString(index_region)));

  m_host_server_name->setEnabled(use_index);
  m_host_server_name->setText(QString::fromStdString(index_name));

  m_host_game_mode->setEnabled(true);

  m_host_server_password->setEnabled(use_index);
  m_host_server_password->setText(QString::fromStdString(index_password));

  m_host_chunked_upload_limit_check->setChecked(enable_chunked_upload_limit);
  m_host_chunked_upload_limit_box->setValue(chunked_upload_limit);
  m_host_chunked_upload_limit_box->setEnabled(enable_chunked_upload_limit);

  // Browser Stuff
  const auto& settings = Settings::Instance().GetQSettings();

  const QByteArray geometry =
      settings.value(QStringLiteral("netplaybrowser/geometry")).toByteArray();
  if (!geometry.isEmpty())
    restoreGeometry(geometry);

  const QString region = settings.value(QStringLiteral("netplaybrowser/region")).toString();
  const bool valid_region = m_region_combo->findText(region) != -1;
  if (valid_region)
    m_region_combo->setCurrentText(region);

  m_edit_name->setText(settings.value(QStringLiteral("netplaybrowser/name")).toString());

  const QString visibility = settings.value(QStringLiteral("netplaybrowser/visibility")).toString();
  if (visibility == QStringLiteral("public"))
    m_radio_public->setChecked(true);
  else if (visibility == QStringLiteral("private"))
    m_radio_private->setChecked(true);

  m_check_hide_ingame->setChecked(true);


  OnConnectionTypeChanged(m_connection_type->currentIndex());

  ConnectWidgets();

  m_refresh_run.Set(true);
  m_refresh_thread = std::thread([this] { RefreshLoopBrowser(); });

  UpdateListBrowser();
  RefreshBrowser();
}

void NetPlaySetupDialog::CreateMainLayout()
{
  m_main_layout = new QGridLayout;
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Cancel);
  m_connection_type = new QComboBox;
  m_reset_traversal_button = new NonDefaultQPushButton(tr("Reset Traversal Settings"));
  m_latency_test = new QPushButton(tr("Internet Test"));
  m_tab_widget = new QTabWidget;
  m_account_name = new QLabel;
  m_account_name->setTextFormat(Qt::RichText);
  m_account_name->setText(tr("<h1><u>An unklnown error has occurred. Please close this window</u></h1>"));
  m_account_box = new QGroupBox(tr("Current User"));


  // Connection widget
  auto* connection_widget = new QWidget;
  auto* connection_layout = new QGridLayout;

  m_ip_label = new QLabel;
  m_ip_edit = new QLineEdit;
  m_ip_edit->setPlaceholderText(tr("To join a private lobby, paste the room code here"));
  m_connect_port_label = new QLabel(tr("Port:"));
  m_connect_port_box = new QSpinBox;
  m_connect_button = new NonDefaultQPushButton(tr("Connect"));

  m_connect_port_box->setMaximum(65535);

  connection_layout->addWidget(m_ip_label, 0, 0);
  connection_layout->addWidget(m_ip_edit, 0, 1);
  connection_layout->addWidget(m_connect_port_label, 0, 2);
  connection_layout->addWidget(m_connect_port_box, 0, 3);
  connection_layout->addWidget(m_connect_button, 0, 4, Qt::AlignRight);
  connection_widget->setLayout(connection_layout);


  // NetPlay Browser
  auto* browser_widget = new QWidget;
  auto* layout = new QVBoxLayout;

  m_table_widget = new QTableWidget;
  m_table_widget->setTabKeyNavigation(false);

  m_table_widget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table_widget->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table_widget->setWordWrap(false);

  m_region_combo = new QComboBox;

  m_region_combo->addItem(tr("Any Region"));

  for (const auto& region : NetPlayIndex::GetRegions())
  {
    m_region_combo->addItem(
        tr("%1 (%2)").arg(tr(region.second.c_str())).arg(QString::fromStdString(region.first)),
        QString::fromStdString(region.first));
  }

  m_region_combo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

  m_status_label = new QLabel;
  m_online_count = new QLabel;
  m_b_button_box = new QDialogButtonBox;
  m_button_refresh = new QPushButton(tr("Refresh"));
  m_button_refresh->setAutoDefault(false);
  m_edit_name = new QLineEdit;
  m_edit_name->setPlaceholderText(tr("Lobby Name"));
  m_check_hide_ingame = new QCheckBox(tr("Hide In-Game Sessions"));

  m_radio_all = new QRadioButton(tr("Private and Public"));
  m_radio_private = new QRadioButton(tr("Private"));
  m_radio_public = new QRadioButton(tr("Public"));

  m_radio_all->setChecked(true);

  auto* filter_box = new QGroupBox(tr("Filters"));
  auto* filter_layout = new QGridLayout;

  filter_layout->addWidget(m_check_hide_ingame, 0, 0);
  filter_layout->addWidget(m_radio_all, 0, 1);
  filter_layout->addWidget(m_radio_public, 0, 2);
  filter_layout->addWidget(m_radio_private, 0, 3);
  filter_layout->addWidget(m_region_combo, 1, 0, 1, 3);
  filter_layout->addWidget(m_edit_name, 1, 1, 1, -1);
  filter_layout->addItem(new QSpacerItem(3, 1, QSizePolicy::Expanding), 3, 4);
  filter_box->setLayout(filter_layout);

  auto* status = new QWidget;
  auto* status_layout = new QVBoxLayout;
  status_layout->addWidget(m_status_label);
  status_layout->addWidget(m_b_button_box);
  status->setLayout(status_layout);

  auto* filter_status = new QWidget;
  auto* filter_status_layout = new QHBoxLayout;
  filter_status_layout->addWidget(status);
  filter_status_layout->addWidget(filter_box);
  filter_status->setLayout(filter_status_layout);

  layout->addWidget(connection_widget);
  layout->addWidget(m_online_count);
  layout->addWidget(m_table_widget);
  layout->addWidget(filter_status);
  //layout->addWidget(filter_box);
  //layout->addWidget(m_status_label);
  //layout->addWidget(m_b_button_box);

  m_b_button_box->addButton(m_button_refresh, QDialogButtonBox::ResetRole);

  browser_widget->setLayout(layout);


  // Host widget
  auto* host_widget = new QWidget;
  auto* host_layout = new QGridLayout;
  m_host_port_label = new QLabel(tr("Port:"));
  m_host_port_box = new QSpinBox;
  m_host_force_port_check = new QCheckBox(tr("Force Listen Port:"));
  m_host_force_port_box = new QSpinBox;
  m_host_chunked_upload_limit_check = new QCheckBox(tr("Limit Chunked Upload Speed:"));
  m_host_chunked_upload_limit_box = new QSpinBox;
  m_host_server_browser = new QCheckBox(tr("Public Match"));
  m_host_server_name = new QLineEdit;
  m_host_server_password = new QLineEdit;
  m_host_server_region = new QComboBox;
  m_host_server_region->setPlaceholderText(tr("Select a Region (required)"));
  m_game_mode_description = new QTextEdit;

  const auto line_height = QFontMetrics(font()).lineSpacing();
  m_game_mode_description = new QTextEdit;
  m_game_mode_description->setReadOnly(true);
  m_game_mode_description->setFixedHeight(line_height * 10);
  m_game_mode_description->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  
  auto* description_widget = new QLabel(tr(
    "Select the appropriate game (Mario Superstar Baseball) and choose a Game Mode (optional).<br/>"
    "Game Modes are pre-made ways to play the game. Any necessary mods and/or game changes are<br/>"
    "automatically applied with a Game Mode. To play normal netplay, select \"No Game Mode\".<br/><br/>"
    "Head to the <a href=\"https://www.projectrio.online/gamemode/\">Project Rio Website</a> "
    "to learn more about Game Modes!<br/><br/>"
    ));
  description_widget->setTextFormat(Qt::RichText);
  description_widget->setTextInteractionFlags(Qt::TextBrowserInteraction);
  description_widget->setOpenExternalLinks(true);

  m_host_game_mode = new QComboBox;
  m_host_game_mode->setToolTip(
      tr("Choose which game mode you would like to play with. This will appear and be visible to other players in the lobby browser."
      ));

  m_host_game_mode->setCurrentIndex(0);

#ifdef USE_UPNP
  m_host_upnp = new QCheckBox(tr("Forward port (UPnP)"));
#endif
  m_host_games = new QComboBox;
  m_host_button = new NonDefaultQPushButton(tr("Host"));

  m_host_port_box->setMaximum(65535);
  m_host_force_port_box->setMaximum(65535);
  m_host_chunked_upload_limit_box->setRange(1, 1000000);
  m_host_chunked_upload_limit_box->setSingleStep(100);
  m_host_chunked_upload_limit_box->setSuffix(QStringLiteral(" kbps"));

  m_host_chunked_upload_limit_check->setToolTip(tr(
      "This will limit the speed of chunked uploading per client, which is used for save sync."));

  m_host_server_name->setToolTip(tr("Name of your session shown in the server browser"));
  m_host_server_name->setPlaceholderText(tr("Lobby Name"));
  m_host_server_password->setToolTip(tr("Password for joining your game (leave empty for none)"));
  m_host_server_password->setPlaceholderText(tr("Password (optional)"));

  for (const auto& region : NetPlayIndex::GetRegions())
  {
    m_host_server_region->addItem(
        tr("%1 (%2)").arg(tr(region.second.c_str())).arg(QString::fromStdString(region.first)),
        QString::fromStdString(region.first));
  }

  auto* game_config = new QGroupBox(tr("Game Options"));
  auto* game_config_layout = new QGridLayout;
  game_config_layout->addWidget(new QLabel(tr("Game: ")), 0, 0);
  game_config_layout->addWidget(m_host_games, 0, 1, 1, -1);
  game_config_layout->addWidget(new QLabel(tr("Game Mode: ")), 1, 0);
  game_config_layout->addWidget(m_host_game_mode, 1, 1, 1, -1);
  game_config_layout->addWidget(m_host_server_browser, 2, 1, 1, -1);
  game_config_layout->setAlignment(Qt::AlignLeft);
  game_config->setLayout(game_config_layout);

  auto* match_config = new QGroupBox(tr("Lobby Options"));
  auto* match_config_layout = new QVBoxLayout;
  match_config_layout->addWidget(m_host_server_name);
  match_config_layout->addWidget(m_host_server_password);
  match_config_layout->addWidget(m_host_server_region);
  //match_config_layout->addWidget(m_host_server_browser);
  match_config->setLayout(match_config_layout);

  auto* lobby_config = new QGroupBox(tr("Advanced Lobby Options"));
  auto* lobby_config_layout = new QGridLayout;

  lobby_config_layout->addWidget(m_host_port_label, 3, 0);
  lobby_config_layout->addWidget(m_host_port_box, 3, 1);
#ifdef USE_UPNP
  lobby_config_layout->addWidget(m_host_upnp, 3, 2);
#endif
  lobby_config_layout->addWidget(m_host_force_port_check, 4, 0);
  lobby_config_layout->addWidget(m_host_force_port_box, 4, 1, Qt::AlignLeft);
  lobby_config_layout->addWidget(m_host_chunked_upload_limit_check, 5, 0);
  lobby_config_layout->addWidget(m_host_chunked_upload_limit_box, 5, 1, Qt::AlignLeft);
  lobby_config->setLayout(lobby_config_layout);

  host_layout->addWidget(game_config, 0, 0);
  host_layout->addWidget(match_config, 0, 1);
  host_layout->addWidget(m_game_mode_description, 1, 0, 1, -1);
  host_layout->addWidget(description_widget, 2, 0, 1, -1);
  host_layout->addWidget(lobby_config, 3, 0, 1, -1, Qt::AlignBottom);
  host_layout->addWidget(m_host_button, 4, 0, 1, -1, Qt::AlignRight);
  host_widget->setLayout(host_layout);

  m_connection_type->addItem(tr("Traversal Server"));
  m_connection_type->addItem(tr("Direct Connection"));

  auto* info_layout = new QGridLayout;
  info_layout->addWidget(m_account_name, 0, 0, 1, -1, Qt::AlignHCenter);
  m_account_box->setLayout(info_layout);


  m_main_layout->addWidget(m_account_box, 0, 0, 1, -1);
  m_main_layout->addWidget(m_tab_widget, 1, 0, 1, -1);
  m_main_layout->addWidget(m_latency_test, 2, 0);
  m_main_layout->addWidget(m_connection_type, 2, 1);
  m_main_layout->addWidget(m_reset_traversal_button, 2, 3);
  m_main_layout->addWidget(m_button_box, 2, 4, 1, -1);

  // Tabs
  //m_tab_widget->addTab(connection_widget, tr("Join Private Lobby"));
  m_tab_widget->addTab(browser_widget, tr("Join Match"));
  m_tab_widget->addTab(host_widget, tr("Host Match"));

  setLayout(m_main_layout);
}

NetPlaySetupDialog::~NetPlaySetupDialog()
{
  m_refresh_run.Set(false);
  m_refresh_event.Set();
  if (m_refresh_thread.joinable())
    m_refresh_thread.join();

  SaveSettings();
}

void NetPlaySetupDialog::ConnectWidgets()
{
  connect(m_connection_type, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &NetPlaySetupDialog::OnConnectionTypeChanged);

  // Connect widget
  connect(m_ip_edit, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_connect_port_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &NetPlaySetupDialog::SaveSettings);
  // Host widget
  connect(m_host_port_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &NetPlaySetupDialog::SaveSettings);
  connect(m_host_games, qOverload<int>(&QComboBox::currentIndexChanged), [this](int index) {
    Settings::GetQSettings().setValue(QStringLiteral("netplay/hostgame"),
                                      m_host_games->currentText());
  });
  connect(m_host_game_mode, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &NetPlaySetupDialog::UpdateGameModeDescription);

  // refresh browser on tab changed
  connect(m_tab_widget, &QTabWidget::currentChanged, this, &NetPlaySetupDialog::RefreshBrowser);

  //connect(m_host_games, &QListWidget::itemDoubleClicked, this, &NetPlaySetupDialog::accept);

  connect(m_host_force_port_check, &QCheckBox::toggled,
          [this](bool value) { m_host_force_port_box->setEnabled(value); });
  connect(m_host_chunked_upload_limit_check, &QCheckBox::toggled, this, [this](bool value) {
    m_host_chunked_upload_limit_box->setEnabled(value);
    SaveSettings();
  });
  connect(m_host_chunked_upload_limit_box, qOverload<int>(&QSpinBox::valueChanged), this,
          &NetPlaySetupDialog::SaveSettings);

  connect(m_host_server_browser, &QCheckBox::toggled, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_host_server_name, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_host_server_password, &QLineEdit::textChanged, this, &NetPlaySetupDialog::SaveSettings);
  connect(m_host_server_region,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &NetPlaySetupDialog::SaveSettings);

#ifdef USE_UPNP
  connect(m_host_upnp, &QCheckBox::stateChanged, this, &NetPlaySetupDialog::SaveSettings);
#endif

  connect(m_connect_button, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_host_button, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_reset_traversal_button, &QPushButton::clicked, this,
          &NetPlaySetupDialog::ResetTraversalHost);
  connect(m_latency_test, &QPushButton::clicked, this, &NetPlaySetupDialog::OpenInternetTest);
  connect(m_host_server_browser, &QCheckBox::toggled, this, [this](bool value) {
    m_host_server_region->setEnabled(value);
    m_host_server_name->setEnabled(value);
    m_host_server_password->setEnabled(value);
  });


  // Browser Stuff
  connect(m_region_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &NetPlaySetupDialog::RefreshBrowser);

  connect(m_button_refresh, &QPushButton::clicked, this, &NetPlaySetupDialog::RefreshBrowser);

  connect(m_radio_all, &QRadioButton::toggled, this, &NetPlaySetupDialog::RefreshBrowser);
  connect(m_radio_private, &QRadioButton::toggled, this, &NetPlaySetupDialog::RefreshBrowser);
  connect(m_check_hide_ingame, &QRadioButton::toggled, this, &NetPlaySetupDialog::RefreshBrowser);

  connect(m_edit_name, &QLineEdit::textChanged, this, &NetPlaySetupDialog::RefreshBrowser);

  connect(m_table_widget, &QTableWidget::itemDoubleClicked, this,
          &NetPlaySetupDialog::acceptBrowser);

  connect(this, &NetPlaySetupDialog::UpdateStatusRequestedBrowser, this,
          &NetPlaySetupDialog::OnUpdateStatusRequestedBrowser, Qt::QueuedConnection);
  connect(this, &NetPlaySetupDialog::UpdateListRequestedBrowser, this,
          &NetPlaySetupDialog::OnUpdateListRequestedBrowser,
          Qt::QueuedConnection);
}

void NetPlaySetupDialog::SaveSettings()
{
  Config::ConfigChangeCallbackGuard config_guard;

  Config::SetBaseOrCurrent(m_connection_type->currentIndex() == 1 ? Config::NETPLAY_ADDRESS :
                                                                    Config::NETPLAY_HOST_CODE,
                           m_ip_edit->text().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_CONNECT_PORT,
                           static_cast<u16>(m_connect_port_box->value()));
  Config::SetBaseOrCurrent(Config::NETPLAY_HOST_PORT, static_cast<u16>(m_host_port_box->value()));
#ifdef USE_UPNP
  Config::SetBaseOrCurrent(Config::NETPLAY_USE_UPNP, m_host_upnp->isChecked());
#endif

  if (m_host_force_port_check->isChecked())
    Config::SetBaseOrCurrent(Config::NETPLAY_LISTEN_PORT,
                             static_cast<u16>(m_host_force_port_box->value()));

  Config::SetBaseOrCurrent(Config::NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT,
                           m_host_chunked_upload_limit_check->isChecked());
  Config::SetBaseOrCurrent(Config::NETPLAY_CHUNKED_UPLOAD_LIMIT,
                           m_host_chunked_upload_limit_box->value());

  Config::SetBaseOrCurrent(Config::NETPLAY_USE_INDEX, m_host_server_browser->isChecked());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_REGION,
                           m_host_server_region->currentData().toString().toStdString());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_NAME, LobbyNameString());
  Config::SetBaseOrCurrent(Config::NETPLAY_INDEX_PASSWORD,
                           m_host_server_password->text().toStdString());

  // Browser Stuff
  auto& settings = Settings::Instance().GetQSettings();

  settings.setValue(QStringLiteral("netplaybrowser/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("netplaybrowser/region"), m_region_combo->currentText());
  settings.setValue(QStringLiteral("netplaybrowser/name"), m_edit_name->text());

  QString visibility(QStringLiteral("all"));
  if (m_radio_public->isChecked())
    visibility = QStringLiteral("public");
  else if (m_radio_private->isChecked())
    visibility = QStringLiteral("private");
  settings.setValue(QStringLiteral("netplaybrowser/visibility"), visibility);

  settings.setValue(QStringLiteral("netplaybrowser/hide_incompatible"), true);
  settings.setValue(QStringLiteral("netplaybrowser/hide_ingame"), m_check_hide_ingame->isChecked());
}

void NetPlaySetupDialog::OnConnectionTypeChanged(int index)
{
  m_connect_port_box->setHidden(index != 1);
  m_connect_port_label->setHidden(index != 1);

  m_host_port_label->setHidden(index != 1);
  m_host_port_box->setHidden(index != 1);
#ifdef USE_UPNP
  m_host_upnp->setHidden(index != 1);
#endif
  m_host_force_port_check->setHidden(index == 1);
  m_host_force_port_box->setHidden(index == 1);

  m_reset_traversal_button->setHidden(index == 1);

  std::string address =
      index == 1 ? Config::Get(Config::NETPLAY_ADDRESS) : Config::Get(Config::NETPLAY_HOST_CODE);

  m_ip_label->setText(index == 1 ? tr("IP Address:") : tr("Host Code:"));
  m_ip_edit->setText(QString::fromStdString(address));

  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_CHOICE,
                           std::string(index == 1 ? "direct" : "traversal"));
}

void NetPlaySetupDialog::UpdateGameModeDescription()
{
  std::optional<Tag::TagSet> selected_tagset = GetTagSet();

  m_game_mode_description->clear();
  if (!selected_tagset.has_value())
  {
    m_game_mode_description->append(tr("No Game Mode Selected."));
    return;
  }

  std::vector<std::string> tags = selected_tagset.value().tag_names_vector();
  std::string tags_string = "\nRules:\n";
  for (auto& tag : tags)
  {
    if (tag != selected_tagset.value().name)
      tags_string.append("- " + tag + "\n");
  }
  tags_string.pop_back();  // remove final delimiter

  m_game_mode_description->append(QString::fromStdString(selected_tagset.value().description()));
  m_game_mode_description->append(QString::fromStdString(tags_string));

  QScrollBar* scrollBar = m_game_mode_description->verticalScrollBar();
  scrollBar->setValue(scrollBar->minimum());  // set scroll bar to the top
}

void NetPlaySetupDialog::show()
{
  m_host_server_browser->setChecked(true);
  RefreshBrowser();

  PopulateGameList();
  QDialog::show();
}

void NetPlaySetupDialog::accept()
{
  SaveSettings();
  if (m_tab_widget->currentIndex() == 0)
  {
    emit Join();
  }
  else
  {
    if (m_host_games->count() == 0)
    {
      ModalMessageBox::critical(this, tr("Error"), tr("You must select a game to host!"));
      return;
    }

    if (m_host_server_browser->isChecked() && m_host_server_name->text().isEmpty())
    {
      ModalMessageBox::critical(this, tr("Error"), tr("You must provide a name for your session!"));
      return;
    }

    if (m_host_server_browser->isChecked() &&
        m_host_server_region->currentData().toString().isEmpty())
    {
      ModalMessageBox::critical(this, tr("Error"),
                                tr("You must provide a region for your session!"));
      return;
    }

    emit Host(host_games_map.at(m_host_games->currentIndex()));
  }
}

std::map<int, Tag::TagSet>& NetPlaySetupDialog::assignOnlineAccount(LocalPlayers::LocalPlayers::Player online_player)
{
  m_active_account.SetUserInfo(online_player);
  std::string label = "<h1>" + m_active_account.GetUsername() + "</h1>";
  m_account_name->setText(QString::fromStdString(label));

  // Here i'm setting the lobby name if it's empty to make
  // NetPlay sessions start more easily for first time players
  if (m_host_server_name->text().isEmpty())
  {
    std::string nickname = m_active_account.username;
    nickname += "'s Lobby";
    m_host_server_name->setText(QString::fromStdString(nickname));
  }

  user_tagsets = Tag::getAvailableTagSets(m_http, m_active_account.userid);

  // add game modes
  tagset_map.clear();
  m_host_game_mode->clear();
  int combobox_index = 0;
  m_host_game_mode->addItem(QString::fromStdString("No Game Mode"));
  tagset_map.insert(std::pair<int, std::optional<Tag::TagSet>>(combobox_index++, std::nullopt));
  for (auto& tagset : user_tagsets)
  {
    m_host_game_mode->addItem(QString::fromStdString(tagset.second.name));
    tagset_map.insert(std::pair<int, std::optional<Tag::TagSet>>(combobox_index++, tagset.second));
  }
  return user_tagsets;
}

std::optional<Tag::TagSet> NetPlaySetupDialog::GetTagSet()
{
  return tagset_map[m_host_game_mode->currentIndex()];
}

void NetPlaySetupDialog::PopulateGameList()
{
  QSignalBlocker blocker(m_host_games);

  m_host_games->clear();
  host_games_map.clear();
  int itemIndex = 0;
  for (int i = 0; i < m_game_list_model.rowCount(QModelIndex()); i++)
  {
    std::shared_ptr<const UICommon::GameFile> game = m_game_list_model.GetGameFile(i);
    if (m_game_list_model.ShouldDisplayGameListItem(i))
    {
      auto item = QString::fromStdString(m_game_list_model.GetNetPlayName(*game));
      m_host_games->addItem(item);
      host_games_map.insert(std::pair<int, UICommon::GameFile>(itemIndex++, *(game.get())));
    }
  }

  const QString selected_game =
      Settings::GetQSettings().value(QStringLiteral("netplay/hostgame"), QString{}).toString();
  const int find_list = m_host_games->findText(selected_game, Qt::MatchFlag::MatchExactly);

  m_host_games->setCurrentIndex(find_list);
}

void NetPlaySetupDialog::ResetTraversalHost()
{
  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_SERVER,
                           Config::NETPLAY_TRAVERSAL_SERVER.GetDefaultValue());
  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_PORT,
                           Config::NETPLAY_TRAVERSAL_PORT.GetDefaultValue());

  ModalMessageBox::information(
      this, tr("Reset Traversal Server"),
      tr("Reset Traversal Server to %1:%2")
          .arg(QString::fromStdString(Config::NETPLAY_TRAVERSAL_SERVER.GetDefaultValue()),
               QString::number(Config::NETPLAY_TRAVERSAL_PORT.GetDefaultValue())));
}

void NetPlaySetupDialog::OpenInternetTest()
{
  QString url = QStringLiteral("https://testmy.net/latency?testALL=1");
  QDesktopServices::openUrl(QUrl(url));
}


void NetPlaySetupDialog::RefreshBrowser()
{
  std::map<std::string, std::string> filters;

  if (!m_edit_name->text().isEmpty())
    filters["name"] = m_edit_name->text().toStdString();

  if (true)
    filters["version"] = Common::GetRioRevStr();

  if (!m_radio_all->isChecked())
    filters["password"] = std::to_string(m_radio_private->isChecked());

  if (m_region_combo->currentIndex() != 0)
    filters["region"] = m_region_combo->currentData().toString().toStdString();

  if (m_check_hide_ingame->isChecked())
    filters["in_game"] = "0";

  std::unique_lock<std::mutex> lock(m_refresh_filters_mutex);
  m_refresh_filters = std::move(filters);
  m_refresh_event.Set();
  SaveSettings();
}

void NetPlaySetupDialog::RefreshLoopBrowser()
{
  while (m_refresh_run.IsSet())
  {
    m_refresh_event.Wait();

    std::unique_lock<std::mutex> lock(m_refresh_filters_mutex);
    if (m_refresh_filters)
    {
      auto filters = std::move(*m_refresh_filters);
      m_refresh_filters.reset();

      lock.unlock();

      emit UpdateStatusRequestedBrowser(tr("Refreshing..."));

      NetPlayIndex client;

      auto entries = client.List(filters);

      if (entries)
      {
        emit UpdateListRequestedBrowser(std::move(*entries));
      }
      else
      {
        emit UpdateStatusRequestedBrowser(tr("Error obtaining session list: %1")
                                       .arg(QString::fromStdString(client.GetLastError())));
      }
    }
  }
}

void NetPlaySetupDialog::UpdateListBrowser()
{
  const int session_count = static_cast<int>(m_sessions.size());

  m_table_widget->clear();
  m_table_widget->setColumnCount(7);
  m_table_widget->setHorizontalHeaderLabels({tr("Region"), tr("Name"), tr("Game Mode"),
                                             tr("In Game"), tr("Password?"), tr("Players"),
                                             tr("Version")});

  auto* hor_header = m_table_widget->horizontalHeader();

  hor_header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(1, QHeaderView::Stretch);
  //hor_header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  //hor_header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  hor_header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  hor_header->setHighlightSections(false);

  m_table_widget->setRowCount(session_count);

  for (int i = 0; i < session_count; i++)
  {
    const auto& entry = m_sessions[i];

    std::vector<std::string> game_tags = Config::LobbyNameVector(entry.name);

    auto* region = new QTableWidgetItem(QString::fromStdString(entry.region));
    auto* name = new QTableWidgetItem(QString::fromStdString(game_tags[0]));
    auto* password = new QTableWidgetItem(entry.has_password ? tr("Yes") : tr("No"));
    auto* in_game = new QTableWidgetItem(entry.in_game ? tr("Yes") : tr("No"));
    auto* gamemode = new QTableWidgetItem(QString::fromStdString(game_tags[1]));
    auto* player_count = new QTableWidgetItem(QStringLiteral("%1").arg(entry.player_count));
    auto* version = new QTableWidgetItem(QString::fromStdString(entry.version));

    bool enabled = Common::GetRioRevStr() == entry.version;
    int tagset_id = stoi(game_tags[2]); // should make this safe in the future to catch when it can't convert to int cleanly
    if (user_tagsets.find(tagset_id) == user_tagsets.end())
      enabled = false;
    if (tagset_id == 0)
      enabled = true;

    for (const auto& item : {region, name, in_game, gamemode, password, player_count, version})
      item->setFlags(enabled ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags);

    m_table_widget->setItem(i, 0, region);
    m_table_widget->setItem(i, 1, name);
    m_table_widget->setItem(i, 2, gamemode);  // was game_id
    m_table_widget->setItem(i, 3, in_game);
    m_table_widget->setItem(i, 4, password);
    m_table_widget->setItem(i, 5, player_count);
    m_table_widget->setItem(i, 6, version);
  }

  m_status_label->setText(
      (session_count == 1 ? tr("%1 session found") : tr("%1 sessions found")).arg(session_count));

  m_online_count->setText((Config::ONLINE_COUNT == 1 ? tr("There is %1 player in a lobby") :
                                               tr("There are %1 players in a lobby"))
                              .arg(Config::ONLINE_COUNT));
}

void NetPlaySetupDialog::OnSelectionChangedBrowser()
{
  return;
}

void NetPlaySetupDialog::OnUpdateStatusRequestedBrowser(const QString& status)
{
  m_status_label->setText(status);
}

void NetPlaySetupDialog::OnUpdateListRequestedBrowser(std::vector<NetPlaySession> sessions)
{
  m_sessions = std::move(sessions);
  UpdateListBrowser();
}

void NetPlaySetupDialog::acceptBrowser()
{
  if (m_table_widget->selectedItems().isEmpty())
    return;

  const int index = m_table_widget->selectedItems()[0]->row();

  NetPlaySession& session = m_sessions[index];

  std::string server_id = session.server_id;

  if (m_sessions[index].has_password)
  {
    auto* dialog = new QInputDialog(this);

    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog->setWindowTitle(tr("Enter password"));
    dialog->setLabelText(tr("This session requires a password:"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setTextEchoMode(QLineEdit::Password);

    if (dialog->exec() != QDialog::Accepted)
      return;

    const std::string password = dialog->textValue().toStdString();

    auto decrypted_id = session.DecryptID(password);

    if (!decrypted_id)
    {
      ModalMessageBox::warning(this, tr("Error"), tr("Invalid password provided."));
      return;
    }

    server_id = decrypted_id.value();
  }

  QDialog::accept();

  Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_CHOICE, session.method);

  Config::SetBaseOrCurrent(Config::NETPLAY_CONNECT_PORT, session.port);

  if (session.method == "traversal")
    Config::SetBaseOrCurrent(Config::NETPLAY_HOST_CODE, server_id);
  else
    Config::SetBaseOrCurrent(Config::NETPLAY_ADDRESS, server_id);

  emit JoinBrowser();
}

std::string NetPlaySetupDialog::LobbyNameString()
{
  std::string lobby_string = m_host_server_name->text().toStdString();
  std::string delimiter = "%%";
  lobby_string += delimiter;
  lobby_string += m_host_game_mode->currentText().toStdString();
  lobby_string += delimiter;
  if (tagset_map[m_host_game_mode->currentIndex()].has_value())
  {
    lobby_string += std::to_string(tagset_map[m_host_game_mode->currentIndex()].value().id);
  } else {
    lobby_string += "0";
  }
  return lobby_string;
}
