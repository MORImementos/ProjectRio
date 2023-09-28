// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/NetplaySettings.h"

#include "Common/Config/Config.h"

namespace Config
{
static constexpr u16 DEFAULT_LISTEN_PORT = 2626;

// Configuration Information

// Main.NetPlay

const Info<std::string> NETPLAY_TRAVERSAL_SERVER{{System::Main, "NetPlay", "TraversalServer"},
                                                 "stun.dolphin-emu.org"};
const Info<u16> NETPLAY_TRAVERSAL_PORT{{System::Main, "NetPlay", "TraversalPort"}, 6262};
const Info<std::string> NETPLAY_TRAVERSAL_CHOICE{{System::Main, "NetPlay", "TraversalChoice"},
                                                 "traversal"};
const Info<std::string> NETPLAY_INDEX_URL{{System::Main, "NetPlay", "IndexServer"},
                                          "https://lobby.dolphin-emu.org"};

const Info<bool> NETPLAY_USE_INDEX{{System::Main, "NetPlay", "UseIndex"}, false};
const Info<std::string> NETPLAY_INDEX_NAME{{System::Main, "NetPlay", "IndexName"}, ""};
const Info<std::string> NETPLAY_INDEX_REGION{{System::Main, "NetPlay", "IndexRegion"}, ""};
const Info<std::string> NETPLAY_INDEX_PASSWORD{{System::Main, "NetPlay", "IndexPassword"}, ""};

const Info<std::string> NETPLAY_HOST_CODE{{System::Main, "NetPlay", "HostCode"}, "00000000"};

const Info<u16> NETPLAY_HOST_PORT{{System::Main, "NetPlay", "HostPort"}, DEFAULT_LISTEN_PORT};
const Info<std::string> NETPLAY_ADDRESS{{System::Main, "NetPlay", "Address"}, "127.0.0.1"};
const Info<u16> NETPLAY_CONNECT_PORT{{System::Main, "NetPlay", "ConnectPort"}, DEFAULT_LISTEN_PORT};
const Info<u16> NETPLAY_LISTEN_PORT{{System::Main, "NetPlay", "ListenPort"}, DEFAULT_LISTEN_PORT};

//const Info<std::string> NETPLAY_NICKNAME{{System::Main, "NetPlay", "Nickname"}, "Player"};
const Info<bool> NETPLAY_USE_UPNP{{System::Main, "NetPlay", "UseUPNP"}, false};

const Info<bool> NETPLAY_ENABLE_QOS{{System::Main, "NetPlay", "EnableQoS"}, true};

const Info<bool> NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT{
    {System::Main, "NetPlay", "EnableChunkedUploadLimit"}, false};
const Info<u32> NETPLAY_CHUNKED_UPLOAD_LIMIT{{System::Main, "NetPlay", "ChunkedUploadLimit"}, 3000};

const Info<u32> NETPLAY_BUFFER_SIZE{{System::Main, "NetPlay", "BufferSize"}, 8};
const Info<u32> NETPLAY_CLIENT_BUFFER_SIZE{{System::Main, "NetPlay", "BufferSizeClient"}, 8};

const Info<bool> NETPLAY_SAVEDATA_LOAD{{System::Main, "NetPlay", "SyncSaves"}, true};
const Info<bool> NETPLAY_SAVEDATA_WRITE{{System::Main, "NetPlay", "WriteSaveData"}, true};
const Info<bool> NETPLAY_SAVEDATA_SYNC_ALL_WII{{System::Main, "NetPlay", "SyncAllWiiSaves"}, false};
const Info<bool> NETPLAY_SYNC_CODES{{System::Main, "NetPlay", "SyncCodes"}, true};
const Info<bool> NETPLAY_RECORD_INPUTS{{System::Main, "NetPlay", "RecordInputs"}, false};
const Info<bool> NETPLAY_STRICT_SETTINGS_SYNC{{System::Main, "NetPlay", "StrictSettingsSync"},
                                              false};
const Info<std::string> NETPLAY_NETWORK_MODE{{System::Main, "NetPlay", "NetworkMode"},
                                             "golf"};
const Info<bool> NETPLAY_SYNC_ALL_WII_SAVES{{System::Main, "NetPlay", "SyncAllWiiSaves"}, false};
const Info<bool> NETPLAY_GOLF_MODE_OVERLAY{{System::Main, "NetPlay", "GolfModeOverlay"}, true};
const Info<bool> NETPLAY_HIDE_REMOTE_GBAS{{System::Main, "NetPlay", "HideRemoteGBAs"}, false};
//const Info<bool> NETPLAY_NIGHT_STADIUM{{System::Main, "NetPlay", "Night Stadium"}, false};
const Info<bool> NETPLAY_DISABLE_MUSIC{{System::Main, "NetPlay", "Disable Music"}, false};
const Info<bool> NETPLAY_HIGHLIGHT_BALL_SHADOW{{System::Main, "NetPlay", "Highlight Ball Shadow"}, false};
//const Info<bool> NETPLAY_NEVER_CULL{{System::Main, "NetPlay", "Never Cull"}, false};

int ONLINE_COUNT = 0;
// get game tags from entry.name string -- each tag seperated by "%%"
// first tag is the name of the lobby, second is ranked, third is superstars
std::vector<std::string> LobbyNameVector(const std::string& name)
{
  std::vector<std::string> game_tags;
  {
    std::string tag_string = name;
    std::string delimiter = "%%";
    size_t pos = 0;
    std::string token;
    while ((pos = tag_string.find(delimiter)) != std::string::npos)
    {
      token = tag_string.substr(0, pos);
      game_tags.push_back(token);
      tag_string.erase(0, pos + delimiter.length());
    }
    game_tags.push_back(tag_string);
  }
  return game_tags;
}

}  // namespace Config
