// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/TexturePackManagerDialog.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTabWidget>
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
constexpr int kCategoryRole = Qt::UserRole + 2;
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

std::string CategoryToString(TexturePackManagerDialog::Category c)
{
  using Category = TexturePackManagerDialog::Category;
  switch (c)
  {
  case Category::Stadium:
    return "stadium";
  case Category::Character:
    return "character";
  case Category::Logo:
    return "logo";
  case Category::Misc:
    return "misc";
  case Category::Uncategorized:
  default:
    return "";
  }
}

// Color used for the category dot. Chosen to read on both light and dark Qt palettes.
QColor CategoryColor(TexturePackManagerDialog::Category c)
{
  using Category = TexturePackManagerDialog::Category;
  switch (c)
  {
  case Category::Stadium:
    return QColor(0x4C, 0xAF, 0x50);  // green
  case Category::Character:
    return QColor(0xFF, 0x98, 0x00);  // orange
  case Category::Logo:
    return QColor(0x21, 0x96, 0xF3);  // blue
  case Category::Misc:
    return QColor(0x9E, 0x9E, 0x9E);  // gray
  case Category::Uncategorized:
  default:
    return QColor();  // invalid -> delegate omits the dot
  }
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

QString GameDisplayName(TexturePackManagerDialog::GameTag g)
{
  switch (g)
  {
  case TexturePackManagerDialog::GameTag::Baseball:
    return QObject::tr("Baseball");
  case TexturePackManagerDialog::GameTag::Golf:
    return QObject::tr("Golf");
  case TexturePackManagerDialog::GameTag::Any:
  default:
    return QObject::tr("Any");
  }
}

QString MakeItemLabel(const TexturePackManagerDialog::PackInfo& info)
{
  QString label = QString::fromStdString(info.display_name);
  if (info.is_builtin)
    label += QObject::tr(" (built-in)");
  return label;
}

QString MakeTagTooltipLine(const TexturePackManagerDialog::PackInfo& info)
{
  return QObject::tr("Category: %1 • Game: %2")
      .arg(CategoryLabel(info.category))
      .arg(GameDisplayName(info.game));
}

// Renders item text followed by a small colored dot indicating the pack's category. The
// delegate stays close to the default look (it delegates back to the base class for
// background, selection highlight, focus rect) and only adds the suffix dot — no other
// padding or color changes.
class CategoryDotDelegate : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override
  {
    // Let the base class draw the row (background, text, selection, icon if present).
    QStyledItemDelegate::paint(painter, option, index);

    const QVariant cat_v = index.data(kCategoryRole);
    if (!cat_v.isValid())
      return;
    const QColor color = cat_v.value<QColor>();
    if (!color.isValid())
      return;

    // Use the style's reported text rect rather than guessing — this handles tree indent and
    // QListWidget left-padding consistently across themes. The text is left-aligned inside
    // that rect by default, so its end is text_rect.left() + measured text width.
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);  // populates opt.text, opt.icon, etc.
    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    const QRect text_rect =
        style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
    const QFontMetrics fm(opt.font);
    const int text_width = fm.horizontalAdvance(opt.text);

    constexpr int kDotRadius = 5;
    constexpr int kGap = 8;
    const int dot_x = text_rect.left() + text_width + kGap + kDotRadius;
    const int dot_y = option.rect.center().y() + 1;  // +1 to optically center with baseline

    // Don't draw past the row's right edge — happens on very narrow widget widths.
    if (dot_x + kDotRadius > option.rect.right())
      return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawEllipse(QPointF(dot_x, dot_y), kDotRadius, kDotRadius);
    painter->restore();
  }
};

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

// Override store: shared file with HiresTextures.cpp's override path. Used here for category
// (which the loader doesn't care about) — game tag goes through HiresTextures helpers.
std::string PackOverridePathForDialog(const std::string& pack_name)
{
  return File::GetUserPath(D_USER_IDX) + "TexturePackOverrides" + DIR_SEP + pack_name + ".json";
}

picojson::object ReadOverrideObject(const std::string& pack_name)
{
  picojson::object obj;
  std::ifstream in(PackOverridePathForDialog(pack_name));
  if (!in.good())
    return obj;
  std::stringstream buffer;
  buffer << in.rdbuf();
  picojson::value parsed;
  if (picojson::parse(parsed, buffer.str()).empty() && parsed.is<picojson::object>())
    obj = parsed.get<picojson::object>();
  return obj;
}

bool WriteOverrideObject(const std::string& pack_name, const picojson::object& obj)
{
  const std::string path = PackOverridePathForDialog(pack_name);
  if (obj.empty())
  {
    if (File::Exists(path))
      File::Delete(path);
    return true;
  }
  File::CreateFullPath(File::GetUserPath(D_USER_IDX) + "TexturePackOverrides" + DIR_SEP);
  std::ofstream out(path, std::ios::trunc);
  if (!out.good())
    return false;
  out << picojson::value(obj).serialize(/*prettify=*/true);
  return out.good();
}

TexturePackManagerDialog::Category ReadCategoryOverride(const std::string& pack_name)
{
  const picojson::object obj = ReadOverrideObject(pack_name);
  auto it = obj.find("category");
  if (it == obj.end() || !it->second.is<std::string>())
    return TexturePackManagerDialog::Category::Uncategorized;
  return ParseCategory(it->second.get<std::string>());
}

bool WriteCategoryOverride(const std::string& pack_name,
                           TexturePackManagerDialog::Category cat)
{
  picojson::object obj = ReadOverrideObject(pack_name);
  const std::string cat_str = CategoryToString(cat);
  if (cat_str.empty())
    obj.erase("category");
  else
    obj["category"] = picojson::value(cat_str);
  return WriteOverrideObject(pack_name, obj);
}

bool WriteCategoryToManifest(const std::string& pack_root,
                             TexturePackManagerDialog::Category cat)
{
  if (pack_root.empty())
    return false;
  const std::string manifest_path = pack_root + DIR_SEP + "pack.json";
  picojson::object obj;
  {
    std::ifstream in(manifest_path);
    if (in.good())
    {
      std::stringstream buffer;
      buffer << in.rdbuf();
      picojson::value parsed;
      if (picojson::parse(parsed, buffer.str()).empty() && parsed.is<picojson::object>())
        obj = parsed.get<picojson::object>();
    }
  }
  const std::string cat_str = CategoryToString(cat);
  if (cat_str.empty())
    obj.erase("category");
  else
    obj["category"] = picojson::value(cat_str);
  std::ofstream out(manifest_path, std::ios::trunc);
  if (!out.good())
    return false;
  out << picojson::value(obj).serialize(/*prettify=*/true);
  return out.good();
}

void ScanRoot(const std::string& root, bool is_builtin,
              std::map<std::string, TexturePackManagerDialog::PackInfo>& out)
{
  if (root.empty() || !File::IsDirectory(root))
    return;

  const auto entries = File::ScanDirectoryTree(root, false);
  for (const auto& child : entries.children)
  {
    if (!child.isDirectory)
      continue;

    const std::string folder_name = child.virtualName;
    if (!IsValidPackFolderName(folder_name))
      continue;

    if (out.count(folder_name))
      continue;

    TexturePackManagerDialog::PackInfo info;
    info.folder_name = folder_name;
    info.absolute_path = root + folder_name;
    info.display_name = folder_name;
    info.is_builtin = is_builtin;
    info.category = TexturePackManagerDialog::Category::Uncategorized;
    info.game = TexturePackManagerDialog::GameTag::Any;

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

    // Built-in defaults: all currently shipped built-ins are Baseball stadium themes.
    if (is_builtin)
    {
      if (info.game == TexturePackManagerDialog::GameTag::Any)
        info.game = GameTagFromHires(GetBuiltinDefaultGame(folder_name));
      if (info.category == TexturePackManagerDialog::Category::Uncategorized &&
          GetBuiltinDefaultGame(folder_name) != TexturePackGame::Any)
      {
        info.category = TexturePackManagerDialog::Category::Stadium;
      }
    }

    // User-side overrides win over manifest and built-in defaults.
    const TexturePackGame override_game = ReadPackGameOverride(folder_name);
    if (override_game != TexturePackGame::Any)
      info.game = GameTagFromHires(override_game);
    const TexturePackManagerDialog::Category override_cat = ReadCategoryOverride(folder_name);
    if (override_cat != TexturePackManagerDialog::Category::Uncategorized)
      info.category = override_cat;

    out.emplace(folder_name, std::move(info));
  }
}

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

// Does the pack belong on a given tab? "Any" packs appear on both; tagged packs only on the
// matching tab.
bool PackBelongsOnTab(TexturePackManagerDialog::GameTag pack_tag,
                      TexturePackManagerDialog::GameTag tab)
{
  if (pack_tag == TexturePackManagerDialog::GameTag::Any)
    return true;
  return pack_tag == tab;
}
}  // namespace

TexturePackManagerDialog::TexturePackManagerDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Texture Pack Manager"));
  setMinimumSize(880, 560);

  m_baseball_tab.game = GameTag::Baseball;
  m_golf_tab.game = GameTag::Golf;

  BuildLayout();
  ScanAvailablePacks();
  PopulateActiveListFromConfig(m_baseball_tab);
  PopulateActiveListFromConfig(m_golf_tab);
  PopulateAvailableTree(m_baseball_tab);
  PopulateAvailableTree(m_golf_tab);
  m_baseball_tab.initial_active = CurrentActiveOrder(m_baseball_tab);
  m_golf_tab.initial_active = CurrentActiveOrder(m_golf_tab);
  UpdateInlineNotice();

  // If a game is running, default to its tab so the user sees the relevant list first.
  const GameTag current = CurrentEmulatedGame();
  if (current == GameTag::Golf)
    m_tab_widget->setCurrentIndex(1);
}

void TexturePackManagerDialog::BuildLayout()
{
  auto* main_layout = new QVBoxLayout;

  // Top: linked global toggles. These are dialog-wide because the underlying config is global.
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

  // Tabs: one per game family.
  m_tab_widget = new QTabWidget;
  m_tab_widget->addTab(BuildTab(m_baseball_tab, GameTag::Baseball),
                       tr("Mario Superstar Baseball"));
  m_tab_widget->addTab(BuildTab(m_golf_tab, GameTag::Golf), tr("Mario Golf Toadstool Tour"));
  main_layout->addWidget(m_tab_widget, 1);

  // Inline notice. Sits in its own row between the tabs and the bottom buttons so it has a
  // clearly-bounded slot — no fighting with the tab content area's bottom margin.
  m_inline_notice = new QLabel;
  m_inline_notice->setWordWrap(false);
  m_inline_notice->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_inline_notice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_inline_notice->setStyleSheet(
      QStringLiteral("QLabel { color: palette(highlight); padding: 0; margin: 0; }"));
  m_inline_notice->hide();
  main_layout->addWidget(m_inline_notice);

  // Bottom bar shared across tabs.
  auto* bottom_layout = new QHBoxLayout;
  auto* open_folder_button = new QPushButton(tr("Open Texture Packs Folder"));
  auto* refresh_button = new QPushButton(tr("Refresh"));
  auto* apply_button = new QPushButton(tr("Apply"));
  auto* close_button = new QPushButton(tr("Close"));
  close_button->setDefault(true);
  bottom_layout->addWidget(open_folder_button);
  bottom_layout->addWidget(refresh_button);
  bottom_layout->addStretch();
  bottom_layout->addWidget(apply_button);
  bottom_layout->addWidget(close_button);
  main_layout->addLayout(bottom_layout);

  setLayout(main_layout);

  connect(refresh_button, &QPushButton::clicked, this, &TexturePackManagerDialog::OnRefresh);
  connect(open_folder_button, &QPushButton::clicked, this,
          &TexturePackManagerDialog::OnOpenFolder);
  connect(apply_button, &QPushButton::clicked, this, &TexturePackManagerDialog::OnApply);
  connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
}

QWidget* TexturePackManagerDialog::BuildTab(Tab& tab, GameTag game)
{
  tab.game = game;

  auto* container = new QWidget;
  auto* outer = new QVBoxLayout(container);

  auto* panes_layout = new QHBoxLayout;
  panes_layout->setSpacing(8);

  auto pane_size_policy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  pane_size_policy.setHorizontalStretch(1);

  // Available pane.
  auto* available_box = new QGroupBox(tr("Available Packs"));
  available_box->setSizePolicy(pane_size_policy);
  auto* available_layout = new QVBoxLayout;
  tab.available_tree = new QTreeWidget;
  tab.available_tree->setHeaderHidden(true);
  tab.available_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tab.available_tree->setContextMenuPolicy(Qt::CustomContextMenu);
  tab.available_tree->setMinimumWidth(300);
  tab.available_tree->setItemDelegate(new CategoryDotDelegate(tab.available_tree));
  connect(tab.available_tree, &QTreeWidget::customContextMenuRequested, this,
          [this, &tab](const QPoint& p) { OnAvailableContextMenu(tab, p); });
  connect(tab.available_tree, &QTreeWidget::itemDoubleClicked, this,
          [this, &tab](QTreeWidgetItem*, int) { OnAddSelected(tab); });
  available_layout->addWidget(tab.available_tree);
  available_box->setLayout(available_layout);

  // Middle column: Add / Remove.
  auto* mid_buttons_layout = new QVBoxLayout;
  mid_buttons_layout->setSpacing(6);
  tab.add_button = new QPushButton(tr("Add →"));
  tab.remove_button = new QPushButton(tr("← Remove"));
  tab.add_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  tab.remove_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  mid_buttons_layout->addStretch();
  mid_buttons_layout->addWidget(tab.add_button);
  mid_buttons_layout->addWidget(tab.remove_button);
  mid_buttons_layout->addStretch();

  // Active pane.
  auto* active_box = new QGroupBox(tr("Active Packs (top = highest priority)"));
  active_box->setSizePolicy(pane_size_policy);
  auto* active_layout = new QVBoxLayout;
  tab.active_list = new QListWidget;
  tab.active_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tab.active_list->setContextMenuPolicy(Qt::CustomContextMenu);
  tab.active_list->setMinimumWidth(300);
  tab.active_list->setDragDropMode(QAbstractItemView::InternalMove);
  tab.active_list->setDefaultDropAction(Qt::MoveAction);
  tab.active_list->setItemDelegate(new CategoryDotDelegate(tab.active_list));
  connect(tab.active_list, &QListWidget::customContextMenuRequested, this,
          [this, &tab](const QPoint& p) { OnActiveContextMenu(tab, p); });
  connect(tab.active_list, &QListWidget::itemDoubleClicked, this,
          [this, &tab](QListWidgetItem*) { OnRemoveSelected(tab); });
  connect(tab.active_list->model(), &QAbstractItemModel::rowsMoved, this,
          [this] { UpdateInlineNotice(); });
  active_layout->addWidget(tab.active_list);

  auto* active_buttons_layout = new QHBoxLayout;
  tab.up_button = new QPushButton(tr("↑ Move Up"));
  tab.down_button = new QPushButton(tr("↓ Move Down"));
  active_buttons_layout->addWidget(tab.up_button);
  active_buttons_layout->addWidget(tab.down_button);
  active_buttons_layout->addStretch();
  active_layout->addLayout(active_buttons_layout);
  active_box->setLayout(active_layout);

  panes_layout->addWidget(available_box, 1);
  panes_layout->addLayout(mid_buttons_layout, 0);
  panes_layout->addWidget(active_box, 1);
  outer->addLayout(panes_layout, 1);

  connect(tab.add_button, &QPushButton::clicked, this,
          [this, &tab] { OnAddSelected(tab); });
  connect(tab.remove_button, &QPushButton::clicked, this,
          [this, &tab] { OnRemoveSelected(tab); });
  connect(tab.up_button, &QPushButton::clicked, this, [this, &tab] { OnMoveUp(tab); });
  connect(tab.down_button, &QPushButton::clicked, this, [this, &tab] { OnMoveDown(tab); });

  return container;
}

void TexturePackManagerDialog::ScanAvailablePacks()
{
  m_available_packs.clear();

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

void TexturePackManagerDialog::PopulateAvailableTree(Tab& tab)
{
  tab.available_tree->clear();

  std::set<std::string> in_active;
  for (const std::string& s : CurrentActiveOrder(tab))
    in_active.insert(s);

  std::map<Category, QTreeWidgetItem*> category_nodes;
  const Category order[] = {Category::Stadium, Category::Character, Category::Logo,
                            Category::Misc, Category::Uncategorized};
  for (Category c : order)
  {
    auto* node = new QTreeWidgetItem(tab.available_tree, {CategoryLabel(c)});
    QFont f = node->font(0);
    f.setBold(true);
    node->setFont(0, f);
    node->setFlags(node->flags() & ~Qt::ItemIsSelectable);
    node->setExpanded(true);
    category_nodes[c] = node;
  }

  for (const PackInfo& info : m_available_packs)
  {
    if (!PackBelongsOnTab(info.game, tab.game))
      continue;  // Filtered by tab game.
    if (in_active.count(info.folder_name))
      continue;  // Already in the active list for this tab.

    auto* item = new QTreeWidgetItem(category_nodes[info.category], {MakeItemLabel(info)});
    item->setData(0, kPackInfoRole, QString::fromStdString(info.folder_name));
    item->setData(0, kCategoryRole, CategoryColor(info.category));

    QString tooltip = MakeTagTooltipLine(info) + QStringLiteral("\n");
    if (!info.author.empty())
      tooltip += tr("Author: %1\n").arg(QString::fromStdString(info.author));
    if (!info.description.empty())
      tooltip += QString::fromStdString(info.description) + QStringLiteral("\n");
    item->setToolTip(0, tooltip.trimmed());
  }

  for (auto& [_, node] : category_nodes)
    node->setHidden(node->childCount() == 0);
}

void TexturePackManagerDialog::PopulateActiveListFromConfig(Tab& tab)
{
  tab.active_list->clear();

  std::map<std::string, const PackInfo*> by_folder;
  for (const auto& p : m_available_packs)
    by_folder[p.folder_name] = &p;

  for (const std::string& folder_name : GetActiveTexturePacks(HiresFromGameTag(tab.game)))
  {
    QString label;
    const PackInfo* info = nullptr;
    if (auto it = by_folder.find(folder_name); it != by_folder.end())
    {
      info = it->second;
      label = MakeItemLabel(*info);
    }
    else
    {
      label = QString::fromStdString(folder_name) + tr(" (missing)");
    }

    auto* item = new QListWidgetItem(label, tab.active_list);
    item->setData(kPackInfoRole, QString::fromStdString(folder_name));
    if (info)
    {
      item->setData(kCategoryRole, CategoryColor(info->category));
      item->setToolTip(MakeTagTooltipLine(*info));
    }
    else
    {
      item->setForeground(QBrush(palette().color(QPalette::Disabled, QPalette::Text)));
    }
  }
}

std::vector<std::string> TexturePackManagerDialog::CurrentActiveOrder(const Tab& tab) const
{
  std::vector<std::string> out;
  for (int i = 0; i < tab.active_list->count(); ++i)
    out.push_back(tab.active_list->item(i)->data(kPackInfoRole).toString().toStdString());
  return out;
}

void TexturePackManagerDialog::UpdateInlineNotice()
{
  const bool emulation_running = Core::GetState() != Core::State::Uninitialized;
  const GameTag running = CurrentEmulatedGame();
  // Only show the in-emulation warning when the running game's list has actually changed —
  // edits to the other tab don't disturb the live texture cache.
  bool running_list_changed = false;
  if (running == GameTag::Baseball)
    running_list_changed = CurrentActiveOrder(m_baseball_tab) != m_baseball_tab.initial_active;
  else if (running == GameTag::Golf)
    running_list_changed = CurrentActiveOrder(m_golf_tab) != m_golf_tab.initial_active;

  m_inline_notice->setVisible(emulation_running && running_list_changed);
  m_inline_notice->setText(
      tr("Emulation is running. Applying will reload the texture cache so the new pack "
         "order takes effect immediately."));
}

void TexturePackManagerDialog::OnAddSelected(Tab& tab)
{
  std::set<std::string> already_active;
  for (const std::string& s : CurrentActiveOrder(tab))
    already_active.insert(s);

  std::map<std::string, const PackInfo*> by_folder;
  for (const auto& p : m_available_packs)
    by_folder[p.folder_name] = &p;

  for (QTreeWidgetItem* item : tab.available_tree->selectedItems())
  {
    const QString folder_qs = item->data(0, kPackInfoRole).toString();
    if (folder_qs.isEmpty())
      continue;  // category header
    const std::string folder = folder_qs.toStdString();
    if (already_active.count(folder))
      continue;

    const PackInfo* info = nullptr;
    QString label;
    if (auto it = by_folder.find(folder); it != by_folder.end())
    {
      info = it->second;
      label = MakeItemLabel(*info);
    }
    else
    {
      label = folder_qs;
    }

    auto* row = new QListWidgetItem(label, tab.active_list);
    row->setData(kPackInfoRole, folder_qs);
    if (info)
    {
      row->setData(kCategoryRole, CategoryColor(info->category));
      row->setToolTip(MakeTagTooltipLine(*info));
    }
    already_active.insert(folder);
  }
  PopulateAvailableTree(tab);
  UpdateInlineNotice();
}

void TexturePackManagerDialog::OnRemoveSelected(Tab& tab)
{
  const QList<QListWidgetItem*> selected = tab.active_list->selectedItems();
  for (QListWidgetItem* item : selected)
    delete tab.active_list->takeItem(tab.active_list->row(item));
  PopulateAvailableTree(tab);
  UpdateInlineNotice();
}

void TexturePackManagerDialog::OnMoveUp(Tab& tab)
{
  const int row = tab.active_list->currentRow();
  if (row <= 0)
    return;
  QListWidgetItem* item = tab.active_list->takeItem(row);
  tab.active_list->insertItem(row - 1, item);
  tab.active_list->setCurrentRow(row - 1);
  UpdateInlineNotice();
}

void TexturePackManagerDialog::OnMoveDown(Tab& tab)
{
  const int row = tab.active_list->currentRow();
  if (row < 0 || row >= tab.active_list->count() - 1)
    return;
  QListWidgetItem* item = tab.active_list->takeItem(row);
  tab.active_list->insertItem(row + 1, item);
  tab.active_list->setCurrentRow(row + 1);
  UpdateInlineNotice();
}

void TexturePackManagerDialog::OnRefresh()
{
  // Preserve both tabs' current edits across the rescan.
  const std::vector<std::string> baseball_active = CurrentActiveOrder(m_baseball_tab);
  const std::vector<std::string> golf_active = CurrentActiveOrder(m_golf_tab);

  ScanAvailablePacks();

  auto rebuild_active = [&](Tab& tab, const std::vector<std::string>& folders) {
    tab.active_list->clear();
    std::map<std::string, const PackInfo*> by_folder;
    for (const auto& p : m_available_packs)
      by_folder[p.folder_name] = &p;
    for (const std::string& folder_name : folders)
    {
      QString label;
      const PackInfo* info = nullptr;
      if (auto it = by_folder.find(folder_name); it != by_folder.end())
      {
        info = it->second;
        label = MakeItemLabel(*info);
      }
      else
      {
        label = QString::fromStdString(folder_name) + tr(" (missing)");
      }
      auto* item = new QListWidgetItem(label, tab.active_list);
      item->setData(kPackInfoRole, QString::fromStdString(folder_name));
      if (info)
      {
        item->setData(kCategoryRole, CategoryColor(info->category));
        item->setToolTip(MakeTagTooltipLine(*info));
      }
    }
  };
  rebuild_active(m_baseball_tab, baseball_active);
  rebuild_active(m_golf_tab, golf_active);
  PopulateAvailableTree(m_baseball_tab);
  PopulateAvailableTree(m_golf_tab);
  UpdateInlineNotice();
}

void TexturePackManagerDialog::OnOpenFolder()
{
  const std::string path = File::GetUserPath(D_TEXTUREPACKS_IDX);
  File::CreateFullPath(path);
  QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path)));
}

void TexturePackManagerDialog::OnApply()
{
  const std::vector<std::string> baseball_new = CurrentActiveOrder(m_baseball_tab);
  const std::vector<std::string> golf_new = CurrentActiveOrder(m_golf_tab);
  const bool baseball_changed = baseball_new != m_baseball_tab.initial_active;
  const bool golf_changed = golf_new != m_golf_tab.initial_active;

  SetActiveTexturePacks(TexturePackGame::Baseball, baseball_new);
  SetActiveTexturePacks(TexturePackGame::Golf, golf_new);
  Config::Save();

  // If the running game's list changed mid-emulation, reload the texture cache so the new
  // pack order takes effect immediately. We do this by briefly toggling GFX_HIRES_TEXTURES
  // off and then back on: the off-edge triggers HiresTexture::Clear() and a TextureCacheBase
  // invalidation (dropping bound textures), and the on-edge re-runs HiresTexture::Update()
  // with the new active list. We defer the on-toggle via the event loop so the video thread
  // gets a chance to observe the off state — an immediate flip can collapse into a no-op.
  const GameTag running = CurrentEmulatedGame();
  const bool running_list_changed = (running == GameTag::Baseball && baseball_changed) ||
                                    (running == GameTag::Golf && golf_changed);
  if (running_list_changed && Core::GetState() != Core::State::Uninitialized &&
      Config::Get(Config::GFX_HIRES_TEXTURES))
  {
    Config::SetBaseOrCurrent(Config::GFX_HIRES_TEXTURES, false);
    // 150ms = ~9 frames at 60fps, comfortable margin for the video thread to observe the
    // off-edge even under stutter. The visible "no custom textures" gap is barely perceptible.
    QTimer::singleShot(150, this, [] {
      Config::SetBaseOrCurrent(Config::GFX_HIRES_TEXTURES, true);
    });
  }

  m_baseball_tab.initial_active = baseball_new;
  m_golf_tab.initial_active = golf_new;
  UpdateInlineNotice();
}

TexturePackManagerDialog::GameTag TexturePackManagerDialog::CurrentEmulatedGame() const
{
  if (Core::GetState() == Core::State::Uninitialized)
    return GameTag::Any;
  return GameTagFromHires(DetectCurrentTexturePackGame(SConfig::GetInstance().GetGameID()));
}

void TexturePackManagerDialog::OnAvailableContextMenu(Tab& tab, const QPoint& point)
{
  QTreeWidgetItem* item = tab.available_tree->itemAt(point);
  if (!item)
    return;
  const QString folder_qs = item->data(0, kPackInfoRole).toString();
  if (folder_qs.isEmpty())
    return;
  ShowPackContextMenu(folder_qs.toStdString(),
                      tab.available_tree->viewport()->mapToGlobal(point));
}

void TexturePackManagerDialog::OnActiveContextMenu(Tab& tab, const QPoint& point)
{
  QListWidgetItem* item = tab.active_list->itemAt(point);
  if (!item)
    return;
  ShowPackContextMenu(item->data(kPackInfoRole).toString().toStdString(),
                      tab.active_list->viewport()->mapToGlobal(point));
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
  const bool can_edit = info != nullptr;

  QMenu* category_menu = menu.addMenu(tr("Set Category"));
  const Category current_cat = info ? info->category : Category::Uncategorized;
  auto add_category = [&](const QString& label, Category cat) {
    auto* action = category_menu->addAction(label);
    action->setCheckable(true);
    action->setChecked(current_cat == cat);
    action->setEnabled(can_edit);
    connect(action, &QAction::triggered, this,
            [this, folder_name, cat] { SetPackCategory(folder_name, cat); });
  };
  add_category(tr("Auto (uncategorized)"), Category::Uncategorized);
  add_category(CategoryLabel(Category::Stadium), Category::Stadium);
  add_category(CategoryLabel(Category::Character), Category::Character);
  add_category(CategoryLabel(Category::Logo), Category::Logo);
  add_category(CategoryLabel(Category::Misc), Category::Misc);

  QMenu* game_menu = menu.addMenu(tr("Set Game"));
  const GameTag current_game = info ? info->game : GameTag::Any;
  auto add_game = [&](const QString& label, GameTag tag) {
    auto* action = game_menu->addAction(label);
    action->setCheckable(true);
    action->setChecked(current_game == tag);
    action->setEnabled(can_edit);
    connect(action, &QAction::triggered, this,
            [this, folder_name, tag] { SetPackGameTag(folder_name, tag); });
  };
  add_game(tr("Both games (any)"), GameTag::Any);
  add_game(tr("Baseball only"), GameTag::Baseball);
  add_game(tr("Golf only"), GameTag::Golf);

  if (info && info->is_builtin)
  {
    menu.addSeparator();
    auto* note = menu.addAction(tr("Built-in pack — tags stored as user overrides"));
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
    ok = WritePackGameOverride(folder_name, HiresFromGameTag(tag));
  }
  else if (!info->absolute_path.empty())
  {
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

void TexturePackManagerDialog::SetPackCategory(const std::string& folder_name, Category cat)
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
    ok = WriteCategoryOverride(folder_name, cat);
  else if (!info->absolute_path.empty())
    ok = WriteCategoryToManifest(info->absolute_path, cat);

  if (!ok)
  {
    QMessageBox::warning(this, tr("Texture Pack Manager"),
                         tr("Failed to update category for \"%1\".")
                             .arg(QString::fromStdString(info->display_name)));
    return;
  }
  OnRefresh();
}
