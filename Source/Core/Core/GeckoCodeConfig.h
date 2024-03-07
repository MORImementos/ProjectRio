// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Core/GeckoCode.h"

namespace Common
{
class IniFile;
}

namespace Gecko
{
std::vector<GeckoCode> LoadCodes(const Common::IniFile& globalIni, const Common::IniFile& localIni, std::string gameId, bool is_netplay);
std::vector<GeckoCode> DownloadCodes(std::string gametdb_id, bool* succeeded,
                                     bool use_https = true);
void SaveCodes(Common::IniFile& inifile, const std::vector<GeckoCode>& gcodes);

std::optional<GeckoCode::Code> DeserializeLine(const std::string& line);
void ReadLines(std::vector<GeckoCode>& gcodes, std::vector<std::string>& lines, bool user_defined);

static bool isDisableReplays = false;
void setDisableReplays(bool disable);
static bool isNightStadium = false;
void setNightStadium(bool is_night);
//static bool isTrainingMode = false;

const std::string MSSB_DisableReplays = R"(
+$Disable Replays [LittleCoaks]
206bb214 38000001
046bb214 38000000
E2000001 00000000
*Disables replays

)";

const std::string MSSB_NightStadium = R"(
+$Night Mario Stadium [LittleCoaks]
20650678 98030058
C2650678 00000004
98030058 89240009
2C090000 4082000C
3A400001 9A44000A
60000000 00000000
E2000001 00000000
*Mario Stadium is given the night-time effect as seen in Bom-omb Derby

)";

const std::string MSSB_BuiltInGeckoCodes = R"(
+$Bat Sound On Game Start [LittleCoaks]
04042cd0 386001bb
*An unused bat sound effect plays when starting a match

+$Clear Hit Result [PeacockSlayer]
206BBF88 99090037
C26BBF88 00000003
99090037 3EA08089
62B53BAA 99150000
3AA00000 00000000
E2000001 00000000
*Clears the hit result from the ram for the stat tracker

+$Store Random Batting Ints [Roeming]
20651E68 98040091
C2651E68 00000006
98040091 3CA08089
38A52684 3CC0802F
38C6C010 80850000
B0860000 80850004
B0860002 A0850018
B0860004 00000000
E2000001 00000000
*Stores the rng values used for trajectory

+$Remove Baserunner Lockout [nuche17, LittleCoaks]
C26C9D78 00000008
3DC0800F 39CE877C
A9CE0000 2C0E0005
4182000C 7C0802A6
48000020 A81D0006
3DC08089 61CE2701
89CE0000 2C0E0000
40820008 38000001
60000000 00000000
*Removes the turnaround lockout window while baserunning

+$Manage Port Info [LittleCoaks]
C2042CD8 0000000B # Get Port Info
9421FFB0 BDC10008
3DE0802E 61EFBF91
3E008089 62102ACA
8A100000 2C100001
4182000C 3A000001
48000008 3A000005
9A0F0000 3E00800E
6210874D 8A100000
3A100001 9A0F0001
B9C10008 38210050
38A0003F 00000000
206706B8 3C608089 # Store Port Info
C26706B8 00000009
3FE08089 63FF3928
7C04F800 3FE0802E
63FFBF91 41820018
887F0000 987F0004
887F0001 987F0003
48000014 887F0001
987F0004 887F0000
987F0003 3C608089
60000000 00000000
E2000001 00000000
2063F14C 38600000 # Clear Port Info
C263F14C 00000004
38600000 3CA0802E
60A5BF90 98650001
B0650002 B0650004
60000000 00000000
E2000001 00000000
*Stores port info for auto golf mode to static mem

+$Enable Controller Rumble [LittleCoaks]
00366177 00000001
*Forces rumble on - people can turn off rumble with controller configs

+$Remember Who Quit [LittleCoaks]
206ED700 B08300FE # Fielder Quit
C26ED700 00000003
B08300FE 3E80802E
6294BF93 8AB40001
9AB40000 00000000
E2000001 00000000
206EDF88 B08300FE # Batter Quit
C26EDF88 00000003
B08300FE 3E80802E
6294BF93 8AB40002
9AB40000 00000000
E2000001 00000000
*Stores the port who paused and returned to main menu

+$Control Stick Overrides DPad [LittleCoaks]
C20A59FC 00000008
7C0E0378 55CEC63E
2C0E0052 40810024
2C0E00AE 4080001C
7C0E0378 55CE063E
2C0E00AE 4080000C
2C0E0052 41810008
54000416 900501C0
60000000 00000000
*If both held, the input from the control stick takes priority

+$Game ID [LittleCoaks]
C2042CCC 00000003 # Generate GameID when "Start Game" is pressed
3C80802E 6084BF8C
7C6C42E6 90640000
3C80800F 00000000
206ED704 981F01D2 # Clear Game ID when exiting mid-game
C26ED704 00000003
981F01D2 3D00802E
6108BF8C 38000000
90080000 00000000
C26EDF8C 00000003
981F01D2 3D00802E
6108BF8C 38000000
90080000 00000000
E2000001 00000000
2069AB2C 98050125 # Clear Game ID when returning to main menu after game ends
C269AB2C 00000003
98050125 3E40802E
6252BF8C 38600000
90720000 00000000
E2000001 00000000
*Manages Game ID for stat files

+$Fix Random Captain [LittleCoaks]
2063F7C4 90040000
C263F7C4 00000002
90040000 90040330
60000000 00000000
E2000001 00000000
*Properly seeds the rng seed which determines the random captain

+$Unlock Everything [PeacockSlayer, LittleCoaks]
000E870E 00000002
000E8710 00050003
000E8716 00050001
00361680 00290001
003616B0 00000001
00361B20 00350001
00361C04 00030001
00361C14 00010001
*All mingames, stadiums, characters, and star characters are unlocked.

+$Boot to Main Menu [LittleCoaks]
280e877d 00000000
0463f964 38600005
e2000001 00000000
*Skips the opening cutscenes and stars the game on the main menu. Does not load in memory card.

+$Default Mercy On [LittleCoaks]
040498DC 9867003F
*Mercy defaults to on when starting a game

+$Captain Swap [nuche]
C264F674 0000005A
3D60800F 398B877C
A54C0000 280A0004
41820014 38C4298C
38A0000D 38830910
480002A8 57C004E7
57C3043E 4182029C
3D608075 398B0C48
7D6CDA14 8D4B0045
280A0000 4082023C
3D608075 398B0C48
7D6CDA14 8D4B0041
280A0000 40820224
3D608075 398B0C48
1D5B0004 7D6C5214
850B0000 2C080009
40800208 3D60803C
398B6738 1D5B0009
7D6C5214 39400009
7D4903A6 38E00000
894B0000 7C085000
40820008 48000014
396B0001 38E70001
4200FFE8 480001CC
60000000 3D60803C
398B6726 1D5B0009
7D6C5214 7D4B3A14
892A0000 3D608011
398B8ED0 3960000C
7D6903A6 896C0000
7C095800 40820008
48000010 398C0001
4200FFEC 48000184
60000000 3D608035
398B3080 1D5B0004
7D6A6214 912B0000
3D60803C 398B6726
1D5B0009 7D6C5214
898B0000 992B0000
7D4B3A14 998A0000
3D60803C 398B6738
1D5B0009 7D6C5214
898B0000 990B0000
7D4B3A14 998A0000
3D608035 396BE9DB
1D8900A0 7D6B6214
39800008 7D8903A6
3D80803C 398C6727
1D5B0009 7D8C5214
3D20803C 3929674B
7D295214 88EC0000
2C0700FF 41820010
7CE75A14 89070000
99090000 39290001
398C0001 4200FFE0
7C0802A6 90010004
9421FF00 BC610008
7F63DB78 3D808006
618C78CC 7D8903A6
4E800421 B8610008
80010104 38210100
7C0803A6 7C0802A6
90010004 9421FF00
BC610008 3C608035
606330EC 80630000
7F64DB78 3D80806B
618C4C78 7D8903A6
4E800421 B8610008
80010104 38210100
7C0803A6 7C0802A6
90010004 9421FF00
BC610008 3C608035
606330EC 80630000
3880006E 38A0001E
3D808004 618C2DA8
7D8903A6 4E800421
B8610008 80010104
38210100 7C0803A6
7C0802A6 90010004
9421FF00 BC610008
386001BC 48000018
7C0802A6 90010004
9421FF00 BC610008
386001BA 3C80800E
6084FBA4 80840000
38A0003F 38C00000
3D80800C 618C836C
7D8903A6 4E800421
B8610008 80010104
38210100 7C0803A6
60000000 00000000
06515E52 00000040
002F004F 00420050
00504002 80570032
0051003E 004F0051
80584002 0051004C
40020050 0054003E
004D4002 0040003E
004D0051 003E0046
004B000D 40024000
*Press "Start" over a character to change your captain while mid-draft

+$Checksum [LittleCoaks]
C200928C 0000004E
9421FFB0 BDC10008
39C00000 3A000000
3DE0800E 61EF877E
A1EF0000 7DCF7214
3DE0800E 61EF8782
A1EF0000 7DCF7214
3DE0800E 61EF874C
A1EF0000 7DCF7214
3DE0803C 61EF6726
7DF078AE 7DCF7214
7DD07214 3DE08035
61EF323B 7DF078AE
7DCF7214 7DD07214
3A100001 2C100012
4180FFD0 3DE08089
61EF2AAA 89EF0000
7DCF7214 3DE08089
61EF2AAB 89EF0000
7DCF7214 3DE08089
61EF09A1 89EF0000
7DCF7214 3DE08089
61EF2857 89EF0000
7DCF7214 3DE08036
61EFF3A9 89EF0000
7DCF7214 3DE08089
61EF09AA 89EF0000
7DCF7214 3DE08087
61EF2540 89EF0000
7DCF7214 3DE08089
61EF0971 89EF0000
7DCF7214 3DE08089
61EF28A3 89EF0000
7DCF7214 3DE08089
61EF294D 89EF0000
7DCF7214 3DE08089
61EF296F 89EF0000
7DCF7214 3DE08089
61EF296B 89EF0000
7DCF7214 3DE08089
61EF2973 89EF0000
7DCF7214 3DE08089
61EF2AD6 89EF0000
7DCF7214 3DE08089
61EF2AD7 89EF0000
7DCF7214 3DE08089
61EF2AD8 89EF0000
7DCF7214 3DE08089
61EF09BA 89EF0000
7DCF7214 3DE08088
61EFF09D 89EF0000
7DCF7214 3DE08088
61EFF1F1 89EF0000
7DCF7214 3DE08088
61EFF345 89EF0000
7DCF7214 3DE08089
61EF38AD 89EF0000
7DCF7214 3DE08089
61EF09A3 89EF0000
7DCF7214 3DE08089
61EF3BAA 89EF0000
7DCF7214 3DE08089
61EF28A4 89EF0000
7DCF7214 3DE08089
61EF28CA 89EF0000
7DCF7214 3DE08089
61EF0971 89EF0000
7DCF7214 3DE08089
61EF0AD9 89EF0000
7DCF7214 3DE08089
61EF0B38 81EF0000
7DCF7214 3DE08089
61EF0B3C 81EF0000
7DCF7214 3DE08089
61EF0B40 81EF0000
7DCF7214 3DE0802E
61EFBFB8 91CF0000
B9C10008 38210050
28180000 00000000
*Calculates a checksum for netplay desync detection

)";
}  // namespace Gecko
