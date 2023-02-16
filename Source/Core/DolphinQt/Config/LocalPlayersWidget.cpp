// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <iosfwd>
#include "DolphinQt/Config/LocalPlayersWidget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QListWidget>

#include <optional>
#include <utility>
#include <vector>

#include "Common/CommonPaths.h"

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Core/LocalPlayersConfig.h"
#include "DolphinQt/Config/AddLocalPlayers.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"

#include "DolphinQt/Settings.h"

LocalPlayersWidget::LocalPlayersWidget(QWidget* parent) : QWidget(parent)
{
  IniFile local_players_ini;
  LocalPlayers::LocalPlayers player;
  local_players_ini.Load(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));
  m_local_players = player.GetPlayers(local_players_ini);
  LocalPlayers::LoadLocalPorts();

  CreateLayout();
  UpdatePlayers();
  ConnectWidgets();
}

void LocalPlayersWidget::CreateLayout()
{
  m_player_layout = new QGridLayout();
  m_player_layout->setVerticalSpacing(7);
  m_player_layout->setColumnStretch(100, 100);
  m_player_list = new QListWidget;

  auto* gc_label1 = new QLabel(tr("Player 1"));
  auto* gc_box1 = m_player_list_1 = new QComboBox();

  auto* gc_label2 = new QLabel(tr("Player 2"));
  auto gc_box2 = m_player_list_2 = new QComboBox();

  auto* gc_label3 = new QLabel(tr("Player 3"));
  auto gc_box3 = m_player_list_3 = new QComboBox();
  
  auto* gc_label4 = new QLabel(tr("Player 4"));
  auto gc_box4 = m_player_list_4 = new QComboBox();

  m_add_button = new QPushButton(tr("Add Player"));
  m_add_button->setToolTip(
      (tr("Local Players System:\n\nAdd players using the \"Add Player\" button.\n"
          "The Local Players are used for recording stats locally.\nThese players are not used "
          "for online games.\n"
          "It is only used for keeping track of stats for offline games.\n\n"
          "Make sure to obtain the EXACT Username and UserID of the player you wish to add.\n"
          "The Local Players can be changed while a game session is running.\n"
          "To edit/remove players, open \"LocalPlayers.ini\" which is found\n in the Dolphin "
          "Emulator Config folder"
          "and edit the file in a notepad or any text editor.\n")));

  m_remove_button = new QPushButton(tr("Remove Player"));
 
  m_player_layout->addWidget(gc_label1, 0, 0);
  m_player_layout->addWidget(gc_box1, 0, 1);
  m_player_layout->addWidget(gc_label2, 1, 0);
  m_player_layout->addWidget(gc_box2, 1, 1);
  m_player_layout->addWidget(gc_label3, 2, 0);
  m_player_layout->addWidget(gc_box3, 2, 1);
  m_player_layout->addWidget(gc_label4, 3, 0);
  m_player_layout->addWidget(gc_box4, 3, 1);

  auto* vlayout = new QVBoxLayout;
  vlayout->addLayout(m_player_layout);
  vlayout->addWidget(m_add_button);

  auto* vlayout2 = new QVBoxLayout;
  vlayout2->addWidget(m_player_list);
  vlayout2->addWidget(m_remove_button);

  auto* layout = new QHBoxLayout;
  layout->addLayout(vlayout, 4);
  layout->addSpacing(5);
  layout->addLayout(vlayout2, 6);
  setLayout(layout);
}

void LocalPlayersWidget::UpdatePlayers()
{
  m_player_list->clear();

  // List available players in LocalPlayers.ini
  for (size_t i = 0; i < m_local_players.size(); i++)
  {
    const auto& player = m_local_players[i];

    auto username = QString::fromStdString(player.username);

    // In the future, i should add in a feature that if a player is selected on another port, they
    // won't appear on the dropdown some conditional that checks the other ports before adding the
    // item
    m_player_list_1->addItem(username);
    m_player_list_2->addItem(username);
    m_player_list_3->addItem(username);
    m_player_list_4->addItem(username);
    if (i > 0)
      m_player_list->addItem(username);
  }

  SetPlayers();
}

void LocalPlayersWidget::SetPlayers()
{
  LocalPlayers::SaveLocalPorts();

  for (int i = 0; i < m_local_players.size(); i++)
  {
    if (LocalPlayers::m_local_player_1.GetUserID() == m_local_players[i].GetUserID())
      m_player_list_1->setCurrentIndex(i);
    if (LocalPlayers::m_local_player_2.GetUserID() == m_local_players[i].GetUserID())
      m_player_list_2->setCurrentIndex(i);
    if (LocalPlayers::m_local_player_3.GetUserID() == m_local_players[i].GetUserID())
      m_player_list_3->setCurrentIndex(i);
    if (LocalPlayers::m_local_player_4.GetUserID() == m_local_players[i].GetUserID())
      m_player_list_4->setCurrentIndex(i);
  }
}

void LocalPlayersWidget::OnRemovePlayers()
{
  if (m_player_list->currentItem() == nullptr)
    return;

  const int index = m_player_list->currentRow() + 1;

  m_local_players.erase(m_local_players.begin() + index);

  m_player_list_1->removeItem(index);
  m_player_list_2->removeItem(index);
  m_player_list_3->removeItem(index);
  m_player_list_4->removeItem(index);

  m_player_list->clear();
  for (size_t i = 1; i < m_local_players.size(); i++)
  {
    const auto& player = m_local_players[i];
    auto username = QString::fromStdString(player.username);
    m_player_list->addItem(username);
  }

  SetPlayers();
  SavePlayers();
}

void LocalPlayersWidget::OnAddPlayers()
{
  LocalPlayers::LocalPlayers::Player name;

  AddLocalPlayersEditor ed(this);
  ed.SetPlayer(&name);
  if (ed.exec() == QDialog::Rejected)
    return;

  m_local_players.push_back(std::move(name));
  SavePlayers();
  AddPlayerToList();
}

void LocalPlayersWidget::AddPlayerToList()
{
  auto playerAdd = m_local_players[m_local_players.size() - 1]; // last player in list
  auto username = QString::fromStdString(playerAdd.username);

  m_player_list_1->addItem(username);
  m_player_list_2->addItem(username);
  m_player_list_3->addItem(username);
  m_player_list_4->addItem(username);
  m_player_list->addItem(username);
}

void LocalPlayersWidget::SavePlayers()
{
  const auto ini_path = std::string(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));

  IniFile local_players_path;
  local_players_path.Load(ini_path);
  LocalPlayers::SavePlayers(local_players_path, m_local_players);
  local_players_path.Save(ini_path);
}

void LocalPlayersWidget::SetPlayerOne(const LocalPlayers::LocalPlayers::Player& local_player_1)
{
  LocalPlayers::m_local_player_1.SetUserInfo(local_player_1);
  LocalPlayers::SaveLocalPorts();
}

void LocalPlayersWidget::SetPlayerTwo(const LocalPlayers::LocalPlayers::Player& local_player_2)
{
  LocalPlayers::m_local_player_2.SetUserInfo(local_player_2);
  LocalPlayers::SaveLocalPorts();
}

void LocalPlayersWidget::SetPlayerThree(const LocalPlayers::LocalPlayers::Player& local_player_3)
{
  LocalPlayers::m_local_player_3.SetUserInfo(local_player_3);
  LocalPlayers::SaveLocalPorts();
}

void LocalPlayersWidget::SetPlayerFour(const LocalPlayers::LocalPlayers::Player& local_player_4)
{
  LocalPlayers::m_local_player_4.SetUserInfo(local_player_4);
  LocalPlayers::SaveLocalPorts();
}

void LocalPlayersWidget::ConnectWidgets()
{ 
  connect(m_player_list_1, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { SetPlayerOne(m_local_players[index]); });
  connect(m_player_list_2, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { SetPlayerTwo(m_local_players[index]); });
  connect(m_player_list_3, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { SetPlayerThree(m_local_players[index]); });
  connect(m_player_list_4, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { SetPlayerFour(m_local_players[index]); });


  connect(m_player_list_1, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);
  connect(m_player_list_2, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);
  connect(m_player_list_3, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);
  connect(m_player_list_4, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);


  connect(m_add_button, &QPushButton::clicked, this, &LocalPlayersWidget::OnAddPlayers);
  connect(m_remove_button, &QPushButton::clicked, this, &LocalPlayersWidget::OnRemovePlayers);
}
