// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/TextureInfo.h"

enum class TextureFormat;

std::set<std::string> GetTextureDirectoriesWithGameId(const std::string& root_directory,
                                                      const std::string& game_id, bool isCustomTexturePack, std::string& texturePack);
std::set<std::string> GetTextureDirectoriesWithGameId(const std::string& root_directory,
                                                      const std::string& game_id);

// Game family a pack is intended for. "Any" means the pack has no `game` field declared
// in pack.json and is loaded for both supported games. The dialog filters by this when
// displaying packs in the Baseball / Golf tabs.
enum class TexturePackGame
{
  Any,
  Baseball,
  Golf,
};

// Returns the active texture pack folder names in priority order for a given game family
// (index 0 = highest priority, overrides everything below it). Performs a one-time migration
// from the legacy single-value GFX_TEXTURE_PACK and the older flat GFX_TEXTURE_PACKS_ACTIVE
// settings on first read.
//
// game must be Baseball or Golf — pass DetectCurrentTexturePackGame(SConfig::GetGameID()) to
// pick the running game. Passing Any returns an empty vector (no neutral list exists).
std::vector<std::string> GetActiveTexturePacks(TexturePackGame game);

// Persists the active texture pack list for the given game family (Baseball or Golf).
void SetActiveTexturePacks(TexturePackGame game, const std::vector<std::string>& packs);

// Resolves a pack folder name to an absolute path, searching the user TexturePacks root
// first, then the system TexturePacks root. Returns empty string if not found.
std::string ResolveTexturePackPath(const std::string& pack_name);

// Parses the user-facing game alias (case-insensitive). Accepted values:
//   baseball, msb, mssb -> Baseball
//   golf, mggt          -> Golf
//   anything else / ""  -> Any
TexturePackGame ParseTexturePackGame(const std::string& s);

// Inverse: canonical lowercase string written into pack.json ("baseball", "golf", or "" for Any).
std::string TexturePackGameToString(TexturePackGame g);

// Maps the running game's ID to the corresponding pack family. Returns Any if the game ID
// doesn't match either supported game (the loader then disables the filter).
TexturePackGame DetectCurrentTexturePackGame(const std::string& game_id);

// User-side override store for pack game tags. Lets the UI tag built-in (Sys/) packs that
// can't have their pack.json edited in place. Overrides win over pack.json when both exist.
// Returns Any if no override is set. Pass Any to clear the override.
TexturePackGame ReadPackGameOverride(const std::string& pack_name);
bool WritePackGameOverride(const std::string& pack_name, TexturePackGame tag);

// Resolves a pack's effective game tag, applying the same precedence the loader uses:
// override > pack.json > built-in default > Any. The UI uses this to decide which tab a
// pack appears in.
TexturePackGame ResolvePackGame(const std::string& pack_name);

// Returns the hardcoded default game tag for a known built-in pack folder name. The current
// shipped built-ins are all stadium themes for Mario Superstar Baseball; returns Any for any
// pack not in that list. Used as a fallback when no override or pack.json declares a game.
TexturePackGame GetBuiltinDefaultGame(const std::string& pack_name);

class HiresTexture
{
public:
  static void Init();
  static void Update();
  static void Clear();
  static void Shutdown();
  static std::shared_ptr<HiresTexture> Search(const TextureInfo& texture_info);

  HiresTexture(bool has_arbitrary_mipmaps, std::shared_ptr<VideoCommon::GameTextureAsset> asset);

  bool HasArbitraryMipmaps() const { return m_has_arbitrary_mipmaps; }
  const std::shared_ptr<VideoCommon::GameTextureAsset>& GetAsset() const { return m_game_texture; }

private:
  bool m_has_arbitrary_mipmaps = false;
  std::shared_ptr<VideoCommon::GameTextureAsset> m_game_texture;
};
