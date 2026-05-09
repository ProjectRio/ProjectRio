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
class QTreeWidget;
class QTreeWidgetItem;

// Dialog for managing the active set and priority of texture packs.
//
// Packs live under either User/TexturePacks/<pack>/ or Sys/Load/TexturePacks/<pack>/. Each pack
// optionally contains a pack.json describing its name, author, category, and description; packs
// without one show up under "Uncategorized" using the folder name. The dialog edits the
// pipe-delimited GFX_TEXTURE_PACKS_ACTIVE config setting in priority order (top of list = highest
// priority). Linked checkboxes for "Load Custom Textures" and "Prefetch Custom Textures" mirror
// the controls in Graphics > Advanced via shared Config bindings.
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
  void BuildLayout();
  void ScanAvailablePacks();
  void PopulateAvailableTree();
  void PopulateActiveListFromConfig();

  void OnAddSelected();
  void OnRemoveSelected();
  void OnMoveUp();
  void OnMoveDown();
  void OnRefresh();
  void OnOpenFolder();
  void OnApply();
  void OnAvailableContextMenu(const QPoint& point);
  void OnActiveContextMenu(const QPoint& point);
  void ShowPackContextMenu(const std::string& folder_name, const QPoint& global_pos);
  void SetPackGameTag(const std::string& folder_name, GameTag tag);

  GameTag CurrentEmulatedGame() const;  // Returns Any if not running or unknown.

  std::vector<std::string> CurrentActiveOrder() const;
  void UpdateInlineNotice();

  std::vector<PackInfo> m_available_packs;
  std::vector<std::string> m_initial_active;

  QTreeWidget* m_available_tree = nullptr;
  QListWidget* m_active_list = nullptr;
  QPushButton* m_add_button = nullptr;
  QPushButton* m_remove_button = nullptr;
  QPushButton* m_up_button = nullptr;
  QPushButton* m_down_button = nullptr;
  ConfigBool* m_load_custom_textures = nullptr;
  ConfigBool* m_prefetch_custom_textures = nullptr;
  QLabel* m_inline_notice = nullptr;
};
