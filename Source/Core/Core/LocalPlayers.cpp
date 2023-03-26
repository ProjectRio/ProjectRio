

#include "Common/IniFile.h"
#include "Core/LocalPlayers.h"
#include "Common/FileUtil.h"

namespace LocalPlayers
{
// gets a vector of all the local players
std::vector<LocalPlayers::Player> LocalPlayers::GetPlayers()
{
  IniFile local_players_ini;
  local_players_ini.Load(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));

  std::vector<LocalPlayers::Player> players;
  LocalPlayers::Player defaultPlayer{"No Player Selected", "0"};
  players.push_back(defaultPlayer);

  for (const IniFile* ini : {&local_players_ini})
  {
    std::vector<std::string> lines;
    ini->GetLines("Local_Players_List", &lines, false);

    for (auto& line : lines)
    {
      LocalPlayers::Player player = toLocalPlayer(line);
       if (!player.username.empty() && player.username != defaultPlayer.username && player.userid != defaultPlayer.userid)
        players.push_back(player);
    }

  }
  return players;
}

// returns a mak of player keys to usernames
std::map<std::string, std::string> LocalPlayers::GetPlayerMap()
{
  std::map<std::string, std::string> playerMap;
  std::vector<LocalPlayers::Player> playerList = GetPlayers();

  for (Player player : playerList)
  {
    playerMap.emplace(player.userid, player.username);
  }
  return playerMap;
}

// maps the rio key to the index of the player vector
std::map<std::string, int> LocalPlayers::GetPlayerIndexMap()
{
  std::map<std::string, int> playerIndexMap;
  std::vector<LocalPlayers::Player> playerList = GetPlayers();

  for (int i = 0; i < playerList.size(); i++)
  {
    playerIndexMap.emplace(playerList[i].userid, i);
  }

  return playerIndexMap;
}

// returns map of local players set to each port
// NOTE: reads from the file
std::map<int, LocalPlayers::Player> LocalPlayers::GetPortPlayers()
{
  std::map<int, LocalPlayers::Player> LocalPortPlayers;

  IniFile local_players_ini;
  local_players_ini.Load(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));
  IniFile::Section* localplayers = local_players_ini.GetOrCreateSection("Local Players");
  const IniFile::Section::SectionMap portmap = localplayers->GetValues();

  for (const auto& name : portmap)
  {
    if (name.first == "Online Player")
      LocalPortPlayers[0] = toLocalPlayer(name.second);
    if (name.first == "Player 1")
      LocalPortPlayers[1] = toLocalPlayer(name.second);
    if (name.first == "Player 2")
      LocalPortPlayers[2] = toLocalPlayer(name.second);
    if (name.first == "Player 3")
      LocalPortPlayers[3] = toLocalPlayer(name.second);
    if (name.first == "Player 4")
      LocalPortPlayers[4] = toLocalPlayer(name.second);
  }

  return LocalPortPlayers;
}


// converts a string to a LocalPlayers object
LocalPlayers::Player LocalPlayers::toLocalPlayer(std::string playerStr)
{
  LocalPlayers::Player player;

  std::istringstream ss(playerStr);
  ss.imbue(std::locale::classic());

  switch ((playerStr)[0])
  {
  case '+':
    ss.seekg(1, std::ios_base::cur);

    // read the username
    std::getline(ss, player.username,
                 '[');  // stop at [ character (beginning of uid)

    // read the uid
    std::getline(ss, player.userid, ']');
    break;
    break;
  }

  return player;
}

// converts local players to a str
std::string LocalPlayers::Player::LocalPlayerToStr()
{
  std::string title =
      "+" + (this->username == "" ? "No Player Selected" : this->username) +
      "[" + (this->userid == "" ? "0]" : this->userid) + "]";
  return title;
}

std::vector<std::string> LocalPlayers::Player::GetUserInfo(std::string playerStr)
{
  std::string a_username;
  std::string a_userid;
  std::vector<std::string> a_userInfo;

  std::istringstream ss(playerStr);
  ss.imbue(std::locale::classic());

  switch ((playerStr)[0])
  {
  case '+':
    // player = LocalPlayers();
    ss.seekg(1, std::ios_base::cur);

    // read the username
    std::getline(ss, a_username,
                 '[');  // stop at [ character (beginning of uid)

    // read the uid
    std::getline(ss, a_userid, ']');
    break;
    break;
  }
  a_userInfo.push_back(a_username);
  a_userInfo.push_back(a_userid);
  return a_userInfo;
}

std::string LocalPlayers::Player::GetUsername()
{
  return GetUserInfo(this->LocalPlayerToStr())[0];
}

std::string LocalPlayers::Player::GetUserID()
{
  return GetUserInfo(this->LocalPlayerToStr())[1];
}

void LocalPlayers::Player::SetUserInfo(LocalPlayers::Player player)
{
  this->username = player.GetUsername();
  this->userid = player.GetUserID();
}

enum LocalPlayers::AccountValidationType LocalPlayers::Player::ValidateAccount(Common::HttpRequest &m_http)
{
  AccountValidationType validationType;
  std::string url = "https://api.projectrio.app/validate_user_from_client/?username=" +
                    this->username + "&rio_key=" + this->userid;

  const Common::HttpRequest::Response response = m_http.Get(url/*, {}, Common::HttpRequest::AllowedReturnCodes::All*/);

  if (!response)
  {
    validationType = Invalid;
  }
  else
  {
    validationType = Valid;
  }

  return validationType;
}

}  // namespace LocalPlayers
