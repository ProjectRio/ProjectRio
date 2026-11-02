// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "UICommon/AutoUpdate.h"

TEST(AutoUpdate, ParseVersion)
{
  EXPECT_EQ(AutoUpdateChecker::ParseVersion("2.3.0"), (std::vector<u32>{2, 3, 0}));
  EXPECT_EQ(AutoUpdateChecker::ParseVersion("v2.10"), (std::vector<u32>{2, 10}));
  EXPECT_EQ(AutoUpdateChecker::ParseVersion("V2"), (std::vector<u32>{2}));

  for (const char* const version : {"", "v", "vv2.3", "2.3.1-hotfix", "2..3", "2.3.", ".2.3",
                                    " 2.3", "2. 3", "+2.3", "-2.3", "2.0x3", "2.4294967296"})
  {
    EXPECT_FALSE(AutoUpdateChecker::ParseVersion(version)) << version;
  }
}

TEST(AutoUpdate, IsNewerVersion)
{
  EXPECT_TRUE(AutoUpdateChecker::IsNewerVersion("2.3.1", "2.3.0"));
  EXPECT_TRUE(AutoUpdateChecker::IsNewerVersion("2.3.0.1", "2.3.0"));
  EXPECT_TRUE(AutoUpdateChecker::IsNewerVersion("2.10", "2.9"));
  EXPECT_TRUE(AutoUpdateChecker::IsNewerVersion("v2.3.1", "2.3.0"));

  EXPECT_FALSE(AutoUpdateChecker::IsNewerVersion("2.3.0", "2.3.0"));
  EXPECT_FALSE(AutoUpdateChecker::IsNewerVersion("2.3", "2.3.0"));
  EXPECT_FALSE(AutoUpdateChecker::IsNewerVersion("v2.3.0", "2.3.0"));
  EXPECT_FALSE(AutoUpdateChecker::IsNewerVersion("2.2.9", "2.3.0"));
  EXPECT_FALSE(AutoUpdateChecker::IsNewerVersion("2.9", "2.10"));
}

TEST(AutoUpdate, UnparseableVersionIsNeverNewer)
{
  EXPECT_FALSE(AutoUpdateChecker::IsNewerVersion("2.3.1-hotfix", "2.3.0"));
  EXPECT_FALSE(AutoUpdateChecker::IsNewerVersion("9.9.9-hotfix", "2.3.0"));
  EXPECT_FALSE(AutoUpdateChecker::IsNewerVersion("2.3.1", "2.3.0-hotfix"));
  EXPECT_FALSE(AutoUpdateChecker::IsNewerVersion("latest", "2.3.0"));
  EXPECT_FALSE(AutoUpdateChecker::IsNewerVersion("", "2.3.0"));
}

TEST(AutoUpdate, IsSameVersion)
{
  EXPECT_TRUE(AutoUpdateChecker::IsSameVersion("2.3", "2.3.0"));
  EXPECT_TRUE(AutoUpdateChecker::IsSameVersion("2.3.0", "2.3"));
  EXPECT_TRUE(AutoUpdateChecker::IsSameVersion("v2.3.0", "2.3.0"));
  EXPECT_TRUE(AutoUpdateChecker::IsSameVersion("2.3.0", "v2.3"));

  EXPECT_FALSE(AutoUpdateChecker::IsSameVersion("2.3.1", "2.3.0"));
  EXPECT_FALSE(AutoUpdateChecker::IsSameVersion("2.3.0.1", "2.3.0"));
  EXPECT_FALSE(AutoUpdateChecker::IsSameVersion("2.3.1-hotfix", "2.3.1"));
  EXPECT_FALSE(AutoUpdateChecker::IsSameVersion("2.3.1-hotfix", "2.3.1-hotfix"));
}
