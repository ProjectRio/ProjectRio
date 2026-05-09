// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/HiresTextures.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <xxhash.h>

#include <fmt/format.h>
#include <fstream>
#include <sstream>

#include <picojson.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "Core/System.h"
#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"
#include <Common/MsgHandler.h>

constexpr std::string_view s_format_prefix{"tex1_"};

static std::unordered_map<std::string, std::shared_ptr<HiresTexture>> s_hires_texture_cache;
static std::unordered_map<std::string, bool> s_hires_texture_id_to_arbmipmap;

static auto s_file_library = std::make_shared<VideoCommon::DirectFilesystemAssetLibrary>();

// Forward declaration; defined alongside the public helpers near the bottom of the file.
static TexturePackGame ReadPackDeclaredGame(const std::string& pack_root);

namespace
{
std::pair<std::string, bool> GetNameArbPair(const TextureInfo& texture_info)
{
  if (s_hires_texture_id_to_arbmipmap.empty())
    return {"", false};

  const auto texture_name_details = texture_info.CalculateTextureName();
  // look for an exact match first
  const std::string full_name = texture_name_details.GetFullName();
  if (auto iter = s_hires_texture_id_to_arbmipmap.find(full_name);
      iter != s_hires_texture_id_to_arbmipmap.end())
  {
    return {full_name, iter->second};
  }

  // Single wildcard ignoring the tlut hash
  const std::string texture_name_single_wildcard_tlut =
      fmt::format("{}_{}_$_{}", texture_name_details.base_name, texture_name_details.texture_name,
                  texture_name_details.format_name);
  if (auto iter = s_hires_texture_id_to_arbmipmap.find(texture_name_single_wildcard_tlut);
      iter != s_hires_texture_id_to_arbmipmap.end())
  {
    return {texture_name_single_wildcard_tlut, iter->second};
  }

  // Single wildcard ignoring the texture hash
  const std::string texture_name_single_wildcard_tex =
      fmt::format("{}_${}_{}", texture_name_details.base_name, texture_name_details.tlut_name,
                  texture_name_details.format_name);
  if (auto iter = s_hires_texture_id_to_arbmipmap.find(texture_name_single_wildcard_tex);
      iter != s_hires_texture_id_to_arbmipmap.end())
  {
    return {texture_name_single_wildcard_tex, iter->second};
  }

  return {"", false};
}
}  // namespace

void HiresTexture::Init()
{
  Update();
}

void HiresTexture::Shutdown()
{
  Clear();
}

void HiresTexture::Update()
{
  if (!g_ActiveConfig.bHiresTextures)
  {
    Clear();
    return;
  }

  const std::string& game_id = SConfig::GetInstance().GetGameID();

  // Build an ORDERED list of texture directories. Earlier entries win on hash conflict
  // (try_emplace below is first-insert-wins), so the priority order is:
  //   1. Each active texture pack, in user-configured order (top of UI list = first here).
  //   2. The standard User/Load/Textures/<game_id>/ fallback (always-on lowest priority).
  std::vector<std::string> texture_directories;

  const TexturePackGame current_game = DetectCurrentTexturePackGame(game_id);
  const std::vector<std::string> active_packs = GetActiveTexturePacks();
  for (const auto& pack_name : active_packs)
  {
    const std::string pack_root = ResolveTexturePackPath(pack_name);
    if (pack_root.empty())
      continue;  // Pack folder no longer exists; silently skip.

    // Game filter: if the pack declares a game and it doesn't match the running one, skip it.
    // Packs that don't declare a game (Any) load for any title. We also skip the filter when the
    // running game itself isn't one we recognize, to avoid hiding everything in unsupported titles.
    if (current_game != TexturePackGame::Any)
    {
      const TexturePackGame pack_game = ReadPackDeclaredGame(pack_root);
      if (pack_game != TexturePackGame::Any && pack_game != current_game)
        continue;
    }

    texture_directories.push_back(pack_root);
  }

  // Lowest-priority fallback: User/Load/Textures/<game_id>/ (and gameid.txt subfolders).
  const std::set<std::string> custom_dirs =
      GetTextureDirectoriesWithGameId(File::GetUserPath(D_HIRESTEXTURES_IDX), game_id);
  for (const auto& dir : custom_dirs)
    texture_directories.push_back(dir);

  const std::vector<std::string> extensions{".png", ".dds"};

  auto& system = Core::System::GetInstance();

  for (const auto& texture_directory : texture_directories)
  {
    const auto texture_paths =
        Common::DoFileSearch({texture_directory}, extensions, /*recursive*/ true);

    bool failed_insert = false;
    for (auto& path : texture_paths)
    {
      std::string filename;
      SplitPath(path, nullptr, &filename, nullptr);

      if (filename.substr(0, s_format_prefix.length()) == s_format_prefix)
      {
        const size_t arb_index = filename.rfind("_arb");
        const bool has_arbitrary_mipmaps = arb_index != std::string::npos;
        if (has_arbitrary_mipmaps)
          filename.erase(arb_index, 4);

        const auto [it, inserted] =
            s_hires_texture_id_to_arbmipmap.try_emplace(filename, has_arbitrary_mipmaps);
        if (!inserted)
        {
          failed_insert = true;
        }
        else
        {
          // Since this is just a texture (single file) the mapper doesn't really matter
          // just provide a string
          s_file_library->SetAssetIDMapData(filename, std::map<std::string, std::filesystem::path>{
                                                          {"texture", StringToPath(path)}});

          if (g_ActiveConfig.bCacheHiresTextures)
          {
            auto hires_texture = std::make_shared<HiresTexture>(
                has_arbitrary_mipmaps,
                system.GetCustomAssetLoader().LoadGameTexture(filename, s_file_library));
            s_hires_texture_cache.try_emplace(filename, std::move(hires_texture));
          }
        }
      }
    }

    if (failed_insert)
    {
      ERROR_LOG_FMT(VIDEO, "One or more textures at path '{}' were already inserted",
                    texture_directory);
    }
  }

  if (g_ActiveConfig.bCacheHiresTextures)
  {
    OSD::AddMessage(fmt::format("Loading '{}' custom textures", s_hires_texture_cache.size()),
                    10000);
  }
  else
  {
    OSD::AddMessage(
        fmt::format("Found '{}' custom textures", s_hires_texture_id_to_arbmipmap.size()), 10000);
  }
}

void HiresTexture::Clear()
{
  s_hires_texture_cache.clear();
  s_hires_texture_id_to_arbmipmap.clear();
  s_file_library = std::make_shared<VideoCommon::DirectFilesystemAssetLibrary>();
}

std::shared_ptr<HiresTexture> HiresTexture::Search(const TextureInfo& texture_info)
{
  const auto [base_filename, has_arb_mipmaps] = GetNameArbPair(texture_info);
  if (base_filename == "")
    return nullptr;

  if (auto iter = s_hires_texture_cache.find(base_filename); iter != s_hires_texture_cache.end())
  {
    return iter->second;
  }
  else
  {
    auto& system = Core::System::GetInstance();
    auto hires_texture = std::make_shared<HiresTexture>(
        has_arb_mipmaps,
        system.GetCustomAssetLoader().LoadGameTexture(base_filename, s_file_library));
    if (g_ActiveConfig.bCacheHiresTextures)
    {
      s_hires_texture_cache.try_emplace(base_filename, hires_texture);
    }
    return hires_texture;
  }
}

HiresTexture::HiresTexture(bool has_arbitrary_mipmaps,
                           std::shared_ptr<VideoCommon::GameTextureAsset> asset)
    : m_has_arbitrary_mipmaps(has_arbitrary_mipmaps), m_game_texture(std::move(asset))
{
}

std::set<std::string> GetTextureDirectoriesWithGameId(const std::string& root_directory,
                                                      const std::string& game_id)
{
  std::string pack = Config::Get(Config::GFX_TEXTURE_PACK);
  bool isCustomTexturePack = pack == "" || pack == "Custom";
  std::string sTexturePack = isCustomTexturePack ? "" : pack;
  auto textures_search_results = Common::DoFileSearch(
      {File::GetUserPath(D_TEXTUREPACKS_IDX), File::GetSysDirectory() + TEXTUREPACKS_DIR});
  const auto textures_directory = File::GetSysDirectory() + TEXTUREPACKS_DIR + DIR_SEP;
  return GetTextureDirectoriesWithGameId(isCustomTexturePack ? root_directory : textures_directory,
                                         game_id, isCustomTexturePack, sTexturePack);
}


std::set<std::string> GetTextureDirectoriesWithGameId(const std::string& root_directory,
                                                      const std::string& game_id,
                                                      bool isCustomTexturePack,
                                                      std::string& texturePack)
{
  std::set<std::string> result;
  const std::string textureFolder = isCustomTexturePack ? game_id : texturePack;
  const std::string texture_directory = root_directory + textureFolder;

  if (File::Exists(texture_directory))
  {
    result.insert(texture_directory);
  }
  else
  {
    // If there's no directory with the region-specific ID, look for a 3-character region-free one
    const std::string region_free_directory = root_directory + game_id.substr(0, 3);

    if (File::Exists(region_free_directory))
    {
      result.insert(region_free_directory);
    }
  }

  const auto match_gameid_or_all = [game_id](const std::string& filename) {
    std::string basename;
    SplitPath(filename, nullptr, &basename, nullptr);
    return basename == game_id || basename == game_id.substr(0, 3) || basename == "all";
  };

  // Look for any other directories that might be specific to the given gameid
  const auto files = Common::DoFileSearch({root_directory}, {".txt"}, true);
  for (const auto& file : files)
  {
    if (match_gameid_or_all(file))
    {
      // The following code is used to calculate the top directory
      // of a found gameid.txt file
      // ex:  <root directory>/My folder/gameids/<gameid>.txt
      // would insert "<root directory>/My folder"
      const auto directory_path = file.substr(root_directory.size());
      const std::size_t first_path_separator_position = directory_path.find_first_of(DIR_SEP_CHR);
      result.insert(root_directory + directory_path.substr(0, first_path_separator_position));
    }
  }

  return result;
}

namespace
{
constexpr char kActivePackDelimiter = '|';

std::vector<std::string> SplitActivePacksString(const std::string& serialized)
{
  std::vector<std::string> packs;
  std::string current;
  for (const char c : serialized)
  {
    if (c == kActivePackDelimiter)
    {
      if (!current.empty())
        packs.push_back(current);
      current.clear();
    }
    else
    {
      current.push_back(c);
    }
  }
  if (!current.empty())
    packs.push_back(current);
  return packs;
}

std::string JoinActivePacksString(const std::vector<std::string>& packs)
{
  std::string out;
  for (size_t i = 0; i < packs.size(); ++i)
  {
    if (i > 0)
      out.push_back(kActivePackDelimiter);
    out += packs[i];
  }
  return out;
}

// One-time migration from the legacy single-value GFX_TEXTURE_PACK setting.
// Runs at most once per process: if the new list is empty and the legacy key holds a real
// pack name (not "" or "Custom"), seed the new list and clear the legacy key so this no
// longer fires on subsequent reads.
void MigrateLegacyTexturePackSetting()
{
  static bool s_migrated = false;
  if (s_migrated)
    return;
  s_migrated = true;

  const std::string current_active = Config::Get(Config::GFX_TEXTURE_PACKS_ACTIVE);
  if (!current_active.empty())
    return;

  const std::string legacy = Config::Get(Config::GFX_TEXTURE_PACK);
  if (legacy.empty() || legacy == "Custom")
    return;

  Config::SetBaseOrCurrent(Config::GFX_TEXTURE_PACKS_ACTIVE, legacy);
  Config::SetBaseOrCurrent(Config::GFX_TEXTURE_PACK, std::string{});
}
}  // namespace

std::vector<std::string> GetActiveTexturePacks()
{
  MigrateLegacyTexturePackSetting();
  return SplitActivePacksString(Config::Get(Config::GFX_TEXTURE_PACKS_ACTIVE));
}

void SetActiveTexturePacks(const std::vector<std::string>& packs)
{
  Config::SetBaseOrCurrent(Config::GFX_TEXTURE_PACKS_ACTIVE, JoinActivePacksString(packs));
}

std::string ResolveTexturePackPath(const std::string& pack_name)
{
  if (pack_name.empty())
    return {};

  // Defense-in-depth: reject anything that could escape the TexturePacks roots.
  if (pack_name.find(DIR_SEP_CHR) != std::string::npos ||
      pack_name.find('/') != std::string::npos || pack_name.find('\\') != std::string::npos ||
      pack_name.find(kActivePackDelimiter) != std::string::npos || pack_name == "." ||
      pack_name == ".." || pack_name.find("..") != std::string::npos)
  {
    return {};
  }

  const std::string user_candidate = File::GetUserPath(D_TEXTUREPACKS_IDX) + pack_name;
  if (File::IsDirectory(user_candidate))
    return user_candidate;

  const std::string sys_candidate =
      File::GetSysDirectory() + TEXTUREPACKS_DIR + DIR_SEP + pack_name;
  if (File::IsDirectory(sys_candidate))
    return sys_candidate;

  return {};
}

TexturePackGame ParseTexturePackGame(const std::string& s)
{
  std::string lower;
  lower.reserve(s.size());
  for (char c : s)
    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

  // Friendly names.
  if (lower == "baseball" || lower == "msb" || lower == "mssb")
    return TexturePackGame::Baseball;
  if (lower == "golf" || lower == "mggt")
    return TexturePackGame::Golf;

  // Game-ID aliases. Accept the 3-char region-free prefix or any region-specific full ID.
  // Mario Superstar Baseball: GYQ / GYQE01 / GYQP01 / GYQJ01.
  // Mario Golf Toadstool Tour: GFT / GFTE01 / GFTP01 / GFTJ01.
  if (lower.rfind("gyq", 0) == 0)
    return TexturePackGame::Baseball;
  if (lower.rfind("gft", 0) == 0)
    return TexturePackGame::Golf;

  return TexturePackGame::Any;
}

std::string TexturePackGameToString(TexturePackGame g)
{
  switch (g)
  {
  case TexturePackGame::Baseball:
    return "baseball";
  case TexturePackGame::Golf:
    return "golf";
  case TexturePackGame::Any:
  default:
    return "";
  }
}

TexturePackGame DetectCurrentTexturePackGame(const std::string& game_id)
{
  if (game_id.size() < 3)
    return TexturePackGame::Any;
  const std::string prefix = game_id.substr(0, 3);
  // Mario Superstar Baseball: GYQE01 / GYQP01 / GYQJ01 -> prefix GYQ.
  // Mario Golf Toadstool Tour: GFTE01 / GFTP01 / GFTJ01 -> prefix GFT.
  if (prefix == "GYQ")
    return TexturePackGame::Baseball;
  if (prefix == "GFT")
    return TexturePackGame::Golf;
  return TexturePackGame::Any;
}

static TexturePackGame ReadPackDeclaredGame(const std::string& pack_root)
{
  if (pack_root.empty())
    return TexturePackGame::Any;
  const std::string manifest_path = pack_root + DIR_SEP + "pack.json";
  std::ifstream in(manifest_path);
  if (!in.good())
    return TexturePackGame::Any;
  std::stringstream buffer;
  buffer << in.rdbuf();
  picojson::value parsed;
  if (!picojson::parse(parsed, buffer.str()).empty() || !parsed.is<picojson::object>())
    return TexturePackGame::Any;
  const auto& obj = parsed.get<picojson::object>();
  auto it = obj.find("game");
  if (it == obj.end() || !it->second.is<std::string>())
    return TexturePackGame::Any;
  return ParseTexturePackGame(it->second.get<std::string>());
}
