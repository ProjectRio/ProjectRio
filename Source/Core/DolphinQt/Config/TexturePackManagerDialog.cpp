// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/TexturePackManagerDialog.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include <QAction>
#include <QBrush>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <picojson.h>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"

#include "VideoCommon/HiresTextures.h"

namespace
{
constexpr int kPackInfoRole = Qt::UserRole + 1;
constexpr size_t kMaxNameLength = 128;
constexpr size_t kMaxDescriptionLength = 1024;

QString CategoryLabel(TexturePackManagerDialog::Category c)
{
  using Category = TexturePackManagerDialog::Category;
  switch (c)
  {
  case Category::Stadium:
    return QObject::tr("Stadiums");
  case Category::Character:
    return QObject::tr("Characters");
  case Category::Logo:
    return QObject::tr("Logos");
  case Category::Misc:
    return QObject::tr("Misc");
  case Category::Uncategorized:
  default:
    return QObject::tr("Uncategorized");
  }
}

TexturePackManagerDialog::Category ParseCategory(const std::string& s)
{
  using Category = TexturePackManagerDialog::Category;
  if (s == "stadium")
    return Category::Stadium;
  if (s == "character")
    return Category::Character;
  if (s == "logo")
    return Category::Logo;
  if (s == "misc")
    return Category::Misc;
  return Category::Uncategorized;
}

TexturePackManagerDialog::GameTag GameTagFromHires(TexturePackGame g)
{
  switch (g)
  {
  case TexturePackGame::Baseball:
    return TexturePackManagerDialog::GameTag::Baseball;
  case TexturePackGame::Golf:
    return TexturePackManagerDialog::GameTag::Golf;
  case TexturePackGame::Any:
  default:
    return TexturePackManagerDialog::GameTag::Any;
  }
}

TexturePackGame HiresFromGameTag(TexturePackManagerDialog::GameTag g)
{
  switch (g)
  {
  case TexturePackManagerDialog::GameTag::Baseball:
    return TexturePackGame::Baseball;
  case TexturePackManagerDialog::GameTag::Golf:
    return TexturePackGame::Golf;
  case TexturePackManagerDialog::GameTag::Any:
  default:
    return TexturePackGame::Any;
  }
}

QString GameTagSuffix(TexturePackManagerDialog::GameTag g)
{
  switch (g)
  {
  case TexturePackManagerDialog::GameTag::Baseball:
    return QObject::tr("  [Baseball]");
  case TexturePackManagerDialog::GameTag::Golf:
    return QObject::tr("  [Golf]");
  case TexturePackManagerDialog::GameTag::Any:
  default:
    return QString();
  }
}

// Single source of truth for how a pack name is rendered in either pane.
QString MakeItemLabel(const TexturePackManagerDialog::PackInfo& info)
{
  QString label = QString::fromStdString(info.display_name);
  if (info.is_builtin)
    label += QObject::tr(" (built-in)");
  label += GameTagSuffix(info.game);
  return label;
}

bool IsValidPackFolderName(const std::string& name)
{
  if (name.empty() || name.size() > kMaxNameLength)
    return false;
  if (name == "." || name == "..")
    return false;
  if (name.find("..") != std::string::npos)
    return false;
  if (name.find('|') != std::string::npos)
    return false;
  if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
    return false;
  return true;
}

std::string ClampString(std::string s, size_t max_len)
{
  if (s.size() > max_len)
    s.resize(max_len);
  return s;
}

void ScanRoot(const std::string& root, bool is_builtin,
              std::map<std::string, TexturePackManagerDialog::PackInfo>& out)
{
  if (root.empty() || !File::IsDirectory(root))
    return;

  // List immediate subdirectories of root.
  const auto entries = File::ScanDirectoryTree(root, false);
  for (const auto& child : entries.children)
  {
    if (!child.isDirectory)
      continue;

    const std::string folder_name = child.virtualName;
    if (!IsValidPackFolderName(folder_name))
      continue;

    // First-write-wins so user packs (scanned first by caller) shadow Sys packs of the same name.
    if (out.count(folder_name))
      continue;

    TexturePackManagerDialog::PackInfo info;
    info.folder_name = folder_name;
    info.absolute_path = root + folder_name;
    info.display_name = folder_name;
    info.is_builtin = is_builtin;
    info.category = TexturePackManagerDialog::Category::Uncategorized;
    info.game = TexturePackManagerDialog::GameTag::Any;

    // Optional pack.json metadata.
    const std::string manifest_path = info.absolute_path + DIR_SEP + "pack.json";
    std::ifstream manifest_stream(manifest_path);
    if (manifest_stream.good())
    {
      std::stringstream buffer;
      buffer << manifest_stream.rdbuf();
      picojson::value parsed;
      const std::string err = picojson::parse(parsed, buffer.str());
      if (err.empty() && parsed.is<picojson::object>())
      {
        const auto& obj = parsed.get<picojson::object>();
        if (auto it = obj.find("name"); it != obj.end() && it->second.is<std::string>())
          info.display_name = ClampString(it->second.get<std::string>(), kMaxNameLength);
        if (auto it = obj.find("author"); it != obj.end() && it->second.is<std::string>())
          info.author = ClampString(it->second.get<std::string>(), kMaxNameLength);
        if (auto it = obj.find("description"); it != obj.end() && it->second.is<std::string>())
          info.description =
              ClampString(it->second.get<std::string>(), kMaxDescriptionLength);
        if (auto it = obj.find("category"); it != obj.end() && it->second.is<std::string>())
          info.category = ParseCategory(it->second.get<std::string>());
        if (auto it = obj.find("game"); it != obj.end() && it->second.is<std::string>())
          info.game = GameTagFromHires(ParseTexturePackGame(it->second.get<std::string>()));
      }
    }

    // User-side override wins over manifest (and is the only way to tag built-ins).
    const TexturePackGame override_tag = ReadPackGameOverride(folder_name);
    if (override_tag != TexturePackGame::Any)
      info.game = GameTagFromHires(override_tag);

    out.emplace(folder_name, std::move(info));
  }
}

// Writes the game tag for a *user* pack into its pack.json. Built-ins go through the
// override file instead (see ReadPackGameOverride / WritePackGameOverride).
bool WritePackGameToManifest(const std::string& pack_root,
                             TexturePackManagerDialog::GameTag tag)
{
  if (pack_root.empty())
    return false;
  const std::string manifest_path = pack_root + DIR_SEP + "pack.json";

  picojson::object obj;
  std::ifstream in(manifest_path);
  if (in.good())
  {
    std::stringstream buffer;
    buffer << in.rdbuf();
    picojson::value parsed;
    if (picojson::parse(parsed, buffer.str()).empty() && parsed.is<picojson::object>())
      obj = parsed.get<picojson::object>();
  }

  std::string game_str;
  switch (tag)
  {
  case TexturePackManagerDialog::GameTag::Baseball:
    game_str = "baseball";
    break;
  case TexturePackManagerDialog::GameTag::Golf:
    game_str = "golf";
    break;
  case TexturePackManagerDialog::GameTag::Any:
  default:
    break;
  }

  if (game_str.empty())
    obj.erase("game");
  else
    obj["game"] = picojson::value(game_str);

  std::ofstream out_stream(manifest_path, std::ios::trunc);
  if (!out_stream.good())
    return false;
  out_stream << picojson::value(obj).serialize(/*prettify=*/true);
  return out_stream.good();
}
}  // namespace

TexturePackManagerDialog::TexturePackManagerDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Texture Pack Manager"));
  setMinimumSize(820, 520);

  BuildLayout();
  ScanAvailablePacks();
  PopulateActiveListFromConfig();
  PopulateAvailableTree();
  m_initial_active = CurrentActiveOrder();
  UpdateInlineNotice();
}

void TexturePackManagerDialog::BuildLayout()
{
  auto* main_layout = new QVBoxLayout;

  // Top row: linked global toggles.
  auto* toggles_box = new QGroupBox(tr("Custom Texture Loading"));
  auto* toggles_layout = new QHBoxLayout;
  m_load_custom_textures = new ConfigBool(tr("Load Custom Textures"), Config::GFX_HIRES_TEXTURES);
  m_prefetch_custom_textures =
      new ConfigBool(tr("Prefetch Custom Textures"), Config::GFX_CACHE_HIRES_TEXTURES);
  toggles_layout->addWidget(m_load_custom_textures);
  toggles_layout->addWidget(m_prefetch_custom_textures);
  toggles_layout->addStretch();
  toggles_box->setLayout(toggles_layout);
  main_layout->addWidget(toggles_box);

  // Middle row: two-pane available + active. We force both panes to share width by
  // giving them identical size policies and stretch factors, and pinning the middle
  // button column to its content width.
  auto* panes_layout = new QHBoxLayout;
  panes_layout->setSpacing(8);

  auto pane_size_policy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  pane_size_policy.setHorizontalStretch(1);

  auto* available_box = new QGroupBox(tr("Available Packs"));
  available_box->setSizePolicy(pane_size_policy);
  auto* available_layout = new QVBoxLayout;
  m_available_tree = new QTreeWidget;
  m_available_tree->setHeaderHidden(true);
  m_available_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_available_tree->setContextMenuPolicy(Qt::CustomContextMenu);
  m_available_tree->setMinimumWidth(300);
  connect(m_available_tree, &QTreeWidget::customContextMenuRequested, this,
          &TexturePackManagerDialog::OnAvailableContextMenu);
  connect(m_available_tree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem*, int) { OnAddSelected(); });
  available_layout->addWidget(m_available_tree);
  available_box->setLayout(available_layout);

  auto* mid_buttons_layout = new QVBoxLayout;
  mid_buttons_layout->setSpacing(6);
  m_add_button = new QPushButton(tr("Add →"));
  m_remove_button = new QPushButton(tr("← Remove"));
  m_add_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_remove_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  mid_buttons_layout->addStretch();
  mid_buttons_layout->addWidget(m_add_button);
  mid_buttons_layout->addWidget(m_remove_button);
  mid_buttons_layout->addStretch();

  auto* active_box = new QGroupBox(tr("Active Packs (top = highest priority)"));
  active_box->setSizePolicy(pane_size_policy);
  auto* active_layout = new QVBoxLayout;
  m_active_list = new QListWidget;
  m_active_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_active_list->setContextMenuPolicy(Qt::CustomContextMenu);
  m_active_list->setMinimumWidth(300);
  connect(m_active_list, &QListWidget::customContextMenuRequested, this,
          &TexturePackManagerDialog::OnActiveContextMenu);
  connect(m_active_list, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem*) { OnRemoveSelected(); });
  active_layout->addWidget(m_active_list);

  auto* active_buttons_layout = new QHBoxLayout;
  m_up_button = new QPushButton(tr("↑ Move Up"));
  m_down_button = new QPushButton(tr("↓ Move Down"));
  active_buttons_layout->addWidget(m_up_button);
  active_buttons_layout->addWidget(m_down_button);
  active_buttons_layout->addStretch();
  active_layout->addLayout(active_buttons_layout);
  active_box->setLayout(active_layout);

  panes_layout->addWidget(available_box, 1);
  panes_layout->addLayout(mid_buttons_layout, 0);
  panes_layout->addWidget(active_box, 1);
  main_layout->addLayout(panes_layout, 1);

  // Inline notice for in-emulation edits.
  m_inline_notice = new QLabel;
  m_inline_notice->setWordWrap(true);
  m_inline_notice->setStyleSheet(QStringLiteral("color: palette(highlight);"));
  m_inline_notice->hide();
  main_layout->addWidget(m_inline_notice);

  // Bottom row: Open Folder + Refresh on the left, Apply + Close together on the right.
  // We use explicit QPushButtons rather than QDialogButtonBox so the Apply/Close pairing
  // is consistent across platforms (QDialogButtonBox reorders by native convention on macOS).
  auto* bottom_layout = new QHBoxLayout;
  auto* open_folder_button = new QPushButton(tr("Open Texture Packs Folder"));
  auto* refresh_button = new QPushButton(tr("Refresh"));
  auto* apply_button = new QPushButton(tr("Apply"));
  auto* close_button = new QPushButton(tr("Close"));
  apply_button->setDefault(false);
  close_button->setDefault(true);
  bottom_layout->addWidget(open_folder_button);
  bottom_layout->addWidget(refresh_button);
  bottom_layout->addStretch();
  bottom_layout->addWidget(apply_button);
  bottom_layout->addWidget(close_button);
  main_layout->addLayout(bottom_layout);

  setLayout(main_layout);

  connect(m_add_button, &QPushButton::clicked, this, &TexturePackManagerDialog::OnAddSelected);
  connect(m_remove_button, &QPushButton::clicked, this,
          &TexturePackManagerDialog::OnRemoveSelected);
  connect(m_up_button, &QPushButton::clicked, this, &TexturePackManagerDialog::OnMoveUp);
  connect(m_down_button, &QPushButton::clicked, this, &TexturePackManagerDialog::OnMoveDown);
  connect(refresh_button, &QPushButton::clicked, this, &TexturePackManagerDialog::OnRefresh);
  connect(open_folder_button, &QPushButton::clicked, this,
          &TexturePackManagerDialog::OnOpenFolder);
  connect(apply_button, &QPushButton::clicked, this, &TexturePackManagerDialog::OnApply);
  connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
}

void TexturePackManagerDialog::ScanAvailablePacks()
{
  m_available_packs.clear();

  // User packs first so they shadow Sys packs of the same folder name.
  std::map<std::string, PackInfo> by_name;
  ScanRoot(File::GetUserPath(D_TEXTUREPACKS_IDX), /*is_builtin=*/false, by_name);
  ScanRoot(File::GetSysDirectory() + TEXTUREPACKS_DIR + DIR_SEP, /*is_builtin=*/true, by_name);

  for (auto& [_, info] : by_name)
    m_available_packs.push_back(std::move(info));

  std::sort(m_available_packs.begin(), m_available_packs.end(),
            [](const PackInfo& a, const PackInfo& b) {
              if (a.category != b.category)
                return static_cast<int>(a.category) < static_cast<int>(b.category);
              return a.display_name < b.display_name;
            });
}

void TexturePackManagerDialog::PopulateAvailableTree()
{
  m_available_tree->clear();

  // Build a set of folders already in the active pane so we can omit them here.
  std::set<std::string> in_active;
  for (const std::string& s : CurrentActiveOrder())
    in_active.insert(s);

  std::map<Category, QTreeWidgetItem*> category_nodes;
  const Category order[] = {Category::Stadium, Category::Character, Category::Logo,
                            Category::Misc, Category::Uncategorized};
  for (Category c : order)
  {
    auto* node = new QTreeWidgetItem(m_available_tree, {CategoryLabel(c)});
    QFont f = node->font(0);
    f.setBold(true);
    node->setFont(0, f);
    node->setFlags(node->flags() & ~Qt::ItemIsSelectable);
    node->setExpanded(true);
    category_nodes[c] = node;
  }

  const GameTag current_game = CurrentEmulatedGame();

  for (const PackInfo& info : m_available_packs)
  {
    if (in_active.count(info.folder_name))
      continue;  // Filtered out — already on the right side.

    auto* item = new QTreeWidgetItem(category_nodes[info.category], {MakeItemLabel(info)});
    item->setData(0, kPackInfoRole, QString::fromStdString(info.folder_name));

    QString tooltip;
    if (!info.author.empty())
      tooltip += tr("Author: %1\n").arg(QString::fromStdString(info.author));
    if (!info.description.empty())
      tooltip += QString::fromStdString(info.description) + QStringLiteral("\n");

    const bool mismatched = current_game != GameTag::Any && info.game != GameTag::Any &&
                            info.game != current_game;
    if (mismatched)
    {
      item->setForeground(0, QBrush(palette().color(QPalette::Disabled, QPalette::Text)));
      tooltip += tr("Not loaded for the running game.");
    }

    if (!tooltip.isEmpty())
      item->setToolTip(0, tooltip);
  }

  // Hide empty category nodes for cleaner display.
  for (auto& [_, node] : category_nodes)
    node->setHidden(node->childCount() == 0);
}

void TexturePackManagerDialog::PopulateActiveListFromConfig()
{
  m_active_list->clear();

  std::map<std::string, const PackInfo*> by_folder;
  for (const auto& p : m_available_packs)
    by_folder[p.folder_name] = &p;

  const GameTag current_game = CurrentEmulatedGame();

  for (const std::string& folder_name : GetActiveTexturePacks())
  {
    QString label;
    GameTag pack_game = GameTag::Any;
    const PackInfo* info = nullptr;
    if (auto it = by_folder.find(folder_name); it != by_folder.end())
    {
      info = it->second;
      label = MakeItemLabel(*info);
      pack_game = info->game;
    }
    else
    {
      label = QString::fromStdString(folder_name) + tr(" (missing)");
    }

    auto* item = new QListWidgetItem(label, m_active_list);
    item->setData(kPackInfoRole, QString::fromStdString(folder_name));

    const bool mismatched = current_game != GameTag::Any && pack_game != GameTag::Any &&
                            pack_game != current_game;
    if (mismatched)
    {
      item->setForeground(QBrush(palette().color(QPalette::Disabled, QPalette::Text)));
      item->setToolTip(tr("Not loaded for the running game."));
    }
    else if (!info)
    {
      item->setForeground(QBrush(palette().color(QPalette::Disabled, QPalette::Text)));
    }
  }
}

std::vector<std::string> TexturePackManagerDialog::CurrentActiveOrder() const
{
  std::vector<std::string> out;
  for (int i = 0; i < m_active_list->count(); ++i)
    out.push_back(m_active_list->item(i)->data(kPackInfoRole).toString().toStdString());
  return out;
}

void TexturePackManagerDialog::UpdateInlineNotice()
{
  const bool emulation_running = Core::GetState() != Core::State::Uninitialized;
  const bool changed = CurrentActiveOrder() != m_initial_active;
  m_inline_notice->setVisible(emulation_running && changed);
  m_inline_notice->setText(
      tr("Emulation is running. Applying these changes will turn off Load Custom Textures; "
         "re-enable it to load the new pack list."));
}

void TexturePackManagerDialog::OnAddSelected()
{
  std::set<std::string> already_active;
  for (const std::string& s : CurrentActiveOrder())
    already_active.insert(s);

  std::map<std::string, const PackInfo*> by_folder;
  for (const auto& p : m_available_packs)
    by_folder[p.folder_name] = &p;

  for (QTreeWidgetItem* item : m_available_tree->selectedItems())
  {
    const QString folder_qs = item->data(0, kPackInfoRole).toString();
    if (folder_qs.isEmpty())  // category header
      continue;
    const std::string folder = folder_qs.toStdString();
    if (already_active.count(folder))
      continue;

    QString label;
    if (auto it = by_folder.find(folder); it != by_folder.end())
      label = MakeItemLabel(*it->second);
    else
      label = folder_qs;

    auto* row = new QListWidgetItem(label, m_active_list);
    row->setData(kPackInfoRole, folder_qs);
    already_active.insert(folder);
  }
  PopulateAvailableTree();
  UpdateInlineNotice();
}

void TexturePackManagerDialog::OnRemoveSelected()
{
  const QList<QListWidgetItem*> selected = m_active_list->selectedItems();
  for (QListWidgetItem* item : selected)
    delete m_active_list->takeItem(m_active_list->row(item));
  PopulateAvailableTree();
  UpdateInlineNotice();
}

void TexturePackManagerDialog::OnMoveUp()
{
  const int row = m_active_list->currentRow();
  if (row <= 0)
    return;
  QListWidgetItem* item = m_active_list->takeItem(row);
  m_active_list->insertItem(row - 1, item);
  m_active_list->setCurrentRow(row - 1);
  UpdateInlineNotice();
}

void TexturePackManagerDialog::OnMoveDown()
{
  const int row = m_active_list->currentRow();
  if (row < 0 || row >= m_active_list->count() - 1)
    return;
  QListWidgetItem* item = m_active_list->takeItem(row);
  m_active_list->insertItem(row + 1, item);
  m_active_list->setCurrentRow(row + 1);
  UpdateInlineNotice();
}

void TexturePackManagerDialog::OnRefresh()
{
  // Preserve the current (possibly-edited) active selection across the rescan.
  const std::vector<std::string> current_active = CurrentActiveOrder();
  ScanAvailablePacks();

  m_active_list->clear();
  std::map<std::string, const PackInfo*> by_folder;
  for (const auto& p : m_available_packs)
    by_folder[p.folder_name] = &p;
  for (const std::string& folder_name : current_active)
  {
    QString label;
    if (auto it = by_folder.find(folder_name); it != by_folder.end())
      label = MakeItemLabel(*it->second);
    else
      label = QString::fromStdString(folder_name) + tr(" (missing)");
    auto* item = new QListWidgetItem(label, m_active_list);
    item->setData(kPackInfoRole, QString::fromStdString(folder_name));
  }
  PopulateAvailableTree();
  UpdateInlineNotice();
}

void TexturePackManagerDialog::OnOpenFolder()
{
  const std::string path = File::GetUserPath(D_TEXTUREPACKS_IDX);
  // The user TexturePacks folder is created lazily — make sure it exists so the file
  // manager has something to open. Without this, openUrl is a no-op on macOS.
  File::CreateFullPath(path);
  QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path)));
}

void TexturePackManagerDialog::OnApply()
{
  const std::vector<std::string> new_order = CurrentActiveOrder();
  const bool changed = new_order != m_initial_active;

  SetActiveTexturePacks(new_order);
  // Persist immediately so a crash or hard quit doesn't lose the change.
  Config::Save();

  // If emulation is running and the list changed, force-disable Load Custom Textures so the
  // change takes effect cleanly when the user re-enables it (avoids a half-applied texture cache).
  if (changed && Core::GetState() != Core::State::Uninitialized &&
      Config::Get(Config::GFX_HIRES_TEXTURES))
  {
    Config::SetBaseOrCurrent(Config::GFX_HIRES_TEXTURES, false);
  }

  m_initial_active = new_order;
  UpdateInlineNotice();
}

TexturePackManagerDialog::GameTag TexturePackManagerDialog::CurrentEmulatedGame() const
{
  if (Core::GetState() == Core::State::Uninitialized)
    return GameTag::Any;
  return GameTagFromHires(DetectCurrentTexturePackGame(SConfig::GetInstance().GetGameID()));
}

void TexturePackManagerDialog::OnAvailableContextMenu(const QPoint& point)
{
  QTreeWidgetItem* item = m_available_tree->itemAt(point);
  if (!item)
    return;
  const QString folder_qs = item->data(0, kPackInfoRole).toString();
  if (folder_qs.isEmpty())  // category header
    return;
  ShowPackContextMenu(folder_qs.toStdString(),
                      m_available_tree->viewport()->mapToGlobal(point));
}

void TexturePackManagerDialog::OnActiveContextMenu(const QPoint& point)
{
  QListWidgetItem* item = m_active_list->itemAt(point);
  if (!item)
    return;
  ShowPackContextMenu(item->data(kPackInfoRole).toString().toStdString(),
                      m_active_list->viewport()->mapToGlobal(point));
}

void TexturePackManagerDialog::ShowPackContextMenu(const std::string& folder_name,
                                                   const QPoint& global_pos)
{
  const PackInfo* info = nullptr;
  for (const auto& p : m_available_packs)
  {
    if (p.folder_name == folder_name)
    {
      info = &p;
      break;
    }
  }

  QMenu menu(this);
  QMenu* game_menu = menu.addMenu(tr("Set Game"));
  // Built-ins now editable too: tag is stored in a User-side override file.
  const bool can_edit = info != nullptr;
  const GameTag current = info ? info->game : GameTag::Any;

  auto add = [&](const QString& label, GameTag tag) {
    auto* action = game_menu->addAction(label);
    action->setCheckable(true);
    action->setChecked(current == tag);
    action->setEnabled(can_edit);
    connect(action, &QAction::triggered, this,
            [this, folder_name, tag] { SetPackGameTag(folder_name, tag); });
  };
  add(tr("Auto (any)"), GameTag::Any);
  add(tr("Baseball"), GameTag::Baseball);
  add(tr("Golf"), GameTag::Golf);

  if (info && info->is_builtin)
  {
    menu.addSeparator();
    auto* note = menu.addAction(tr("Built-in pack — tag stored as user override"));
    note->setEnabled(false);
  }

  menu.exec(global_pos);
}

void TexturePackManagerDialog::SetPackGameTag(const std::string& folder_name, GameTag tag)
{
  const PackInfo* info = nullptr;
  for (const auto& p : m_available_packs)
  {
    if (p.folder_name == folder_name)
    {
      info = &p;
      break;
    }
  }
  if (!info)
    return;

  bool ok = false;
  if (info->is_builtin)
  {
    // Built-ins live under Sys/ which is treated as read-only — write a user-side override.
    ok = WritePackGameOverride(folder_name, HiresFromGameTag(tag));
  }
  else
  {
    // User packs: write directly to the pack's manifest.
    if (!info->absolute_path.empty())
      ok = WritePackGameToManifest(info->absolute_path, tag);
  }

  if (!ok)
  {
    QMessageBox::warning(this, tr("Texture Pack Manager"),
                         tr("Failed to update game tag for \"%1\".")
                             .arg(QString::fromStdString(info->display_name)));
    return;
  }

  OnRefresh();
}
