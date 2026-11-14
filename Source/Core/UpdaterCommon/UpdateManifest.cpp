// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UpdaterCommon/UpdateManifest.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include <ed25519.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <picojson.h>

#include "Common/StringUtil.h"

namespace UpdateManifest
{
namespace
{
constexpr size_t MAX_SIGNATURE_LINES = 8;
constexpr size_t SIGNATURE_SIZE = 64;  // ed25519 sig size.
constexpr size_t KEY_ID_BYTES = 8;     // A key id is the first 16 hex characters of the public key.
constexpr size_t SHA256_HEX_SIZE = 64;

// A JSON number is a double, which stops being able to hold every whole number past 2^53.
constexpr double MAX_ASSET_SIZE = 9007199254740992.0;

constexpr char PRODUCT[] = "ProjectRio";
constexpr int FORMAT = 1;

void SetError(std::string* error, std::string message)
{
  if (error)
    *error = std::move(message);
}

std::string HexEncode(const u8* buffer, size_t size)
{
  std::string out(size * 2, '\0');

  for (size_t i = 0; i < size; ++i)
  {
    out[2 * i] = "0123456789abcdef"[buffer[i] >> 4];
    out[2 * i + 1] = "0123456789abcdef"[buffer[i] & 0xF];
  }

  return out;
}

std::vector<std::string_view> SplitLines(std::string_view text)
{
  std::vector<std::string_view> lines;
  while (!text.empty())
  {
    const size_t end = text.find('\n');
    std::string_view line = text.substr(0, end);
    if (line.ends_with('\r'))
      line.remove_suffix(1);
    lines.push_back(line);

    if (end == std::string_view::npos)
      break;
    text.remove_prefix(end + 1);
  }
  return lines;
}

const PublicKey* FindKey(std::string_view key_id, std::span<const PublicKey> keys)
{
  const auto it = std::find_if(keys.begin(), keys.end(), [key_id](const PublicKey& key) {
    return HexEncode(key.bytes.data(), KEY_ID_BYTES) == key_id;
  });
  return it != keys.end() ? &*it : nullptr;
}

bool VerifySignature(std::span<const u8> manifest, std::string_view b64_signature,
                     const PublicKey& key)
{
  std::array<u8, SIGNATURE_SIZE> signature;
  size_t sig_size;

  if (mbedtls_base64_decode(signature.data(), signature.size(), &sig_size,
                            reinterpret_cast<const u8*>(b64_signature.data()),
                            b64_signature.size()) != 0 ||
      sig_size != signature.size())
  {
    return false;
  }

  return ed25519_verify(signature.data(), manifest.data(), manifest.size(), key.bytes.data()) != 0;
}

template <typename T>
const T* GetField(const picojson::object& object, const std::string& name)
{
  const auto it = object.find(name);
  if (it == object.end() || !it->second.is<T>())
    return nullptr;
  return &it->second.get<T>();
}

std::optional<u64> ParseSize(const picojson::object& object)
{
  const double* const size = GetField<double>(object, "size");
  if (!size || *size < 0 || *size > MAX_ASSET_SIZE || std::floor(*size) != *size)
    return std::nullopt;
  return static_cast<u64>(*size);
}

bool IsSha256(std::string_view hash)
{
  return hash.size() == SHA256_HEX_SIZE && std::all_of(hash.begin(), hash.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
         });
}
}  // namespace

bool VerifySignatures(std::span<const u8> manifest, std::string_view signature_file,
                      std::span<const PublicKey> keys, std::string* error)
{
  if (keys.empty())
  {
    SetError(error, "This build trusts no update keys.");
    return false;
  }

  const std::vector<std::string_view> lines = SplitLines(signature_file);
  if (lines.size() > MAX_SIGNATURE_LINES)
  {
    SetError(error, "The signature file has too many lines.");
    return false;
  }

  bool found_known_key = false;
  for (const std::string_view line : lines)
  {
    const size_t separator = line.find(' ');
    if (separator == std::string_view::npos)
      continue;

    const PublicKey* const key = FindKey(line.substr(0, separator), keys);
    if (!key)
      continue;

    found_known_key = true;
    if (VerifySignature(manifest, line.substr(separator + 1), *key))
      return true;
  }

  SetError(error, found_known_key ? "No signature in the signature file is valid." :
                                    "The signature file has no signature from a trusted key.");
  return false;
}

std::optional<Manifest> Parse(std::string_view manifest, std::string* error)
{
  // get<T>() aborts the process on a type mismatch, so every value below is checked with is<T>()
  // first. GetField() does that for all of them.
  picojson::value root;
  const std::string parse_error = picojson::parse(root, std::string(manifest));
  if (!parse_error.empty())
  {
    SetError(error, "The manifest is not valid JSON: " + parse_error);
    return std::nullopt;
  }
  if (!root.is<picojson::object>())
  {
    SetError(error, "The manifest is not a JSON object.");
    return std::nullopt;
  }
  const picojson::object& object = root.get<picojson::object>();

  const double* const format = GetField<double>(object, "format");
  if (!format || *format != FORMAT)
  {
    SetError(error, "The manifest has an unsupported format.");
    return std::nullopt;
  }

  const std::string* const product = GetField<std::string>(object, "product");
  if (!product || *product != PRODUCT)
  {
    SetError(error, "The manifest is for a different product.");
    return std::nullopt;
  }

  const std::string* const version = GetField<std::string>(object, "version");
  if (!version || version->empty())
  {
    SetError(error, "The manifest has no version.");
    return std::nullopt;
  }

  const picojson::object* const assets = GetField<picojson::object>(object, "assets");
  if (!assets)
  {
    SetError(error, "The manifest has no asset list.");
    return std::nullopt;
  }

  Manifest result;
  result.version = *version;
  for (const auto& [name, value] : *assets)
  {
    if (!value.is<picojson::object>())
    {
      SetError(error, "The manifest entry for " + name + " is not an object.");
      return std::nullopt;
    }
    const picojson::object& asset = value.get<picojson::object>();

    const std::optional<u64> size = ParseSize(asset);
    if (!size)
    {
      SetError(error, "The manifest entry for " + name + " has an invalid size.");
      return std::nullopt;
    }

    const std::string* const sha256 = GetField<std::string>(asset, "sha256");
    if (!sha256 || !IsSha256(*sha256))
    {
      SetError(error, "The manifest entry for " + name + " has an invalid sha256.");
      return std::nullopt;
    }

    result.assets.emplace(name, Asset{*size, *sha256});
  }
  return result;
}

bool VerifyPackage(const Asset& asset, std::span<const u8> package, std::string* error)
{
  if (package.size() != asset.size)
  {
    SetError(error, "Package size mismatch: expected " + std::to_string(asset.size) + ", got " +
                        std::to_string(package.size()) + ".");
    return false;
  }

  std::array<u8, 32> hash;
  if (mbedtls_sha256_ret(package.data(), package.size(), hash.data(), false) != 0)
  {
    SetError(error, "Could not hash the package.");
    return false;
  }
  const std::string computed = HexEncode(hash.data(), hash.size());

  std::string expected = asset.sha256;
  Common::ToLower(&expected);
  if (computed != expected)
  {
    SetError(error, "Package hash mismatch: expected " + expected + ", got " + computed + ".");
    return false;
  }
  return true;
}
}  // namespace UpdateManifest
