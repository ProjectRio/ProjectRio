// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <ed25519.h>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <mbedtls/base64.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "UpdaterCommon/UpdateManifest.h"

using namespace UpdateManifest;

namespace
{
struct TestKey
{
  PublicKey public_key;
  std::array<u8, 64> private_key;
};

// sha256 of "package".
constexpr char PACKAGE[] = "package";
constexpr char PACKAGE_SHA256[] =
    "bc4a71180870f7945155fbb02f4b0a2e3faa2a62d6d31b7039013055ed19869a";

constexpr char MANIFEST[] =
    "{\n"
    "  \"assets\": {\n"
    "    \"Project_Rio_Windows.zip\": {\n"
    "      \"sha256\": \"bc4a71180870f7945155fbb02f4b0a2e3faa2a62d6d31b7039013055ed19869a\",\n"
    "      \"size\": 7\n"
    "    }\n"
    "  },\n"
    "  \"format\": 1,\n"
    "  \"product\": \"ProjectRio\",\n"
    "  \"version\": \"2.3.1\"\n"
    "}\n";

TestKey MakeKey(u8 seed_byte)
{
  std::array<u8, 32> seed;
  seed.fill(seed_byte);

  TestKey key;
  ed25519_create_keypair(key.public_key.bytes.data(), key.private_key.data(), seed.data());
  return key;
}

std::span<const u8> Bytes(std::string_view text)
{
  return {reinterpret_cast<const u8*>(text.data()), text.size()};
}

std::string KeyId(const TestKey& key)
{
  std::string id;
  for (size_t i = 0; i < 8; ++i)
    id += fmt::format("{:02x}", key.public_key.bytes[i]);
  return id;
}

std::string Base64(std::span<const u8> data)
{
  std::array<u8, 128> encoded;
  size_t size = 0;
  EXPECT_EQ(mbedtls_base64_encode(encoded.data(), encoded.size(), &size, data.data(), data.size()),
            0);
  return std::string(reinterpret_cast<const char*>(encoded.data()), size);
}

std::string SignatureBase64(const TestKey& key, std::string_view manifest)
{
  std::array<u8, 64> signature;
  ed25519_sign(signature.data(), reinterpret_cast<const u8*>(manifest.data()), manifest.size(),
               key.public_key.bytes.data(), key.private_key.data());
  return Base64(signature);
}

std::string SignatureLine(const TestKey& key, std::string_view manifest)
{
  return KeyId(key) + " " + SignatureBase64(key, manifest) + "\n";
}

bool Verify(std::string_view manifest, std::string_view signature_file,
            std::span<const PublicKey> keys)
{
  std::string error;
  const bool result = VerifySignatures(Bytes(manifest), signature_file, keys, &error);
  EXPECT_EQ(result, error.empty());
  return result;
}

// Parse() must turn all of these down rather than abort the process.
void ExpectParseFails(const std::string& manifest)
{
  std::string error;
  EXPECT_FALSE(Parse(manifest, &error)) << manifest;
  EXPECT_FALSE(error.empty()) << manifest;
}

std::string ManifestWith(std::string_view format, std::string_view product,
                         std::string_view version, std::string_view assets)
{
  return fmt::format("{{\"assets\":{},\"format\":{},\"product\":{},\"version\":{}}}", assets,
                     format, product, version);
}

std::string AssetsWith(std::string_view size, std::string_view sha256)
{
  return fmt::format("{{\"a.zip\":{{\"sha256\":{},\"size\":{}}}}}", sha256, size);
}

std::string ValidHash()
{
  return fmt::format("\"{}\"", PACKAGE_SHA256);
}
}  // namespace

TEST(UpdateManifest, ValidSignature)
{
  const TestKey key = MakeKey(1);
  const std::array keys{key.public_key};
  EXPECT_TRUE(Verify(MANIFEST, SignatureLine(key, MANIFEST), keys));
}

TEST(UpdateManifest, SignatureFromSecondKey)
{
  const TestKey first = MakeKey(1);
  const TestKey second = MakeKey(2);
  const std::array keys{first.public_key, second.public_key};
  EXPECT_TRUE(Verify(MANIFEST, SignatureLine(second, MANIFEST), keys));
}

TEST(UpdateManifest, UnknownKeyIdBeforeValidLine)
{
  const TestKey key = MakeKey(1);
  const TestKey unknown = MakeKey(2);
  const std::array keys{key.public_key};
  EXPECT_TRUE(
      Verify(MANIFEST, SignatureLine(unknown, MANIFEST) + SignatureLine(key, MANIFEST), keys));
}

TEST(UpdateManifest, OnlyUnknownKeyIds)
{
  const TestKey key = MakeKey(1);
  const TestKey unknown = MakeKey(2);
  const std::array keys{key.public_key};
  EXPECT_FALSE(Verify(MANIFEST, SignatureLine(unknown, MANIFEST), keys));
}

TEST(UpdateManifest, KnownKeyIdWithAnotherKeysSignature)
{
  const TestKey key = MakeKey(1);
  const TestKey other = MakeKey(2);
  const std::array keys{key.public_key};
  EXPECT_FALSE(Verify(MANIFEST, KeyId(key) + " " + SignatureBase64(other, MANIFEST), keys));
}

TEST(UpdateManifest, ChangedManifestByte)
{
  const TestKey key = MakeKey(1);
  const std::array keys{key.public_key};
  const std::string signature = SignatureLine(key, MANIFEST);

  std::string manifest = MANIFEST;
  manifest[manifest.size() / 2] ^= 1;
  EXPECT_FALSE(Verify(manifest, signature, keys));
}

TEST(UpdateManifest, ReserializedManifest)
{
  const TestKey key = MakeKey(1);
  const std::array keys{key.public_key};
  const std::string signature = SignatureLine(key, MANIFEST);

  // Means the same thing, but those aren't the bytes that were signed.
  EXPECT_FALSE(Verify(ReplaceAll(MANIFEST, "\n", "\r\n"), signature, keys));
}

TEST(UpdateManifest, ChangedSignatureByte)
{
  const TestKey key = MakeKey(1);
  const std::array keys{key.public_key};

  std::array<u8, 64> signature;
  ed25519_sign(signature.data(), reinterpret_cast<const u8*>(MANIFEST), sizeof(MANIFEST) - 1,
               key.public_key.bytes.data(), key.private_key.data());
  signature[10] ^= 1;
  EXPECT_FALSE(Verify(MANIFEST, KeyId(key) + " " + Base64(signature) + "\n", keys));
}

TEST(UpdateManifest, MalformedSignatureLines)
{
  const TestKey key = MakeKey(1);
  const std::array keys{key.public_key};
  const std::string good = SignatureBase64(key, MANIFEST);
  const std::string id = KeyId(key);

  // Malformed base64.
  EXPECT_FALSE(Verify(MANIFEST, id + " " + good.substr(0, 40) + "!!!!", keys));
  EXPECT_FALSE(Verify(MANIFEST, id + " not base64 at all", keys));
  // Wrong length.
  EXPECT_FALSE(Verify(MANIFEST, id + " " + good.substr(0, 40), keys));
  EXPECT_FALSE(Verify(MANIFEST, id + " " + Base64(std::array<u8, 65>{}), keys));
  EXPECT_FALSE(Verify(MANIFEST, id + " ", keys));
  // Missing space.
  EXPECT_FALSE(Verify(MANIFEST, id + good, keys));
  EXPECT_FALSE(Verify(MANIFEST, id, keys));
}

TEST(UpdateManifest, EmptySignatureFile)
{
  const TestKey key = MakeKey(1);
  const std::array keys{key.public_key};
  EXPECT_FALSE(Verify(MANIFEST, "", keys));
  EXPECT_FALSE(Verify(MANIFEST, "\n", keys));
}

TEST(UpdateManifest, TooManySignatureLines)
{
  const TestKey key = MakeKey(1);
  const std::array keys{key.public_key};

  std::string signature_file;
  for (int i = 0; i < 8; ++i)
    signature_file += SignatureLine(key, MANIFEST);
  EXPECT_TRUE(Verify(MANIFEST, signature_file, keys));

  signature_file += SignatureLine(key, MANIFEST);
  EXPECT_FALSE(Verify(MANIFEST, signature_file, keys));
}

TEST(UpdateManifest, EmptyKeyList)
{
  const TestKey key = MakeKey(1);
  EXPECT_FALSE(Verify(MANIFEST, SignatureLine(key, MANIFEST), std::span<const PublicKey>{}));
}

TEST(UpdateManifest, ParseValidManifest)
{
  std::string error;
  const std::optional<Manifest> manifest = Parse(MANIFEST, &error);
  ASSERT_TRUE(manifest) << error;
  EXPECT_EQ(manifest->version, "2.3.1");
  ASSERT_EQ(manifest->assets.size(), 1u);
  const Asset& asset = manifest->assets.at("Project_Rio_Windows.zip");
  EXPECT_EQ(asset.size, 7u);
  EXPECT_EQ(asset.sha256, PACKAGE_SHA256);
}

TEST(UpdateManifest, ParseInvalidJson)
{
  ExpectParseFails("");
  ExpectParseFails("{");
  ExpectParseFails("not json");
  ExpectParseFails("{\"format\": 1,}");
}

TEST(UpdateManifest, ParseNonObjectRoot)
{
  ExpectParseFails("[]");
  ExpectParseFails("1");
  ExpectParseFails("\"ProjectRio\"");
  ExpectParseFails("null");
  ExpectParseFails("true");
}

TEST(UpdateManifest, ParseWrongFieldTypes)
{
  const std::string assets = AssetsWith("7", ValidHash());
  EXPECT_TRUE(Parse(ManifestWith("1", "\"ProjectRio\"", "\"2.3.1\"", assets), nullptr));

  for (const std::string_view wrong : {"null", "true", "[]", "{}", "\"1\""})
    ExpectParseFails(ManifestWith(wrong, "\"ProjectRio\"", "\"2.3.1\"", assets));
  for (const std::string_view wrong : {"null", "true", "[]", "{}", "1"})
  {
    ExpectParseFails(ManifestWith("1", wrong, "\"2.3.1\"", assets));
    ExpectParseFails(ManifestWith("1", "\"ProjectRio\"", wrong, assets));
  }
  for (const std::string_view wrong : {"null", "true", "[]", "1", "\"a\""})
    ExpectParseFails(ManifestWith("1", "\"ProjectRio\"", "\"2.3.1\"", wrong));

  for (const std::string_view wrong : {"null", "true", "[]", "1", "\"a\""})
  {
    ExpectParseFails(
        ManifestWith("1", "\"ProjectRio\"", "\"2.3.1\"", fmt::format("{{\"a.zip\":{}}}", wrong)));
  }
  for (const std::string_view wrong : {"null", "true", "[]", "{}", "\"7\""})
  {
    ExpectParseFails(
        ManifestWith("1", "\"ProjectRio\"", "\"2.3.1\"", AssetsWith(wrong, ValidHash())));
  }
  for (const std::string_view wrong : {"null", "true", "[]", "{}", "1"})
    ExpectParseFails(ManifestWith("1", "\"ProjectRio\"", "\"2.3.1\"", AssetsWith("7", wrong)));
}

TEST(UpdateManifest, ParseMissingFields)
{
  const std::string assets = AssetsWith("7", ValidHash());
  ExpectParseFails("{}");
  ExpectParseFails(fmt::format(R"({{"format":1,"product":"ProjectRio","version":"2.3.1"}})"));
  ExpectParseFails(
      fmt::format(R"({{"assets":{},"product":"ProjectRio","version":"2.3.1"}})", assets));
  ExpectParseFails(fmt::format(R"({{"assets":{},"format":1,"version":"2.3.1"}})", assets));
  ExpectParseFails(fmt::format(R"({{"assets":{},"format":1,"product":"ProjectRio"}})", assets));
  ExpectParseFails(
      ManifestWith("1", "\"ProjectRio\"", "\"2.3.1\"", fmt::format(R"({{"a.zip":{{"size":7}}}})")));
  ExpectParseFails(ManifestWith("1", "\"ProjectRio\"", "\"2.3.1\"",
                                fmt::format(R"({{"a.zip":{{"sha256":{}}}}})", ValidHash())));
}

TEST(UpdateManifest, ParseWrongFormatOrProduct)
{
  const std::string assets = AssetsWith("7", ValidHash());
  ExpectParseFails(ManifestWith("2", "\"ProjectRio\"", "\"2.3.1\"", assets));
  ExpectParseFails(ManifestWith("0", "\"ProjectRio\"", "\"2.3.1\"", assets));
  ExpectParseFails(ManifestWith("1.5", "\"ProjectRio\"", "\"2.3.1\"", assets));
  ExpectParseFails(ManifestWith("1", "\"Dolphin\"", "\"2.3.1\"", assets));
  ExpectParseFails(ManifestWith("1", "\"projectrio\"", "\"2.3.1\"", assets));
  ExpectParseFails(ManifestWith("1", "\"ProjectRio\"", "\"\"", assets));
}

TEST(UpdateManifest, ParseBadSizeOrHash)
{
  const auto manifest = [](std::string_view size, std::string_view sha256) {
    return ManifestWith("1", "\"ProjectRio\"", "\"2.3.1\"", AssetsWith(size, sha256));
  };

  ExpectParseFails(manifest("-1", ValidHash()));
  ExpectParseFails(manifest("1.5", ValidHash()));
  ExpectParseFails(manifest("1e300", ValidHash()));

  const std::string hash = PACKAGE_SHA256;
  ExpectParseFails(manifest("7", "\"\""));
  ExpectParseFails(manifest("7", fmt::format("\"{}\"", hash.substr(1))));
  ExpectParseFails(manifest("7", fmt::format("\"{}0\"", hash)));
  ExpectParseFails(manifest("7", fmt::format("\"{}g\"", hash.substr(1))));
}

TEST(UpdateManifest, PackageWrongSize)
{
  std::string error;
  EXPECT_FALSE(VerifyPackage(Asset{8, PACKAGE_SHA256}, Bytes(PACKAGE), &error));
  EXPECT_FALSE(error.empty());
}

TEST(UpdateManifest, PackageWrongHash)
{
  std::string error;
  EXPECT_FALSE(VerifyPackage(Asset{7, PACKAGE_SHA256}, Bytes("pAckage"), &error));
  EXPECT_FALSE(error.empty());
}

TEST(UpdateManifest, MatchingPackage)
{
  std::string error;
  EXPECT_TRUE(VerifyPackage(Asset{7, PACKAGE_SHA256}, Bytes(PACKAGE), &error)) << error;
}

TEST(UpdateManifest, MatchingPackageUppercaseHash)
{
  std::string uppercase = PACKAGE_SHA256;
  Common::ToUpper(&uppercase);

  // Through Parse() as well, since that is how an uppercase hash would get here.
  const std::string text = ManifestWith("1", "\"ProjectRio\"", "\"2.3.1\"",
                                        AssetsWith("7", fmt::format("\"{}\"", uppercase)));
  std::string error;
  const std::optional<Manifest> manifest = Parse(text, &error);
  ASSERT_TRUE(manifest) << error;
  EXPECT_TRUE(VerifyPackage(manifest->assets.at("a.zip"), Bytes(PACKAGE), &error)) << error;
}

// Written by Tools/sign-release.py with a throwaway key, whose private half was then discarded, to
// prove that the updater accepts what the tool writes. Regenerate it with the tool if the formats
// ever change; don't edit it by hand.
TEST(UpdateManifest, FileFromSigningTool)
{
  constexpr char TOOL_PACKAGE[] = "Project Rio fixture package\n";
  constexpr char TOOL_MANIFEST[] =
      "{\n"
      "  \"assets\": {\n"
      "    \"Project_Rio_Windows.zip\": {\n"
      "      \"sha256\": \"aa837010a25793ec0c275d91f7f4443f61bcef4bdfde2b24ec35cc75b0f11b60\",\n"
      "      \"size\": 28\n"
      "    }\n"
      "  },\n"
      "  \"format\": 1,\n"
      "  \"product\": \"ProjectRio\",\n"
      "  \"version\": \"2.3.1\"\n"
      "}\n";
  constexpr char TOOL_SIGNATURE[] =
      "211231ea4e064889 "
      "qtzWzRYlmxNdaPWXFRB6Hi/4dQ9crykkfoe+nN5Tfd/0N9zY1qj/TrJkUC9kASVaX5Cm/t4zYJ50Wgd6DCveDg==\n";
  constexpr std::array<PublicKey, 1> TOOL_KEYS{
      PublicKey{{0x21, 0x12, 0x31, 0xea, 0x4e, 0x06, 0x48, 0x89, 0x2a, 0x86, 0x38,
                 0xd1, 0xae, 0x7a, 0x99, 0xe4, 0x4d, 0x0d, 0xce, 0xfc, 0xad, 0x94,
                 0xf7, 0x15, 0x8c, 0x20, 0xfe, 0xa8, 0x6e, 0x24, 0x3d, 0x82}}};

  std::string error;
  ASSERT_TRUE(VerifySignatures(Bytes(TOOL_MANIFEST), TOOL_SIGNATURE, TOOL_KEYS, &error)) << error;

  const std::optional<Manifest> manifest = Parse(TOOL_MANIFEST, &error);
  ASSERT_TRUE(manifest) << error;
  EXPECT_EQ(manifest->version, "2.3.1");
  EXPECT_TRUE(
      VerifyPackage(manifest->assets.at("Project_Rio_Windows.zip"), Bytes(TOOL_PACKAGE), &error))
      << error;
}
