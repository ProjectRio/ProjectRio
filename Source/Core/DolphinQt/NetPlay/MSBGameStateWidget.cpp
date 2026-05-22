// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/MSBGameStateWidget.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/MSB_Constants.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static QComboBox* MakeCharCombo()
{
  auto* combo = new QComboBox;
  combo->addItem(QStringLiteral("— Not Set —"), QVariant());
  for (const auto& [id, name] : MSB::CHAR_LIST)
    combo->addItem(QString::fromUtf8(name), QVariant(static_cast<int>(id)));
  return combo;
}

static QComboBox* MakePosCombo()
{
  auto* combo = new QComboBox;
  combo->addItem(QStringLiteral("— Not Set —"), QVariant());
  for (uint8_t p = 0; p < 9; ++p)
    combo->addItem(QString::fromUtf8(MSB::POSITION_NAMES[p]), QVariant(static_cast<int>(p)));
  return combo;
}

static QComboBox* MakeHandCombo()
{
  auto* combo = new QComboBox;
  combo->addItem(QStringLiteral("—"), QVariant());
  combo->addItem(QStringLiteral("R"), QVariant(0));
  combo->addItem(QStringLiteral("L"), QVariant(1));
  return combo;
}

static void SetComboByData(QComboBox* combo, std::optional<uint8_t> opt)
{
  if (opt.has_value())
  {
    const int idx = combo->findData(QVariant(static_cast<int>(opt.value())));
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
  }
  else
  {
    combo->setCurrentIndex(0);
  }
}

// ── Construction ──────────────────────────────────────────────────────────────

MSBGameStateWidget::MSBGameStateWidget(QWidget* parent)
    : QGroupBox(tr("Game State"), parent)
{
  CreateLayout();
  ConnectWidgets();
  UpdateEditability();
}

void MSBGameStateWidget::CreateLayout()
{
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(4, 8, 4, 4);
  outer->setSpacing(4);

  // ── Header row ──────────────────────────────────────────────────────────────
  m_enable_check = new QCheckBox(tr("Enable Fast Reset from State"));
  m_load_hud_btn = new QPushButton(tr("Load from HUD"));
  m_apply_btn    = new QPushButton(tr("Apply"));
  m_clear_btn    = new QPushButton(tr("Clear"));

  m_load_hud_btn->setToolTip(tr("Populate all fields from the latest HUD game state file."));
  m_apply_btn->setToolTip(tr("Send the current state to all clients and enable fast reset."));
  m_clear_btn->setToolTip(tr("Reset all fields and disable fast reset."));

  auto* header = new QHBoxLayout;
  header->addWidget(m_enable_check);
  header->addStretch();
  header->addWidget(m_load_hud_btn);
  header->addWidget(m_apply_btn);
  header->addWidget(m_clear_btn);
  outer->addLayout(header);

  auto* sep = new QFrame;
  sep->setFrameShape(QFrame::HLine);
  sep->setFrameShadow(QFrame::Sunken);
  outer->addWidget(sep);

  // ── Scroll area ─────────────────────────────────────────────────────────────
  auto* scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  outer->addWidget(scroll);

  m_scroll_content = new QWidget;
  scroll->setWidget(m_scroll_content);

  auto* content = new QVBoxLayout(m_scroll_content);
  content->setAlignment(Qt::AlignTop);
  content->setSpacing(6);

  CreatePreGameSection(content);
  CreateRosterSection(content);

  // ── In-Game placeholder ─────────────────────────────────────────────────────
  auto* ig_group  = new QGroupBox(tr("In-Game State"));
  auto* ig_layout = new QVBoxLayout(ig_group);
  auto* ig_label  = new QLabel(tr("In-game state editing coming in Step 3."));
  ig_label->setAlignment(Qt::AlignCenter);
  ig_label->setStyleSheet(QStringLiteral("color: gray; font-style: italic;"));
  ig_layout->addWidget(ig_label);
  content->addWidget(ig_group);

  content->addStretch();
}

void MSBGameStateWidget::CreatePreGameSection(QVBoxLayout* content)
{
  m_pregame_group = new QGroupBox(tr("Pre-Game Settings"));
  content->addWidget(m_pregame_group);

  auto* form = new QFormLayout(m_pregame_group);
  form->setLabelAlignment(Qt::AlignRight);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  // Stadium — use MSB_Constants names
  m_stadium_combo = new QComboBox;
  m_stadium_combo->addItem(tr("— Not Set —"), QVariant());
  for (const auto& [id, name] : MSB::STADIUM_LIST)
    m_stadium_combo->addItem(QString::fromUtf8(name), QVariant(static_cast<int>(id)));
  form->addRow(tr("Stadium:"), m_stadium_combo);

  m_innings_combo = new QComboBox;
  m_innings_combo->addItem(tr("— Not Set —"), QVariant());
  for (int v : {3, 5, 7, 9, 11, 13, 15, 17, 19})
    m_innings_combo->addItem(QString::number(v), QVariant(v));
  form->addRow(tr("Innings:"), m_innings_combo);

  m_first_batter_combo = new QComboBox;
  m_first_batter_combo->addItem(tr("— Not Set —"), QVariant());
  m_first_batter_combo->addItem(tr("P1"), QVariant(0));
  m_first_batter_combo->addItem(tr("P2"), QVariant(1));
  form->addRow(tr("First Batter:"), m_first_batter_combo);

  m_star_skills_combo = new QComboBox;
  m_star_skills_combo->addItem(tr("— Not Set —"), QVariant());
  m_star_skills_combo->addItem(tr("Off"), QVariant(0));
  m_star_skills_combo->addItem(tr("On"),  QVariant(1));
  form->addRow(tr("Star Skills:"), m_star_skills_combo);

  m_mercy_combo = new QComboBox;
  m_mercy_combo->addItem(tr("— Not Set —"), QVariant());
  m_mercy_combo->addItem(tr("Off"), QVariant(0));
  m_mercy_combo->addItem(tr("On"),  QVariant(1));
  form->addRow(tr("Mercy Rule:"), m_mercy_combo);
}

void MSBGameStateWidget::CreateRosterSection(QVBoxLayout* content)
{
  m_roster_group = new QGroupBox(tr("Rosters"));
  content->addWidget(m_roster_group);

  auto* outer = new QVBoxLayout(m_roster_group);

  // Options row: which side is P1, and handedness toggle
  auto* opts = new QHBoxLayout;
  opts->addWidget(new QLabel(tr("P1 plays:")));
  m_p1_side_combo = new QComboBox;
  m_p1_side_combo->addItem(tr("Away"), QVariant(true));
  m_p1_side_combo->addItem(tr("Home"), QVariant(false));
  opts->addWidget(m_p1_side_combo);
  opts->addStretch();
  m_show_hand_check = new QCheckBox(tr("Show Handedness"));
  opts->addWidget(m_show_hand_check);
  outer->addLayout(opts);

  // Away team
  auto* away_group = new QGroupBox(tr("Away Team"));
  auto* away_layout = new QVBoxLayout(away_group);
  m_away_captain_group = new QButtonGroup(this);
  m_away_table = CreateTeamTable(m_away_captain_group);
  away_layout->addWidget(m_away_table);
  outer->addWidget(away_group);

  // Home team (stacked below)
  auto* home_group = new QGroupBox(tr("Home Team"));
  auto* home_layout = new QVBoxLayout(home_group);
  m_home_captain_group = new QButtonGroup(this);
  m_home_table = CreateTeamTable(m_home_captain_group);
  home_layout->addWidget(m_home_table);
  outer->addWidget(home_group);

  // Hide hand columns until the checkbox is ticked
  for (QTableWidget* t : {m_away_table, m_home_table})
  {
    t->setColumnHidden(COL_BAT_HAND, true);
    t->setColumnHidden(COL_FLD_HAND, true);
  }
}

QTableWidget* MSBGameStateWidget::CreateTeamTable(QButtonGroup* captain_group)
{
  auto* table = new QTableWidget(9, COL_COUNT);
  table->setHorizontalHeaderLabels(
      {tr("Character"), tr("Position"), tr("Capt"), tr("SS"), tr("Bat"), tr("Fld")});
  table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  table->verticalHeader()->setDefaultSectionSize(24);
  table->horizontalHeader()->setStretchLastSection(false);
  table->horizontalHeader()->setSectionResizeMode(COL_CHARACTER, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(COL_POSITION,  QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(COL_CAPTAIN,   QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(COL_SUPERSTAR, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(COL_BAT_HAND,  QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(COL_FLD_HAND,  QHeaderView::ResizeToContents);
  table->setSelectionMode(QAbstractItemView::NoSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);

  // Row labels = batting order slots 1–9
  for (int row = 0; row < 9; ++row)
  {
    table->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(row + 1)));

    table->setCellWidget(row, COL_CHARACTER, MakeCharCombo());
    table->setCellWidget(row, COL_POSITION,  MakePosCombo());

    auto* capt = new QRadioButton;
    capt->setProperty("row", row);
    captain_group->addButton(capt, row);
    auto* capt_cell = new QWidget;
    auto* capt_lay  = new QHBoxLayout(capt_cell);
    capt_lay->setContentsMargins(0, 0, 0, 0);
    capt_lay->setAlignment(Qt::AlignCenter);
    capt_lay->addWidget(capt);
    table->setCellWidget(row, COL_CAPTAIN, capt_cell);

    auto* ss = new QCheckBox;
    auto* ss_cell = new QWidget;
    auto* ss_lay  = new QHBoxLayout(ss_cell);
    ss_lay->setContentsMargins(0, 0, 0, 0);
    ss_lay->setAlignment(Qt::AlignCenter);
    ss_lay->addWidget(ss);
    table->setCellWidget(row, COL_SUPERSTAR, ss_cell);

    table->setCellWidget(row, COL_BAT_HAND, MakeHandCombo());
    table->setCellWidget(row, COL_FLD_HAND, MakeHandCombo());
  }

  // Compact height: 9 rows * 24px + header
  table->setMinimumHeight(9 * 24 + table->horizontalHeader()->height() + 4);
  table->setMaximumHeight(table->minimumHeight());

  return table;
}

// ── Signal wiring ─────────────────────────────────────────────────────────────

void MSBGameStateWidget::ConnectWidgets()
{
  connect(m_enable_check, &QCheckBox::stateChanged, this, [this](int) {
    UpdateEditability();
    if (!m_enable_check->isChecked())
    {
      Clear();
      emit ClearRequested();
    }
  });

  connect(m_apply_btn, &QPushButton::clicked, this,
          [this] { emit ApplyRequested(BuildState()); });

  connect(m_clear_btn, &QPushButton::clicked, this, [this] {
    Clear();
    emit ClearRequested();
  });

  connect(m_show_hand_check, &QCheckBox::stateChanged, this, [this](int state) {
    const bool show = (state == Qt::Checked);
    for (QTableWidget* t : {m_away_table, m_home_table})
    {
      t->setColumnHidden(COL_BAT_HAND, !show);
      t->setColumnHidden(COL_FLD_HAND, !show);
    }
  });
}

void MSBGameStateWidget::UpdateEditability()
{
  const bool editable = m_enable_check->isChecked() && m_is_host;
  m_load_hud_btn->setEnabled(editable);
  m_apply_btn->setEnabled(editable);
  m_clear_btn->setEnabled(editable);
  m_scroll_content->setEnabled(editable);
}

// ── Public interface ──────────────────────────────────────────────────────────

void MSBGameStateWidget::SetHostMode(bool is_host)
{
  m_is_host = is_host;
  m_enable_check->setEnabled(is_host);
  UpdateEditability();
}

bool MSBGameStateWidget::IsEnabled() const
{
  return m_enable_check->isChecked();
}

void MSBGameStateWidget::Clear()
{
  // Pre-game
  m_stadium_combo->setCurrentIndex(0);
  m_innings_combo->setCurrentIndex(0);
  m_first_batter_combo->setCurrentIndex(0);
  m_star_skills_combo->setCurrentIndex(0);
  m_mercy_combo->setCurrentIndex(0);

  // Rosters
  m_p1_side_combo->setCurrentIndex(0);  // P1 = Away
  for (QTableWidget* table : {m_away_table, m_home_table})
  {
    for (int row = 0; row < 9; ++row)
    {
      static_cast<QComboBox*>(table->cellWidget(row, COL_CHARACTER))->setCurrentIndex(0);
      static_cast<QComboBox*>(table->cellWidget(row, COL_POSITION))->setCurrentIndex(0);
      static_cast<QComboBox*>(table->cellWidget(row, COL_BAT_HAND))->setCurrentIndex(0);
      static_cast<QComboBox*>(table->cellWidget(row, COL_FLD_HAND))->setCurrentIndex(0);

      // Uncheck captain and superstar
      auto* capt_cell = table->cellWidget(row, COL_CAPTAIN);
      auto* capt = capt_cell->findChild<QRadioButton*>();
      if (capt) capt->setAutoExclusive(false), capt->setChecked(false), capt->setAutoExclusive(true);

      auto* ss_cell = table->cellWidget(row, COL_SUPERSTAR);
      auto* ss = ss_cell->findChild<QCheckBox*>();
      if (ss) ss->setChecked(false);
    }
  }
}

// ── PopulateFromState ─────────────────────────────────────────────────────────

void MSBGameStateWidget::PopulateFromState(const MSB_QuickMatchState& state)
{
  // Pre-game
  SetComboByData(m_stadium_combo,      state.GetStadium());
  SetComboByData(m_innings_combo,      state.GetInningsSelected());
  SetComboByData(m_first_batter_combo, state.GetFirstBatter());
  SetComboByData(m_star_skills_combo,  state.GetStarSkills());
  SetComboByData(m_mercy_combo,        state.GetMercy());

  // P1 side
  const bool p1IsAway = state.GetP1IsAway();
  m_p1_side_combo->setCurrentIndex(p1IsAway ? 0 : 1);

  // Rosters — away/home tables hold the away/home *team*, not necessarily P1/P2
  const MSB_Team& awayTeam = p1IsAway ? state.GetP1() : state.GetP2();
  const MSB_Team& homeTeam = p1IsAway ? state.GetP2() : state.GetP1();
  PopulateTeamFromState(awayTeam, m_away_table, m_away_captain_group);
  PopulateTeamFromState(homeTeam, m_home_table, m_home_captain_group);
}

void MSBGameStateWidget::PopulateTeamFromState(const MSB_Team& team, QTableWidget* table,
                                               QButtonGroup* captain_group) const
{
  const auto captainSlot = team.GetCaptainBattingSlot();

  for (int slot = 0; slot < 9; ++slot)
  {
    const MSB_Player* p = team.GetPlayerByBattingSlot(static_cast<uint8_t>(slot));

    auto* char_combo = static_cast<QComboBox*>(table->cellWidget(slot, COL_CHARACTER));
    auto* pos_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_POSITION));
    auto* bat_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_BAT_HAND));
    auto* fld_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_FLD_HAND));
    auto* capt_cell  = table->cellWidget(slot, COL_CAPTAIN);
    auto* ss_cell    = table->cellWidget(slot, COL_SUPERSTAR);
    auto* capt       = capt_cell  ? capt_cell->findChild<QRadioButton*>()  : nullptr;
    auto* ss         = ss_cell    ? ss_cell->findChild<QCheckBox*>()        : nullptr;

    if (p && p->IsSet())
    {
      SetComboByData(char_combo, p->charID);
      SetComboByData(pos_combo,  p->position);
      SetComboByData(bat_combo,  p->battingHand);
      SetComboByData(fld_combo,  p->fieldingHand);
      if (ss) ss->setChecked(p->superstar.value_or(0) == 1);
    }
    else
    {
      char_combo->setCurrentIndex(0);
      pos_combo->setCurrentIndex(0);
      bat_combo->setCurrentIndex(0);
      fld_combo->setCurrentIndex(0);
      if (ss) ss->setChecked(false);
    }

    if (capt)
    {
      capt->setAutoExclusive(false);
      capt->setChecked(captainSlot.has_value() && captainSlot.value() == slot);
      capt->setAutoExclusive(true);
    }
  }
}

// ── BuildState ────────────────────────────────────────────────────────────────

MSB_QuickMatchState MSBGameStateWidget::BuildState() const
{
  MSB_QuickMatchState state;

  // Pre-game
  auto applyU8 = [&](QComboBox* combo, auto setter) {
    const QVariant v = combo->currentData();
    if (!v.isNull()) (state.*setter)(static_cast<uint8_t>(v.toInt()));
  };
  applyU8(m_stadium_combo,      &MSB_QuickMatchState::SetStadium);
  applyU8(m_innings_combo,      &MSB_QuickMatchState::SetInningsSelected);
  applyU8(m_first_batter_combo, &MSB_QuickMatchState::SetFirstBatter);
  applyU8(m_star_skills_combo,  &MSB_QuickMatchState::SetStarSkills);
  applyU8(m_mercy_combo,        &MSB_QuickMatchState::SetMercy);

  // P1 side
  const bool p1IsAway = m_p1_side_combo->currentData().toBool();
  state.SetP1IsAway(p1IsAway);

  MSB_Team awayTeam = BuildTeamFromTable(m_away_table, m_away_captain_group);
  MSB_Team homeTeam = BuildTeamFromTable(m_home_table, m_home_captain_group);

  if (p1IsAway)
  {
    state.SetP1(awayTeam);
    state.SetP2(homeTeam);
  }
  else
  {
    state.SetP1(homeTeam);
    state.SetP2(awayTeam);
  }

  return state;
}

MSB_Team MSBGameStateWidget::BuildTeamFromTable(QTableWidget* table,
                                                const QButtonGroup* captain_group) const
{
  MSB_Team team;
  const int captain_slot = captain_group->checkedId();  // -1 if none checked

  for (int slot = 0; slot < 9; ++slot)
  {
    auto* char_combo = static_cast<QComboBox*>(table->cellWidget(slot, COL_CHARACTER));
    const QVariant char_v = char_combo->currentData();
    if (char_v.isNull()) continue;  // row not set, skip

    auto* pos_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_POSITION));
    auto* bat_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_BAT_HAND));
    auto* fld_combo  = static_cast<QComboBox*>(table->cellWidget(slot, COL_FLD_HAND));
    auto* ss_cell    = table->cellWidget(slot, COL_SUPERSTAR);
    auto* ss         = ss_cell ? ss_cell->findChild<QCheckBox*>() : nullptr;

    const QVariant pos_v  = pos_combo->currentData();
    if (pos_v.isNull()) continue;  // position required alongside character

    const uint8_t charID   = static_cast<uint8_t>(char_v.toInt());
    const uint8_t position = static_cast<uint8_t>(pos_v.toInt());
    const uint8_t batHand  = bat_combo->currentData().isNull() ? 0
                             : static_cast<uint8_t>(bat_combo->currentData().toInt());
    const uint8_t fldHand  = fld_combo->currentData().isNull() ? 0
                             : static_cast<uint8_t>(fld_combo->currentData().toInt());
    const uint8_t superstar = (ss && ss->isChecked()) ? 1 : 0;

    MSB_Player player(charID, position, static_cast<uint8_t>(slot),
                      batHand, fldHand, superstar);
    team.SetPlayer(position, player);
  }

  if (captain_slot >= 0)
    team.SetCaptainBattingSlot(static_cast<uint8_t>(captain_slot));

  return team;
}
