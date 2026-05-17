// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <string>
#include <vector>

class ConfigBool;
class QLabel;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

// Dialog for managing the active set and priority of texture packs.
//
// Packs live under either User/TexturePacks/<pack>/ or Sys/Load/TexturePacks/<pack>/. Each pack
// optionally contains a pack.json describing its name, author, category, description, and game
// family; packs without one show up under "Uncategorized" using the folder name.
//
// The dialog presents two tabs (Mario Superstar Baseball, Mario Golf Toadstool Tour). Each tab
// owns an independent priority list persisted to its own config key
// (GFX_TEXTURE_PACKS_BASEBALL / GFX_TEXTURE_PACKS_GOLF). Packs tagged "Any" appear in both
// tabs' available panes and can be activated in either, independently. Tagging a pack to a
// specific game restricts it to that tab.
class TexturePackManagerDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit TexturePackManagerDialog(QWidget* parent = nullptr);

  enum class Category
  {
    Stadium,
    Character,
    Logo,
    Misc,
    Uncategorized,
  };

  enum class GameTag
  {
    Any,
    Baseball,
    Golf,
  };

  struct PackInfo
  {
    std::string folder_name;     // identifier persisted in config
    std::string absolute_path;   // resolved path on disk
    std::string display_name;
    std::string author;
    std::string description;
    Category category = Category::Uncategorized;
    GameTag game = GameTag::Any;
    bool is_builtin = false;     // lives under Sys/ rather than User/
  };

private:
  // One tab's set of widgets + state. Two instances are created (Baseball / Golf) and most
  // populate/handler methods take a Tab& so the same code drives both.
  struct Tab
  {
    GameTag game = GameTag::Baseball;
    QTreeWidget* available_tree = nullptr;
    QListWidget* active_list = nullptr;
    QPushButton* add_button = nullptr;
    QPushButton* remove_button = nullptr;
    QPushButton* up_button = nullptr;
    QPushButton* down_button = nullptr;
    // The active list at dialog-open time, so we can detect "did the user change anything
    // mid-emulation?" and disable Load Custom Textures on Apply when they did.
    std::vector<std::string> initial_active;
  };

  void BuildLayout();
  QWidget* BuildTab(Tab& tab, GameTag game);
  void ScanAvailablePacks();

  void PopulateAvailableTree(Tab& tab);
  void PopulateActiveListFromConfig(Tab& tab);
  std::vector<std::string> CurrentActiveOrder(const Tab& tab) const;
  // Dialog-level: refreshes the single inline notice based on both tabs' state.
  void UpdateInlineNotice();

  void OnAddSelected(Tab& tab);
  void OnRemoveSelected(Tab& tab);
  void OnMoveUp(Tab& tab);
  void OnMoveDown(Tab& tab);
  void OnRefresh();
  void OnOpenFolder();
  void OnApply();
  void OnAvailableContextMenu(Tab& tab, const QPoint& point);
  void OnActiveContextMenu(Tab& tab, const QPoint& point);
  void ShowPackContextMenu(const std::string& folder_name, const QPoint& global_pos);
  void SetPackGameTag(const std::string& folder_name, GameTag tag);
  void SetPackCategory(const std::string& folder_name, Category cat);

  GameTag CurrentEmulatedGame() const;  // Returns Any if not running or unknown.

  std::vector<PackInfo> m_available_packs;

  QTabWidget* m_tab_widget = nullptr;
  Tab m_baseball_tab;
  Tab m_golf_tab;

  ConfigBool* m_load_custom_textures = nullptr;
  ConfigBool* m_prefetch_custom_textures = nullptr;
  QLabel* m_inline_notice = nullptr;
};
