// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Core/MSB_GenerateQuickMatchSetupGeckoCode.h"
#include "Core/MSB_HUDStateLoader.h"

namespace
{
const std::string AWAY_USER = "AwayUser";
const std::string HOME_USER = "HomeUser";

// A small JSON tree, kept separate from picojson so the fixture can be edited and written back out
// without going through the code under test.
struct HudNode
{
  enum class Kind
  {
    Scalar,
    Object,
    Array
  };

  Kind kind = Kind::Scalar;
  std::string scalar = "null";  // raw JSON text
  std::vector<std::pair<std::string, HudNode>> members;
  std::vector<HudNode> items;

  HudNode* Find(const std::string& key)
  {
    for (auto& [member_key, member] : members)
    {
      if (member_key == key)
        return &member;
    }
    return nullptr;
  }

  void Set(const std::string& key, HudNode value)
  {
    if (HudNode* member = Find(key))
      *member = std::move(value);
    else
      members.emplace_back(key, std::move(value));
  }

  void Remove(const std::string& key)
  {
    for (size_t i = 0; i < members.size(); i++)
    {
      if (members[i].first == key)
      {
        members.erase(members.begin() + i);
        return;
      }
    }
  }

  std::string Serialize() const
  {
    if (kind == Kind::Scalar)
      return scalar;

    std::string text = (kind == Kind::Object) ? "{" : "[";
    bool first = true;
    if (kind == Kind::Object)
    {
      for (const auto& [key, member] : members)
      {
        text += (first ? "\"" : ",\"") + key + "\":" + member.Serialize();
        first = false;
      }
      return text + "}";
    }
    for (const HudNode& item : items)
    {
      text += (first ? "" : ",") + item.Serialize();
      first = false;
    }
    return text + "]";
  }
};

HudNode Raw(const std::string& json)
{
  HudNode node;
  node.scalar = json;
  return node;
}

HudNode Num(int value)
{
  return Raw(std::to_string(value));
}

HudNode Str(const std::string& value)
{
  return Raw("\"" + value + "\"");
}

HudNode Obj(std::vector<std::pair<std::string, HudNode>> members = {})
{
  HudNode node;
  node.kind = HudNode::Kind::Object;
  node.members = std::move(members);
  return node;
}

HudNode Arr(std::vector<HudNode> items = {})
{
  HudNode node;
  node.kind = HudNode::Kind::Array;
  node.items = std::move(items);
  return node;
}

// Rosters are deliberately not in position order, so slot and position can't be confused.
int FixtureChar(int team, int roster)
{
  return team * 18 + roster;
}

int FixturePosition(int team, int roster)
{
  return (roster + 3 + team) % 9;
}

constexpr int CAPTAIN_ROSTER = 2;

int FixtureStamina(int team, int roster)
{
  return 100 + team * 10 + roster;
}

// The same fields, types and nesting as StatTracker::getHUDJSON with inDecode = false.
HudNode MakeHud()
{
  HudNode hud = Obj({
      {"GameID", Str("1a2b3c4d")},
      {"TagSetID", Num(-1)},
      {"Loaded from HUD", Num(0)},
      {"StadiumID", Num(4)},
      {"Innings Selected", Num(9)},
      {"First Batting Team", Num(0)},
      {"Star Skills On", Num(1)},
      {"Mercy On", Num(0)},
      {"Away Logo", Num(12)},
      {"Home Logo", Num(34)},
      {"Event Num", Str("12b")},
      {"Away Player", Str(AWAY_USER)},
      {"Home Player", Str(HOME_USER)},
      {"Away Port", Num(1)},
      {"Home Port", Num(2)},
      {"Inning", Num(3)},
      {"Half Inning", Num(1)},
      {"Away Score", Num(4)},
      {"Home Score", Num(2)},
      {"Away Inning Scores", Arr({Num(1), Num(3), Num(0)})},
      {"Home Inning Scores", Arr({Num(2), Num(0), Num(0)})},
      {"Balls", Num(2)},
      {"Strikes", Num(1)},
      {"Outs", Num(1)},
      {"Star Chance", Num(0)},
      {"Away Stars", Num(3)},
      {"Home Stars", Num(5)},
      {"Pitcher Stamina", Num(77)},
      {"Chemistry Links on Base", Num(0)},
      {"Num Outs During Play", Num(0)},
      {"Pitcher Roster Loc", Num(1)},
      {"Batter Roster Loc", Num(5)},
      {"Away Batter Roster Loc", Num(4)},
      {"Home Batter Roster Loc", Num(5)},
  });

  for (int team = 0; team < 2; team++)
  {
    for (int roster = 0; roster < 9; roster++)
    {
      HudNode defensive = Obj({
          {"Batters Faced", Num(0)},
          {"Runs Allowed", Num(0)},
          {"Earned Runs", Num(0)},
          {"Batters Walked", Num(0)},
          {"Batters Hit", Num(0)},
          {"Hits Allowed", Num(0)},
          {"HRs Allowed", Num(0)},
          {"Pitches Thrown", Num(0)},
          {"Stamina", Num(FixtureStamina(team, roster))},
          {"Was Pitcher", Num(0)},
          {"Strikeouts", Num(0)},
          {"Star Pitches Thrown", Num(0)},
          {"Big Plays", Num(0)},
          {"Outs Pitched", Num(0)},
          {"Batters Per Position", Arr({Obj({{"P", Num(3)}})})},
          {"Batter Outs Per Position", Arr()},
          {"Outs Per Position", Arr()},
      });
      HudNode offensive = Obj({
          {"At Bats", Num(1)},
          {"Hits", Num(0)},
          {"Singles", Num(0)},
          {"Doubles", Num(0)},
          {"Triples", Num(0)},
          {"Homeruns", Num(0)},
          {"Successful Bunts", Num(0)},
          {"Sac Flys", Num(0)},
          {"Strikeouts", Num(0)},
          {"Walks (4 Balls)", Num(0)},
          {"Walks (Hit)", Num(0)},
          {"RBI", Num(0)},
          {"Bases Stolen", Num(0)},
          {"Star Hits", Num(0)},
      });

      const std::string team_string = (team == 0) ? "Away" : "Home";
      hud.Set(team_string + " Roster " + std::to_string(roster),
              Obj({
                  {"Team", Str(std::to_string(team))},
                  {"RosterID", Num(roster)},
                  {"CharID", Num(FixtureChar(team, roster))},
                  {"Superstar", Num(roster == 6 ? 1 : 0)},
                  {"Captain", Num(roster == CAPTAIN_ROSTER ? 1 : 0)},
                  {"Fielding Hand", Num(roster % 2)},
                  {"Batting Hand", Num((roster + 1) % 2)},
                  {"Fielding Position", Num(FixturePosition(team, roster))},
                  {"Defensive Stats", std::move(defensive)},
                  {"Offensive Stats", std::move(offensive)},
              }));
    }
  }

  const auto runner = [](int roster_loc, int char_id, int base) {
    return Obj({
        {"Runner Roster Loc", Num(roster_loc)},
        {"Runner Char Id", Num(char_id)},
        {"Runner Initial Base", Num(base)},
        {"Out Type", Num(0)},
        {"Out Location", Num(0)},
        {"Steal", Num(0)},
        {"Runner Result Base", Num(base)},
    });
  };
  hud.Set("Runner Batter", runner(5, FixtureChar(1, 5), 0));
  hud.Set("Runner 1B", runner(4, FixtureChar(1, 4), 1));
  hud.Set("Runner 3B", runner(2, FixtureChar(1, 2), 3));

  hud.Set("Previous Event",
          Obj({
              {"RBI", Num(0)},
              {"Dead Ball Reason", Num(0)},
              {"Result of AB", Num(0)},
              {"Pitch", Obj({
                            {"Pitcher Team Id", Num(0)},
                            {"Pitcher Char Id", Num(1)},
                            {"Ball Position - Strikezone", Raw("-0.25")},
                            {"Type of Swing", Num(1)},
                            {"Contact", Obj({
                                            {"Type of Contact", Num(1)},
                                            {"Frame of Swing", Str("4")},
                                            {"First Fielder", Obj({
                                                                  {"Fielder Roster Location", Num(3)},
                                                                  {"Fielder Position - X", Raw("1.5")},
                                                              })},
                                        })},
                        })},
          }));
  return hud;
}

// Calls every entry point the HUD reaches in a netplay session, in the order the client does.
int RunAll(const std::string& json, MSBQuickMatchGameState* out_state = nullptr)
{
  HUDEventInfo info;
  GetHUDEventInfo(json, info);

  const int result = allowLoadFromHUDJson(json, AWAY_USER, HOME_USER, false);
  MSBQuickMatchGameState state;
  const bool loaded = LoadStateFromHudJson(json, state, AWAY_USER, HOME_USER);
  // Allowing a HUD has to mean it loads: netplay only finds out from allowLoadFromHUDJson.
  EXPECT_TRUE(result != 0 || loaded);
  if (loaded)
    MSBQuickMatchCodeBuilder::MSB_GenerateQuickMatchSetupGeckoCode(state);
  if (out_state)
    *out_state = state;
  return result;
}

void ExpectRejected(const std::string& json)
{
  HUDEventInfo info;
  MSBQuickMatchGameState state;
  EXPECT_FALSE(LoadStateFromHudJson(json, state, AWAY_USER, HOME_USER)) << json.substr(0, 200);
  EXPECT_EQ(allowLoadFromHUDJson(json, AWAY_USER, HOME_USER, false), 2) << json.substr(0, 200);
}

// A field in the fixture, by the path to it.
HudNode* Field(HudNode& hud, const std::vector<std::string>& path)
{
  HudNode* node = &hud;
  for (const std::string& key : path)
    node = node->Find(key);
  return node;
}

using Path = std::vector<std::string>;

// Every field the loader reads, whatever HUD it is in.
const std::vector<Path> READ_NUMBERS = {
    {"StadiumID"},
    {"Innings Selected"},
    {"Star Skills On"},
    {"Mercy On"},
    {"Away Logo"},
    {"Home Logo"},
    {"Inning"},
    {"Half Inning"},
    {"Away Score"},
    {"Home Score"},
    {"Balls"},
    {"Strikes"},
    {"Outs"},
    {"Star Chance"},
    {"Away Stars"},
    {"Home Stars"},
    {"Pitcher Stamina"},
    {"Pitcher Roster Loc"},
    {"Away Batter Roster Loc"},
    {"Home Batter Roster Loc"},
    {"Away Roster 0", "CharID"},
    {"Away Roster 0", "Fielding Position"},
    {"Away Roster 0", "Batting Hand"},
    {"Away Roster 0", "Fielding Hand"},
    {"Away Roster 0", "Superstar"},
    {"Away Roster 0", "Captain"},
    {"Away Roster 0", "Defensive Stats", "Stamina"},
    {"Home Roster 8", "CharID"},
    {"Home Roster 8", "Fielding Position"},
    {"Home Roster 8", "Batting Hand"},
    {"Home Roster 8", "Fielding Hand"},
    {"Home Roster 8", "Superstar"},
    {"Home Roster 8", "Captain"},
    {"Home Roster 8", "Defensive Stats", "Stamina"},
    {"Runner 1B", "Runner Roster Loc"},
    {"Runner 1B", "Runner Char Id"},
    {"Runner 3B", "Runner Roster Loc"},
    {"Runner 3B", "Runner Char Id"},
};

// Calls visit once per edit of the tree: each object field removed, and each field or array entry
// replaced with each of replacements. Puts the tree back afterwards.
void ForEachEdit(HudNode& node, const std::vector<std::string>& replacements,
                 const std::function<void(const std::string&)>& visit)
{
  const auto replace_with_each = [&](HudNode& child, const std::string& name) {
    const HudNode saved = child;
    for (const std::string& replacement : replacements)
    {
      child = Raw(replacement);
      visit(name + " = " + replacement);
    }
    child = saved;
    ForEachEdit(child, replacements, visit);
  };

  for (size_t i = 0; i < node.members.size(); i++)
  {
    const auto saved = node.members[i];
    node.members.erase(node.members.begin() + i);
    visit("remove " + saved.first);
    node.members.insert(node.members.begin() + i, saved);
    replace_with_each(node.members[i].second, saved.first);
  }
  for (size_t i = 0; i < node.items.size(); i++)
    replace_with_each(node.items[i], "item " + std::to_string(i));
}
}  // namespace

TEST(MSBHUDStateLoader, ValidHudLoadsAsAwayPlayer)
{
  const std::string json = MakeHud().Serialize();

  EXPECT_EQ(allowLoadFromHUDJson(json, AWAY_USER, HOME_USER, false), 0);
  MSBQuickMatchGameState state;
  ASSERT_TRUE(LoadStateFromHudJson(json, state, AWAY_USER, HOME_USER));

  for (int roster = 0; roster < 9; roster++)
  {
    const int away_position = FixturePosition(0, roster);
    const int home_position = FixturePosition(1, roster);
    EXPECT_EQ(state.rosterCharP1BySlot[roster], FixtureChar(0, roster));
    EXPECT_EQ(state.rosterCharP2BySlot[roster], FixtureChar(1, roster));
    EXPECT_EQ(state.positionByRosterSlotP1[roster], away_position);
    EXPECT_EQ(state.positionByRosterSlotP2[roster], home_position);
    EXPECT_EQ(state.charactersP1ByPosition[away_position], FixtureChar(0, roster));
    EXPECT_EQ(state.charactersP2ByPosition[home_position], FixtureChar(1, roster));
    EXPECT_EQ(state.fieldingHandP1ByPosition[away_position], roster % 2);
    EXPECT_EQ(state.battingHandP1ByPosition[away_position], (roster + 1) % 2);
    EXPECT_EQ(state.superstarP1ByPosition[away_position], roster == 6 ? 1 : 0);
  }

  EXPECT_EQ(state.captainCharacterP1, static_cast<uint32_t>(FixtureChar(0, CAPTAIN_ROSTER)));
  EXPECT_EQ(state.captainPositionP1, FixturePosition(0, CAPTAIN_ROSTER));
  EXPECT_EQ(state.captainCharacterP2, static_cast<uint32_t>(FixtureChar(1, CAPTAIN_ROSTER)));
  EXPECT_EQ(state.captainPositionP2, FixturePosition(1, CAPTAIN_ROSTER));

  EXPECT_EQ(state.logoP1, 12u);
  EXPECT_EQ(state.logoP2, 34u);
  EXPECT_EQ(state.stadium, 1);  // Peach Garden's menu index
  EXPECT_EQ(state.starSkills, 1);
  EXPECT_EQ(state.inningsSelected, 9);
  EXPECT_EQ(state.mercy, 0);
  EXPECT_EQ(state.isCpuMatch, 0);

  EXPECT_EQ(state.inning, 3u);
  EXPECT_EQ(state.halfInning, 1);
  EXPECT_EQ(state.battingTeam, 1u);
  EXPECT_EQ(state.fieldingTeam, 0u);
  EXPECT_EQ(state.firstBatter, 1);
  EXPECT_EQ(state.awayScore, 4);
  EXPECT_EQ(state.homeScore, 2);
  EXPECT_EQ(state.awayInningScores[1], 3);
  EXPECT_EQ(state.homeInningScores[0], 2);
  EXPECT_FALSE(state.awayInningScores[3].has_value());
  EXPECT_EQ(state.balls, 2u);
  EXPECT_EQ(state.strikes, 1u);
  EXPECT_EQ(state.outs, 1u);
  EXPECT_EQ(state.p1TeamStars, 3);
  EXPECT_EQ(state.p2TeamStars, 5);
  EXPECT_EQ(state.isStarChance, 0);

  EXPECT_EQ(state.awayBatterRosterLoc, 4);
  EXPECT_EQ(state.homeBatterRosterLoc, 5);
  for (int i = 0; i < 9; i++)
  {
    EXPECT_EQ(state.awayPositionByBattingOrder[i], static_cast<uint32_t>(FixturePosition(0, (i + 4) % 9)));
    EXPECT_EQ(state.homePositionByBattingOrder[i], static_cast<uint32_t>(FixturePosition(1, (i + 5) % 9)));
  }

  EXPECT_EQ(state.runnerRosterSpot[0], 4);
  EXPECT_EQ(state.runnerCharacterID[0], FixtureChar(1, 4));
  EXPECT_FALSE(state.runnerRosterSpot[1].has_value());
  EXPECT_EQ(state.runnerRosterSpot[2], 2);

  // Bottom of the inning, so home bats and away (P1) pitches, with the pre-play stamina.
  EXPECT_EQ(state.pitcherStaminaP1[0], FixtureStamina(0, 0));
  EXPECT_EQ(state.pitcherStaminaP1[1], 77);
  EXPECT_EQ(state.pitcherStaminaP2[1], FixtureStamina(1, 1));

  EXPECT_FALSE(MSBQuickMatchCodeBuilder::MSB_GenerateQuickMatchSetupGeckoCode(state).empty());
}

TEST(MSBHUDStateLoader, ValidHudLoadsAsHomePlayer)
{
  const std::string json = MakeHud().Serialize();

  EXPECT_EQ(allowLoadFromHUDJson(json, HOME_USER, AWAY_USER, false), 0);
  MSBQuickMatchGameState state;
  ASSERT_TRUE(LoadStateFromHudJson(json, state, HOME_USER, AWAY_USER));

  EXPECT_EQ(state.rosterCharP1BySlot[3], FixtureChar(1, 3));
  EXPECT_EQ(state.rosterCharP2BySlot[3], FixtureChar(0, 3));
  EXPECT_EQ(state.logoP1, 34u);
  EXPECT_EQ(state.p1TeamStars, 5);
  EXPECT_EQ(state.firstBatter, 0);
  EXPECT_EQ(state.pitcherStaminaP2[1], 77);
}

TEST(MSBHUDStateLoader, ValidHudEventInfo)
{
  HUDEventInfo info;
  ASSERT_TRUE(GetHUDEventInfo(MakeHud().Serialize(), info));
  EXPECT_EQ(info.eventNum, "12b");
  EXPECT_EQ(info.eventOrder, 25u);
  EXPECT_EQ(info.gameId, "1a2b3c4d");
}

TEST(MSBHUDStateLoader, ExistingFailureCodesAreKept)
{
  HudNode hud = MakeHud();
  EXPECT_EQ(allowLoadFromHUDJson(hud.Serialize(), "SomeoneElse", HOME_USER, false), 4);
  EXPECT_EQ(allowLoadFromHUDJson(hud.Serialize(), AWAY_USER, "SomeoneElse", false), 4);
  EXPECT_EQ(allowLoadFromHUDJson(hud.Serialize(), "", HOME_USER, false), 4);

  HUDValidationDetails details;
  hud.Set("TagSetID", Num(5));
  EXPECT_EQ(allowLoadFromHUDJson(hud.Serialize(), AWAY_USER, HOME_USER, false, &details), 3);
  EXPECT_EQ(details.hudTagSetId, 5);

  EXPECT_EQ(allowLoadFromHUD("Z:/does/not/exist/hud.json", AWAY_USER, HOME_USER, false), 2);
}

TEST(MSBHUDStateLoader, InvalidJsonIsRejected)
{
  const std::string valid = MakeHud().Serialize();
  const std::vector<std::string> bad_json = {
      "",
      "not json",
      "{",
      "{\"Away Player\":",
      "{\"Away Player\" \"AwayUser\"}",
      "{\"Away Inning Scores\": [1, 2, ]}",
      valid.substr(0, valid.size() / 2),
  };
  for (const std::string& json : bad_json)
  {
    ExpectRejected(json);
    HUDEventInfo info;
    EXPECT_FALSE(GetHUDEventInfo(json, info));
  }
}

TEST(MSBHUDStateLoader, NumberTooBigForADoubleIsRejected)
{
  // picojson::value(double) aborts on a non-finite number, so these must not reach it.
  for (const std::string json :
       {"1e999", "-1e999", "[1e999]", "{\"Unread Field\": 1e999}", "{\"a\": {\"b\": [0, -1e999]}}",
        "{\"Event Num\": \"1a\", \"x\": 1e999}"})
  {
    ExpectRejected(json);
    HUDEventInfo info;
    EXPECT_FALSE(GetHUDEventInfo(json, info));
  }

  HudNode hud = MakeHud();
  hud.Set("Inning", Raw("1e999"));
  ExpectRejected(hud.Serialize());
}

TEST(MSBHUDStateLoader, DeepNestingIsRejected)
{
  constexpr size_t DEPTH = 500000;

  std::string arrays(DEPTH, '[');
  ExpectRejected(arrays);
  ExpectRejected(arrays + std::string(DEPTH, ']'));

  std::string objects;
  objects.reserve(DEPTH * 5);
  for (size_t i = 0; i < DEPTH; i++)
    objects += "{\"a\":";
  ExpectRejected(objects);

  HudNode hud = MakeHud();
  hud.Set("Unread Field", Raw(arrays));
  ExpectRejected(hud.Serialize());
}

TEST(MSBHUDStateLoader, NonObjectRootIsRejected)
{
  for (const std::string json : {"[]", "1", "\"hud\"", "null", "true", "[{\"Away Player\":\"a\"}]"})
  {
    ExpectRejected(json);
    HUDEventInfo info;
    EXPECT_FALSE(GetHUDEventInfo(json, info));
  }
}

TEST(MSBHUDStateLoader, EmptyHudIsRejected)
{
  // getHUDJSON writes this before the first inning.
  HUDEventInfo info;
  EXPECT_FALSE(GetHUDEventInfo("{}", info));
  MSBQuickMatchGameState state;
  EXPECT_FALSE(LoadStateFromHudJson("{}", state, AWAY_USER, HOME_USER));
  EXPECT_EQ(allowLoadFromHUDJson("{}", AWAY_USER, HOME_USER, false), 4);
}

TEST(MSBHUDStateLoader, EventNumMustBeAStringStartingWithDigits)
{
  HUDEventInfo info;
  HudNode hud = MakeHud();

  hud.Remove("Event Num");
  EXPECT_FALSE(GetHUDEventInfo(hud.Serialize(), info));
  for (const char* bad : {"12", "null", "true", "[]", "{}", "\"\"", "\"b12\""})
  {
    hud.Set("Event Num", Raw(bad));
    EXPECT_FALSE(GetHUDEventInfo(hud.Serialize(), info)) << bad;
  }
}

TEST(MSBHUDStateLoader, WrongTypesAreRejected)
{
  for (const Path& path : READ_NUMBERS)
  {
    for (const char* bad : {"null", "true", "\"7\"", "[]", "{}"})
    {
      HudNode hud = MakeHud();
      *Field(hud, path) = Raw(bad);
      ExpectRejected(hud.Serialize());
    }
  }

  const std::vector<std::pair<Path, const char*>> other_fields = {
      {{"Away Player"}, "1"},
      {{"Home Player"}, "null"},
      {{"Away Roster 4"}, "1"},
      {{"Away Roster 4"}, "[]"},
      {{"Home Roster 4"}, "\"roster\""},
      {{"Runner 1B"}, "1"},
      {{"Runner 2B"}, "[]"},
      {{"Away Roster 4", "Defensive Stats"}, "1"},
      {{"Home Roster 4", "Defensive Stats"}, "[]"},
      {{"Away Inning Scores"}, "1"},
      {{"Home Inning Scores"}, "{}"},
  };
  for (const auto& [path, bad] : other_fields)
  {
    HudNode hud = MakeHud();
    hud.Set("Runner 2B", Obj());
    *Field(hud, path) = Raw(bad);
    ExpectRejected(hud.Serialize());
  }

  HudNode hud = MakeHud();
  Field(hud, {"Away Inning Scores"})->items[1] = Str("3");
  ExpectRejected(hud.Serialize());
}

// Only allowLoadFromHUDJson reads this one.
TEST(MSBHUDStateLoader, BadTagSetIdIsRejected)
{
  for (const char* bad : {"\"-1\"", "null", "true", "[]", "{}", "2147483648", "-2147483649", "1e300"})
  {
    HudNode hud = MakeHud();
    hud.Set("TagSetID", Raw(bad));
    EXPECT_EQ(allowLoadFromHUDJson(hud.Serialize(), AWAY_USER, HOME_USER, false), 2) << bad;
  }
}

TEST(MSBHUDStateLoader, ValuesThatDontFitTheirTypeAreRejected)
{
  for (const Path& path : READ_NUMBERS)
  {
    // Captain is only ever compared, never cast.
    if (path.back() == "Captain")
      continue;
    for (const char* bad : {"1e300", "-1e300", "4294967296", "-4294967297"})
    {
      HudNode hud = MakeHud();
      *Field(hud, path) = Raw(bad);
      ExpectRejected(hud.Serialize());
    }
  }

  const std::vector<std::pair<Path, const char*>> small_fields = {
      {{"StadiumID"}, "256"},
      {{"Star Skills On"}, "-1"},
      {{"Away Stars"}, "1000"},
      {{"Half Inning"}, "-1"},
      {{"Away Score"}, "65536"},
      {{"Pitcher Stamina"}, "65536"},
      {{"Pitcher Stamina"}, "-1"},
      {{"Away Roster 0", "CharID"}, "256"},
      {{"Away Roster 0", "Fielding Position"}, "-1"},
      {{"Away Roster 0", "Fielding Position"}, "256"},
      {{"Away Roster 0", "Defensive Stats", "Stamina"}, "65536"},
      {{"Runner 1B", "Runner Roster Loc"}, "65536"},
      {{"Away Batter Roster Loc"}, "2147483647"},
      {{"Home Batter Roster Loc"}, "-2147483648"},
      {{"Inning"}, "-1"},
  };
  for (const auto& [path, bad] : small_fields)
  {
    HudNode hud = MakeHud();
    *Field(hud, path) = Raw(bad);
    ExpectRejected(hud.Serialize());
  }

  HudNode hud = MakeHud();
  Field(hud, {"Home Inning Scores"})->items[2] = Num(70000);
  ExpectRejected(hud.Serialize());
}

TEST(MSBHUDStateLoader, CharIdOutOfRangeIsRejectedOnceItReachesTheBattingOrder)
{
  HudNode hud = MakeHud();
  *Field(hud, {"Away Roster 3", "CharID"}) = Num(0x36);
  ExpectRejected(hud.Serialize());

  *Field(hud, {"Away Roster 3", "CharID"}) = Num(0x35);
  EXPECT_EQ(allowLoadFromHUDJson(hud.Serialize(), AWAY_USER, HOME_USER, false), 0);

  // Off the field the generator clamps it, so this has always loaded.
  *Field(hud, {"Away Roster 3", "CharID"}) = Num(0xFF);
  *Field(hud, {"Away Roster 3", "Fielding Position"}) = Num(0xFF);
  EXPECT_EQ(allowLoadFromHUDJson(hud.Serialize(), AWAY_USER, HOME_USER, false), 0);
}

TEST(MSBHUDStateLoader, RequiredRosterFieldsAreStillRequired)
{
  for (const char* key : {"CharID", "Fielding Position", "Batting Hand", "Fielding Hand", "Superstar"})
  {
    HudNode hud = MakeHud();
    Field(hud, {"Home Roster 5"})->Remove(key);
    ExpectRejected(hud.Serialize());
  }
}

TEST(MSBHUDStateLoader, OptionalFieldsAreStillOptional)
{
  const std::vector<Path> optional_fields = {
      {"GameID"},
      {"TagSetID"},
      {"Loaded from HUD"},
      {"StadiumID"},
      {"Innings Selected"},
      {"First Batting Team"},
      {"Star Skills On"},
      {"Mercy On"},
      {"Away Logo"},
      {"Home Logo"},
      {"Inning"},
      {"Half Inning"},
      {"Away Score"},
      {"Home Score"},
      {"Away Inning Scores"},
      {"Home Inning Scores"},
      {"Balls"},
      {"Strikes"},
      {"Outs"},
      {"Star Chance"},
      {"Away Stars"},
      {"Home Stars"},
      {"Pitcher Stamina"},
      {"Pitcher Roster Loc"},
      {"Away Batter Roster Loc"},
      {"Home Batter Roster Loc"},
      {"Away Roster 3"},
      {"Home Roster 7"},
      {"Runner 1B"},
      {"Previous Event"},
  };
  for (const Path& path : optional_fields)
  {
    HudNode hud = MakeHud();
    hud.Remove(path.back());
    EXPECT_EQ(RunAll(hud.Serialize()), 0) << path.back();
  }

  const std::vector<Path> optional_nested_fields = {
      {"Away Roster 2", "Captain"},
      {"Away Roster 2", "Defensive Stats"},
      {"Away Roster 2", "Offensive Stats"},
      {"Home Roster 2", "Defensive Stats", "Stamina"},
      {"Runner 1B", "Runner Roster Loc"},
      {"Runner 1B", "Runner Char Id"},
  };
  for (const Path& path : optional_nested_fields)
  {
    HudNode hud = MakeHud();
    Path parent = path;
    parent.pop_back();
    Field(hud, parent)->Remove(path.back());
    EXPECT_EQ(RunAll(hud.Serialize()), 0) << path.back();
  }

  // "{}" with the player names, the least a loadable HUD can hold.
  EXPECT_EQ(RunAll(Obj({{"Away Player", Str(AWAY_USER)}, {"Home Player", Str(HOME_USER)}}).Serialize()),
            0);
}

TEST(MSBHUDStateLoader, MissingPlayerNameIsAMismatch)
{
  HudNode hud = MakeHud();
  hud.Remove("Away Player");
  EXPECT_EQ(allowLoadFromHUDJson(hud.Serialize(), AWAY_USER, HOME_USER, false), 4);
  EXPECT_EQ(allowLoadFromHUDJson(hud.Serialize(), HOME_USER, AWAY_USER, false), 4);
}

// Everything here worked before the loader had any checks, because the odd value is never read or
// is read harmlessly. A valid-looking HUD must not start failing for it.
TEST(MSBHUDStateLoader, FieldsThatWereNeverReadStayUnchecked)
{
  const auto expect_loads = [](HudNode& hud) { EXPECT_EQ(RunAll(hud.Serialize()), 0); };

  {
    // The stat tracker's fielder map starts every position at 0xFF.
    HudNode hud = MakeHud();
    *Field(hud, {"Away Roster 1", "Fielding Position"}) = Num(0xFF);
    *Field(hud, {"Away Roster 1", "Batting Hand"}) = Str("Right");
    *Field(hud, {"Away Roster 1", "Fielding Hand"}) = Raw("null");
    *Field(hud, {"Away Roster 1", "Superstar"}) = Raw("[]");
    expect_loads(hud);
  }
  {
    // A logo is only read when both are there.
    HudNode hud = MakeHud();
    hud.Remove("Home Logo");
    hud.Set("Away Logo", Str("Mario Fireballs"));
    expect_loads(hud);
  }
  {
    HudNode hud = MakeHud();
    hud.Remove("Home Batter Roster Loc");
    hud.Set("Away Batter Roster Loc", Raw("[]"));
    expect_loads(hud);
  }
  {
    // The pitcher's pre-play stamina needs both fields and a half inning.
    HudNode hud = MakeHud();
    hud.Remove("Pitcher Roster Loc");
    hud.Set("Pitcher Stamina", Str("full"));
    expect_loads(hud);

    hud = MakeHud();
    hud.Remove("Half Inning");
    hud.Set("Pitcher Stamina", Str("full"));
    hud.Set("Pitcher Roster Loc", Raw("null"));
    expect_loads(hud);
  }
  {
    // Only 18 innings are read.
    HudNode hud = MakeHud();
    auto& scores = Field(hud, {"Away Inning Scores"})->items;
    scores.assign(18, Num(0));
    scores.push_back(Str("not read"));
    expect_loads(hud);
  }
  {
    // Fields the loader doesn't use at all.
    HudNode hud = MakeHud();
    hud.Set("Loaded from HUD", Str("yes"));
    hud.Set("Previous Event", Raw("7"));
    hud.Set("Runner Batter", Raw("7"));
    *Field(hud, {"Away Roster 0", "Offensive Stats"}) = Raw("7");
    expect_loads(hud);
  }
  {
    // Out of range but always tolerated: the value is skipped or clamped later.
    HudNode hud = MakeHud();
    hud.Set("Pitcher Roster Loc", Num(0xFF));
    hud.Set("Away Batter Roster Loc", Num(-3));
    hud.Set("Home Batter Roster Loc", Num(40));
    hud.Set("Half Inning", Num(2));
    hud.Set("StadiumID", Num(200));
    hud.Set("Away Logo", Num(4000));
    expect_loads(hud);
  }
  {
    HudNode hud = MakeHud();
    hud.Set("Inning", Raw("3.9"));
    MSBQuickMatchGameState state;
    EXPECT_EQ(RunAll(hud.Serialize(), &state), 0);
    EXPECT_EQ(state.inning, 3u);

    // Truncates to 0, which fits.
    hud.Set("Star Chance", Raw("-0.5"));
    EXPECT_EQ(RunAll(hud.Serialize(), &state), 0);
    EXPECT_EQ(state.isStarChance, 0);
  }
  {
    // Wrong type here was always just "no game id".
    HudNode hud = MakeHud();
    hud.Set("GameID", Num(1234));
    expect_loads(hud);
    HUDEventInfo info;
    EXPECT_TRUE(GetHUDEventInfo(hud.Serialize(), info));
    EXPECT_EQ(info.gameId, "");
  }
}

TEST(MSBHUDStateLoader, CaptainMeansExactlyOne)
{
  HudNode hud = MakeHud();
  *Field(hud, {"Away Roster 2", "Captain"}) = Raw("1e300");
  MSBQuickMatchGameState state;
  ASSERT_TRUE(LoadStateFromHudJson(hud.Serialize(), state, AWAY_USER, HOME_USER));
  EXPECT_FALSE(state.captainCharacterP1.has_value());
}

// A negative batter location builds keys like "Away Roster -1", which the first pass over the
// rosters never looked at.
TEST(MSBHUDStateLoader, BatterRosterLocCanNotReachAnUncheckedRoster)
{
  HudNode hud = MakeHud();
  hud.Set("Away Batter Roster Loc", Num(-1));
  hud.Set("Away Roster -1", Obj());
  ExpectRejected(hud.Serialize());

  hud.Set("Away Roster -1", Raw("1"));
  ExpectRejected(hud.Serialize());
}

// Every field of a valid HUD, removed or given every kind of bad value. The test is that this
// finishes: any unchecked read aborts the process.
TEST(MSBHUDStateLoader, EveryEditOfAValidHudIsHandled)
{
  const std::vector<std::string> replacements = {
      "null", "true",  "\"text\"",  "[]",     "{}",           "[1,[2]]", "{\"a\":1}",
      "1.5",  "-1",    "0",         "256",    "70000",        "4294967296",
      "1e300", "-1e300", "2147483647", "-2147483648",
  };

  HudNode hud = MakeHud();
  size_t edits = 0;
  size_t accepted = 0;
  ForEachEdit(hud, replacements, [&](const std::string& edit) {
    SCOPED_TRACE(edit);
    const int result = RunAll(hud.Serialize());
    EXPECT_TRUE(result == 0 || result == 2 || result == 3 || result == 4);
    edits++;
    if (result == 0)
      accepted++;
  });

  EXPECT_EQ(hud.Serialize(), MakeHud().Serialize());
  EXPECT_GT(edits, 10000u);
  EXPECT_GT(accepted, 0u);
  EXPECT_LT(accepted, edits);
}
