/************************************************************************
Tethealla Login Server
Copyright (C) 2008  Terry Chatman Jr.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
************************************************************************/


// Notes:
//
// - Limit to 40 guild cards for now.
//

//数据库相关
#define SQL
#define NO_CONNECT_TEST //会在连接舰船的时候 给舰船发送一个0字节的心跳包

#include <windows.h>
#include "targetver.h"
#include <stdio.h>
#include <tchar.h>
#include <mbstring.h>
#include <locale.h> //12.22
#include <time.h>
#include <math.h>

#ifndef NO_SQL
#include <mysql.h>
#endif
#include <md5.h>

#include "resource.h"
#include "pso_crypt.h"
#include "bbtable.h"
#include "prs.cpp"
#include "login_server.h"
#include "def_defines.h"
#include "def_funcs.h"
#include "def_packets.h"
#include "def_struct.h"


//#define SERVER_VERSION "0.050"

//const wchar_t Message03[22] = { L"Tethealla 登录服务器 v.047" };

//const char *PSO_CLIENT_VER_STRING = "TethVer12513";

/* String sent to server to retrieve IP address. */

char* HTTP_REQ = "GET http://www.pioneer2.net/remote.php HTTP/1.0\r\n\r\n\r\n";
char error_buffer[1024] = { 0 };

/* Populated by load_config_file(): */

char mySQL_Host[255] = { 0 };
char mySQL_Username[255] = { 0 };
char mySQL_Password[255] = { 0 };
char mySQL_Database[255] = { 0 };
unsigned int mySQL_Port;
unsigned char serverIP[4];
unsigned short serverPort;
int override_on = 1;
unsigned char overrideIP[4];
unsigned short serverMaxConnections;
unsigned short serverMaxShips;
unsigned serverNumConnections = 0;
unsigned serverConnectionList[LOGIN_COMPILED_MAX_CONNECTIONS];
unsigned serverNumShips = 0;
unsigned serverShipList[SHIP_COMPILED_MAX_CONNECTIONS];
unsigned quest_numallows;
unsigned* quest_allow;
unsigned max_ship_keys = 0;

/* Rare table structure */

unsigned rt_tables_ep1[0x200 * 10 * 4] = { 0 };
unsigned rt_tables_ep2[0x200 * 10 * 4] = { 0 };
unsigned rt_tables_ep4[0x200 * 10 * 4] = { 0 };

unsigned mob_rate[8]; // rare appearance rate

char Welcome_Message[255] = { 0 };
time_t servertime;

#ifndef NO_SQL

MYSQL* myData;
char myQuery[0x10000] = { 0 };
MYSQL_ROW myRow;
MYSQL_RES* myResult;

#endif

fd_set ReadFDs, WriteFDs, ExceptFDs;

DRESSFLAG dress_flags[MAX_DRESS_FLAGS];
unsigned char dp[TCP_BUFFER_SIZE * 4];
unsigned char tmprcv[PACKET_BUFFER_SIZE];
char Packet1AData[TCP_BUFFER_SIZE];
char PacketEEData[TCP_BUFFER_SIZE];
unsigned char PacketEB01[0x4C8 * 2] = { 0 };
unsigned char PacketEB02[MAX_EB02] = { 0 };
unsigned PacketEB01_Total;
unsigned PacketEB02_Total;

unsigned keys_in_use[SHIP_COMPILED_MAX_CONNECTIONS + 1] = { 0 };

#ifdef NO_SQL

typedef struct st_bank_file {
	unsigned guildcard;
	BANK common_bank;
} L_BANK_DATA;

typedef struct st_account {
	char username[18];
	char password[33];
	char email[255];
	unsigned regtime;
	char lastip[16];
	long long lasthwinfo;
	unsigned guildcard;
	int isgm;
	int isbanned;
	int islogged;
	int isactive;
	int teamid;
	int privlevel;
	unsigned char lastchar[24];
} L_ACCOUNT_DATA;


typedef struct st_character {
	unsigned guildcard;
	int slot;
	CHARDATA data;
	MINICHAR header;
} L_CHARACTER_DATA;


typedef struct st_guild {
	unsigned accountid;
	unsigned friendid;
	char friendname[24];
	char friendtext[176];
	unsigned short reserved;
	unsigned short sectionid;
	unsigned short pclass;
	unsigned short comment[45];
	int priority;
} L_GUILD_DATA;


typedef struct st_hwbans {
	unsigned guildcard;
	long long hwinfo;
} L_HW_BANS;


typedef struct st_ipbans {
	unsigned char ipinfo;
} L_IP_BANS;


typedef struct st_key_data {
	unsigned guildcard;
	unsigned char controls[420];
} L_KEY_DATA;


typedef struct st_security_data {
	unsigned guildcard;
	unsigned thirtytwo;
	long long sixtyfour;
	int slotnum;
	int isgm;
} L_SECURITY_DATA;


typedef struct st_ship_data {
	unsigned char rc4key[128];
	unsigned idx;
} L_SHIP_DATA;


typedef struct st_team_data {
	unsigned short name[12];
	unsigned owner;
	unsigned char flag[2048];
	unsigned teamid;
} L_TEAM_DATA;

// Oh brother :D

L_BANK_DATA* bank_data[MAX_ACCOUNTS];
L_ACCOUNT_DATA* account_data[MAX_ACCOUNTS];
L_CHARACTER_DATA* character_data[MAX_ACCOUNTS * 4];
L_GUILD_DATA* guild_data[MAX_ACCOUNTS * 40];
L_HW_BANS* hw_bans[MAX_ACCOUNTS];
L_IP_BANS* ip_bans[MAX_ACCOUNTS];
L_KEY_DATA* key_data[MAX_ACCOUNTS];
L_SECURITY_DATA* security_data[MAX_ACCOUNTS];
L_SHIP_DATA* ship_data[SHIP_COMPILED_MAX_CONNECTIONS];
L_TEAM_DATA* team_data[MAX_ACCOUNTS];

unsigned num_accounts = 0;
unsigned num_characters = 0;
unsigned num_guilds = 0;
unsigned num_hwbans = 0;
unsigned num_ipbans = 0;
unsigned num_keydata = 0;
unsigned num_bankdata = 0;
unsigned num_security = 0;
unsigned num_shipkeys = 0;
unsigned num_teams = 0;
unsigned ds, ds2;
int ds_found, new_record, free_record;

#endif

BANK empty_bank;
//CHALLENGEDATA empty_challengeData;
//BATTLEDATA empty_battleData;

/* encryption stuff 加密*/

void prepare_key(unsigned char* keydata, unsigned len, struct rc4_key* key);

PSO_CRYPT* cipher_ptr;

void encryptcopy(BANANA* client, const unsigned char* src, unsigned size);
void decryptcopy(unsigned char* dest, const unsigned char* src, unsigned size);
void compressShipPacket(ORANGE* ship, unsigned char* src, unsigned long src_size);
void decompressShipPacket(ORANGE* ship, unsigned char* dest, unsigned char* src);

#ifdef NO_SQL

void UpdateDataFile(const char* filename, unsigned count, void* data, unsigned record_size, int new_record);
void DumpDataFile(const char* filename, unsigned* count, void** data, unsigned record_size);

unsigned lastdump = 0;

#endif

#define MYWM_NOTIFYICON (WM_USER+2)
int program_hidden = 1;
HWND consoleHwnd;
HWND backupHwnd;

void WriteLog(char* fmt, ...)
{
#define MAX_GM_MESG_LEN 4096

	va_list args;
	char text[MAX_GM_MESG_LEN];
	SYSTEMTIME rawtime;

	FILE* fp;

	GetLocalTime(&rawtime);
	va_start(args, fmt);
	vsprintf_s(text, _countof(text), fmt, args);
	strcat_s(text, _countof(text), "\r\n");
	va_end(args);

	fopen_s(&fp, "login.log", "a");
	if (!fp)
	{
		printf("无法记录日志 login.log\n");
	}

	fprintf(fp, "[%02u-%02u-%u, %02u:%02u] %s", rawtime.wMonth, rawtime.wDay, rawtime.wYear,
		rawtime.wHour, rawtime.wMinute, text);
	fclose(fp);

	printf("[%02u-%02u-%u, %02u:%02u] %s", rawtime.wMonth, rawtime.wDay, rawtime.wYear,
		rawtime.wHour, rawtime.wMinute, text);
}


void display_packet(unsigned char* buf, int len)
{
	int c, c2, c3, c4;

	c = c2 = c3 = c4 = 0;

	for (c = 0;c < len;c++)
	{
		if (c3 == 16)
		{
			for (;c4 < c;c4++)
				if (buf[c4] >= 0x20)
					dp[c2++] = buf[c4];
				else
					dp[c2++] = 0x2E;
			c3 = 0;
			sprintf_s(&dp[c2++], _countof(dp) - c2, "\n");
		}

		if ((c == 0) || !(c % 16))
		{
			sprintf_s(&dp[c2], _countof(dp) - c2, "(%04X) ", c);
			c2 += 7;
		}

		sprintf_s(&dp[c2], _countof(dp) - c2, "(%02X) ", buf[c]);
		c2 += 3;
		c3++;
	}

	if (len % 16)
	{
		c3 = len;
		while (c3 % 16)
		{
			sprintf_s(&dp[c2], _countof(dp) - c2, "   ");
			c2 += 3;
			c3++;
		}
	}

	for (;c4 < c;c4++)
		if (buf[c4] >= 0x20)
			dp[c2++] = buf[c4];
		else
			dp[c2++] = 0x2E;

	dp[c2] = 0;
	printf("%s\n\n", &dp[0]);
}

/* Computes the message digest for string inString.
Prints out message digest, a space, the string (in quotes) and a
carriage return.
*/
void MDString(inString, outString)
char* inString;
char* outString;
{
	unsigned char c;
	MD5_CTX mdContext;
	unsigned int len = strlen(inString);

	MD5Init(&mdContext);
	MD5Update(&mdContext, inString, len);
	MD5Final(&mdContext);
	for (c = 0;c < 16;c++)
	{
		*outString = mdContext.digest[c];
		outString++;
	}
}

void convertIPString(char* IPData, unsigned IPLen, int fromConfig)
{
	unsigned p, p2, p3;
	char convert_buffer[5];

	p2 = 0;
	p3 = 0;
	for (p = 0;p < IPLen;p++)
	{
		if ((IPData[p] > 0x20) && (IPData[p] != 46))
			convert_buffer[p3++] = IPData[p]; else
		{
			convert_buffer[p3] = 0;
			if (IPData[p] == 46) // .
			{
				serverIP[p2] = atoi(&convert_buffer[0]);
				p2++;
				p3 = 0;
				if (p2 > 3)
				{
					if (fromConfig)
						printf("tethealla.ini 已损坏. (无法从文件中读取IP信息!)\n"); else
						printf("无法确定IP地址.\n");
					printf("按下 [回车键] 退出");
					gets_s(&dp[0], sizeof(dp));
					exit(1);
				}
			}
			else
			{
				serverIP[p2] = atoi(&convert_buffer[0]);
				if (p2 != 3)
				{
					if (fromConfig)
						printf("tethealla.ini 已损坏. (无法从文件中读取IP信息!)\n"); else
						printf("无法确定IP地址.\n");
					printf("按下 [回车键] 退出");
					gets_s(&dp[0], sizeof(dp));
					exit(1);
				}
				break;
			}
		}
	}
}

long CalculateChecksum(void* data, unsigned long size)
{
	long offset, y, cs = 0xFFFFFFFF;
	for (offset = 0; offset < (long)size; offset++)
	{
		cs ^= *(unsigned char*)((long)data + offset);
		for (y = 0; y < 8; y++)
		{
			if (!(cs & 1)) cs = (cs >> 1) & 0x7FFFFFFF;
			else cs = ((cs >> 1) & 0x7FFFFFFF) ^ 0xEDB88320;
		}
	}
	return (cs ^ 0xFFFFFFFF);
}

unsigned char EBBuffer[0x10000];

void construct0xEB()
{
	char EBFiles[16][255];
	unsigned EBSize;
	unsigned EBOffset = 0;
	unsigned EBChecksum;
	FILE* fp;
	FILE* fpb;
	unsigned char ch, ch6;
	unsigned ch2, ch3, ch4, ch5, ch7;

	fopen_s(&fp, "e8send.txt", "r");
	if (fp == NULL)
	{
		printf("缺失 e8send.txt\n");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}
	PacketEB01[0x02] = 0xEB;
	PacketEB01[0x03] = 0x01;
	ch = ch6 = 0;
	ch3 = 0x08;
	ch4 = ch5 = 0x00;
	while (fgets(&EBFiles[ch][0], 255, fp) != NULL)
	{
		ch2 = strlen(&EBFiles[ch][0]);
		if (EBFiles[ch][ch2 - 1] == 0x0A)
			EBFiles[ch][ch2--] = 0x00;
		EBFiles[ch][ch2] = 0;
		printf("\n正在从 %s 中载入... ", &EBFiles[ch][0]);
		fopen_s(&fpb, EBFiles[ch], "rb");
		if (fpb == NULL)
		{
			printf("无法打开 %s!\n", &EBFiles[ch][0]);
			printf("按下 [回车键] 退出");
			gets_s(&dp[0], sizeof(dp));
			exit(1);
		}
		fseek(fpb, 0, SEEK_END);
		EBSize = ftell(fpb);
		fseek(fpb, 0, SEEK_SET);
		fread(&EBBuffer[0], 1, EBSize, fpb);
		EBChecksum = (unsigned)CalculateChecksum(&EBBuffer[0], EBSize);
		*(unsigned*)&PacketEB01[ch3] = EBSize;
		ch3 += 4;
		*(unsigned*)&PacketEB01[ch3] = EBChecksum;
		ch3 += 4;
		*(unsigned*)&PacketEB01[ch3] = EBOffset;
		ch3 += 4;
		memcpy(&PacketEB01[ch3], &EBFiles[ch][0], ch2 + 1);
		ch3 += 0x40;
		EBOffset += EBSize;
		ch++;
		fclose(fpb);
		for (ch7 = 0;ch7 < EBSize;ch7++)
		{
			if (ch4 == 0x00)
			{
				ch5 += 2;
				PacketEB02[ch5++] = 0xEB;
				PacketEB02[ch5++] = 0x02;
				ch5 += 4;
				PacketEB02[ch5++] = ch6;
				ch5 += 3;
				ch4 = 0x0C;
			}
			PacketEB02[ch5++] = EBBuffer[ch7];
			ch4++;
			if (ch4 == 26636)
			{
				*(unsigned short*)&PacketEB02[ch5 - 26636] = (unsigned short)ch4;
				ch4 = 0;
				ch6++;
			}
		}
	}

	if (ch4) // Probably have some remaining data.
	{
		*(unsigned short*)&PacketEB02[ch5 - ch4] = (unsigned short)ch4;
		PacketEB02_Total = ch5;
		if (ch5 > MAX_EB02)
		{
			printf("太多的补丁数据要发送.\n");
			printf("按下 [回车键] 退出");
			gets_s(&dp[0], sizeof(dp));
			exit(1);
		}
	}

	*(unsigned short*)&PacketEB01[0x00] = (unsigned short)ch3;
	PacketEB01[0x04] = ch;
	PacketEB01_Total = ch3;
	fclose(fp);
}

unsigned normalName = 0xFFFFFFFF;
unsigned globalName = 0xFF1D94F7;
unsigned localName = 0xFFB0C4DE;

unsigned char hexToByte(char* hs)
{
	unsigned b;

	if (hs[0] < 58) b = (hs[0] - 48); else b = (hs[0] - 55);
	b *= 16;
	if (hs[1] < 58) b += (hs[1] - 48); else b += (hs[1] - 55);
	return (unsigned char)b;
}

//测试检测的数据是否含有反斜杠
void strd(unsigned char* a) {
	unsigned char* p = "";
	//需要的子串
	if (strstr(a, p))
		WriteLog("\n[警告]:！！ \n 数据 %p 字符串为 %d\n字节长度; %d\n", a, a, sizeof(a));
}

void load_config_file()
{
	int config_index = 0;
	char config_data[255];
	unsigned ch;

	FILE* fp;

	fopen_s(&fp, "tethealla.ini", "r");
	if (fp == NULL)
	{
		printf("设置文件 tethealla.ini 似乎缺失了.\n");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}
	else
		while (fgets(&config_data[0], 255, fp) != NULL)
		{
			if (config_data[0] != 0x23)
			{
				if ((config_index < 0x04) || (config_index > 0x04))
				{
					ch = strlen(&config_data[0]);
					if (config_data[ch - 1] == 0x0A)
						config_data[ch--] = 0x00;
					config_data[ch] = 0;
				}
				switch (config_index)
				{
				case 0x00:
					// MySQL Host
					memcpy(&mySQL_Host[0], &config_data[0], ch + 1);
					break;
				case 0x01:
					// MySQL Username
					memcpy(&mySQL_Username[0], &config_data[0], ch + 1);
					break;
				case 0x02:
					// MySQL Password
					memcpy(&mySQL_Password[0], &config_data[0], ch + 1);
					break;
				case 0x03:
					// MySQL Database
					memcpy(&mySQL_Database[0], &config_data[0], ch + 1);
					break;
				case 0x04:
					// MySQL Port
					mySQL_Port = atoi(&config_data[0]);
					break;
				case 0x05:
					// Server IP address
				{
					if ((config_data[0] == 0x41) || (config_data[0] == 0x61))
					{
						struct sockaddr_in pn_in;
						struct hostent* pn_host;
						int pn_sockfd, pn_len;
						char pn_buf[512];
						char* pn_ipdata;

						printf("\n** 正在确认IP地址 ... ");

						pn_host = gethostbyname("sanc.top");
						if (!pn_host) {
							printf("无法解析 sanc.top\n");
							printf("按下 [回车键] 退出");
							gets_s(&dp[0], sizeof(dp));
							exit(1);
						}

						/* Create a reliable, stream socket using TCP */
						if ((pn_sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
						{
							printf("无法创建TCP/IP流套接字.");
							printf("按下 [回车键] 退出");
							gets_s(&dp[0], sizeof(dp));
							exit(1);
						}

						/* Construct the server address structure */
						memset(&pn_in, 0, sizeof(pn_in)); /* Zero out structure */
						pn_in.sin_family = AF_INET; /* Internet address family */

						*(unsigned*)&pn_in.sin_addr.s_addr = *(unsigned*)pn_host->h_addr; /* Web Server IP address */

						pn_in.sin_port = htons(80); /* Web Server port */

													/* Establish the connection to the pioneer2.net Web Server ... */

						if (connect(pn_sockfd, (struct sockaddr*)&pn_in, sizeof(pn_in)) < 0)
						{
							printf("\n无法连接至 sanc.top!");
							printf("按下 [回车键] 退出");
							gets_s(&dp[0], sizeof(dp));
							exit(1);
						}

						/* Process pioneer2.net's response into the serverIP variable. */

						send_to_server(pn_sockfd, HTTP_REQ);
						pn_len = recv(pn_sockfd, &pn_buf[0], sizeof(pn_buf) - 1, 0);
						closesocket(pn_sockfd);
						pn_buf[pn_len] = 0;
						pn_ipdata = strstr(&pn_buf[0], "/html");
						if (!pn_ipdata)
						{
							printf("无法确定IP地址.\n");
						}
						else
							pn_ipdata += 9;

						convertIPString(pn_ipdata, strlen(pn_ipdata), 0);
					}
					else
					{
						convertIPString(&config_data[0], ch + 1, 1);
					}
				}
				break;
				case 0x06:
					// Welcome Message
					memcpy(&Welcome_Message[0], &config_data[0], ch + 1);
					break;
				case 0x07:
					// Server Listen Port
					serverPort = atoi(&config_data[0]);
					break;
				case 0x08:
					// Max Client Connections
					serverMaxConnections = atoi(&config_data[0]);
					if (serverMaxConnections > LOGIN_COMPILED_MAX_CONNECTIONS)
					{
						serverMaxConnections = LOGIN_COMPILED_MAX_CONNECTIONS;
						printf("此版本的登录服务器尚未被编译以处理多于 %u 的连接登陆请求.  已自动调整.\n", LOGIN_COMPILED_MAX_CONNECTIONS);
					}
					if (!serverMaxConnections)
						serverMaxConnections = LOGIN_COMPILED_MAX_CONNECTIONS;
					break;
				case 0x09:
					// Max Ship Connections
					serverMaxShips = atoi(&config_data[0]);
					if (serverMaxShips > SHIP_COMPILED_MAX_CONNECTIONS)
					{
						serverMaxShips = SHIP_COMPILED_MAX_CONNECTIONS;
						printf("此版本的登录服务器尚未编译为处理超过 %u 个舰船p连接.  已自动调整.\n", SHIP_COMPILED_MAX_CONNECTIONS);
					}
					if (!serverMaxShips)
						serverMaxShips = SHIP_COMPILED_MAX_CONNECTIONS;
					break;
				case 0x0A:
					// Override IP address (if specified, this IP will be sent out instead of your own to those who connect)
					if (config_data[0] > 0x30)
					{
						if (override_on == 1)
						{
							struct hostent* IP_host;
							//这里域名竟然-1,待解决
							//config_data[strlen(&config_data[0]) - 1] = 0x00;
							config_data[strlen(&config_data[0])] = 0x00;
							printf("解析中 %s ...\n", (char*)&config_data[0]);
							IP_host = gethostbyname(&config_data[0]);
							if (!IP_host)
							{
								printf("无法解析该域名.");
								printf("按下 [回车键] 退出");
								gets_s(&dp[0], 0);
								exit(1);
							}
							*(unsigned*)&serverIP[0] = *(unsigned*)IP_host->h_addr;
							printf("域名解析成功.");
						}
						else {
							*(unsigned*)&overrideIP[0] = *(unsigned*)&serverIP[0];
							serverIP[0] = 0;
							convertIPString(&config_data[0], ch + 1, 1);
						}
					}
					break;
				case 0x0B:
					// Hildebear rate
					mob_rate[0] = atoi(&config_data[0]);
					break;
				case 0x0C:
					// Rappy rate
					mob_rate[1] = atoi(&config_data[0]);
					break;
				case 0x0D:
					// Lily rate
					mob_rate[2] = atoi(&config_data[0]);
					break;
				case 0x0E:
					// Slime rate
					mob_rate[3] = atoi(&config_data[0]);
					break;
				case 0x0F:
					// Merissa rate
					mob_rate[4] = atoi(&config_data[0]);
					break;
				case 0x10:
					// Pazuzu rate
					mob_rate[5] = atoi(&config_data[0]);
					break;
				case 0x11:
					// Dorphon Eclair rate
					mob_rate[6] = atoi(&config_data[0]);
					break;
				case 0x12:
					// Kondrieu rate
					mob_rate[7] = atoi(&config_data[0]);
					break;
				case 0x13:
					// Global GM name color
					(unsigned char)config_data[6] = hexToByte(&config_data[4]);
					(unsigned char)config_data[7] = hexToByte(&config_data[2]);
					(unsigned char)config_data[8] = hexToByte(&config_data[0]);
					config_data[9] = 0xFF;
					globalName = *(unsigned*)&config_data[6];
					break;
				case 0x14:
					// Local GM name color
					(unsigned char)config_data[6] = hexToByte(&config_data[4]);
					(unsigned char)config_data[7] = hexToByte(&config_data[2]);
					(unsigned char)config_data[8] = hexToByte(&config_data[0]);
					config_data[9] = 0xFF;
					localName = *(unsigned*)&config_data[6];
					break;
				case 0x15:
					// Normal name color
					(unsigned char)config_data[6] = hexToByte(&config_data[4]);
					(unsigned char)config_data[7] = hexToByte(&config_data[2]);
					(unsigned char)config_data[8] = hexToByte(&config_data[0]);
					config_data[9] = 0xFF;
					normalName = *(unsigned*)&config_data[6];
					break;
				default:
					break;
				}
				config_index++;
			}
		}
	fclose(fp);

	if (config_index < 0x13)
	{
		printf("tethealla.ini seems to be corrupted.\n");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}
}

BANANA* connections[LOGIN_COMPILED_MAX_CONNECTIONS];
ORANGE* ships[SHIP_COMPILED_MAX_CONNECTIONS];
BANANA* workConnect;
ORANGE* workShip;

unsigned char PacketA0Data[0x4000] = { 0 };
unsigned short PacketA0Size = 0;

const wchar_t serverName[6] = { L"母舰 \0" }; //sancaros 汉化有问题
const char NoShips[14] = { "未发现舰船 \0" };

void construct0xA0()
{
	ORANGE* shipcheck;
	unsigned totalShips = 0xFFFFFFF4;
	unsigned short A0Offset;
	char* shipName;
	unsigned char playerCountString[5];
	unsigned ch, ch2, shipNum;

	memset(&PacketA0Data[0], 0, 0x4000);

	PacketA0Data[0x02] = 0xA0;
	PacketA0Data[0x0A] = 0x20;
	*(unsigned*)&PacketA0Data[0x0C] = totalShips;
	PacketA0Data[0x10] = 0x04;
	memcpy(&PacketA0Data[0x12], &serverName[0], 18);
	A0Offset = 0x36;
	totalShips = 0x00;
	for (ch = 0;ch < serverNumShips;ch++)
	{
		shipNum = serverShipList[ch];
		if (ships[shipNum])
		{
			shipcheck = ships[shipNum];
			if ((shipcheck->shipSockfd >= 0) && (shipcheck->playerCount < 10000))
			{
				totalShips++;
				PacketA0Data[A0Offset] = 0x12;
				*(unsigned*)&PacketA0Data[A0Offset + 2] = shipcheck->shipID;
				ch2 = A0Offset + 0x08;
				shipName = &shipcheck->name[0];
				while (*shipName != 0x00)
				{
					PacketA0Data[ch2++] = *shipName;
					PacketA0Data[ch2++] = 0x00;
					shipName++;
				}
				PacketA0Data[ch2++] = 0x20;
				PacketA0Data[ch2++] = 0x00;
				PacketA0Data[ch2++] = 0x28;
				PacketA0Data[ch2++] = 0x00;
				_itoa_s(shipcheck->playerCount, playerCountString, _countof(playerCountString), 10);
				shipName = &playerCountString[0];
				while (*shipName != 0x00)
				{
					PacketA0Data[ch2++] = *shipName;
					PacketA0Data[ch2++] = 0x00;
					shipName++;
				}
				PacketA0Data[ch2++] = 0x29;
				PacketA0Data[ch2++] = 0x00;
				A0Offset += 0x2C;
			}
		}
	}
	if (!totalShips)
	{
		totalShips++;
		PacketA0Data[A0Offset] = 0x12;
		*(unsigned*)&PacketA0Data[A0Offset + 2] = totalShips;
		for (ch = 0;ch < 9;ch++)
			PacketA0Data[A0Offset + 0x08 + (ch * 2)] = NoShips[ch];
		A0Offset += 0x2C;
	}
	*(unsigned*)&PacketA0Data[0x04] = totalShips;
	while (A0Offset % 8)
		PacketA0Data[A0Offset++] = 0x00;
	*(unsigned short*)&PacketA0Data[0x00] = (unsigned short)A0Offset;
	PacketA0Size = A0Offset;
}



unsigned free_connection()
{
	unsigned fc;
	BANANA* wc;

	for (fc = 0;fc < serverMaxConnections;fc++)
	{
		wc = connections[fc];
		if (wc->plySockfd < 0)
			return fc;
	}
	return 0xFFFF;
}

unsigned free_shipconnection()
{
	unsigned fc;
	ORANGE* wc;

	for (fc = 0;fc < serverMaxShips;fc++)
	{
		wc = ships[fc];
		if (wc->shipSockfd < 0)
			return fc;
	}
	return 0xFFFF;
}


void initialize_connection(BANANA* connect)
{
	unsigned ch, ch2;

	if (connect->plySockfd >= 0)
	{
		ch2 = 0;
		for (ch = 0;ch < serverNumConnections;ch++)
		{
			if (serverConnectionList[ch] != connect->connection_index)
				serverConnectionList[ch2++] = serverConnectionList[ch];
		}
		serverNumConnections = ch2;
		closesocket(connect->plySockfd);
	}
	memset(connect, 0, sizeof(BANANA));
	connect->plySockfd = -1;
	connect->login = -1;
	connect->lastTick = 0xFFFFFFFF;
	connect->connected = 0xFFFFFFFF;
}

void initialize_ship(ORANGE* ship)
{
	unsigned ch, ch2;

	if (ship->shipSockfd >= 0)
	{
		if ((ship->key_index) && (ship->key_index <= max_ship_keys) && (keys_in_use[ship->key_index]))
			keys_in_use[ship->key_index] = 0; // key no longer in use

		ch2 = 0;
		for (ch = 0;ch < serverNumShips;ch++)
		{
			if (serverShipList[ch] != ship->connection_index)
				serverShipList[ch2++] = serverShipList[ch];
		}
		serverNumShips = ch2;
		closesocket(ship->shipSockfd);
	}
	memset(ship, 0, sizeof(ORANGE));
	for (ch = 0;ch < 128;ch++)
		ship->key_change[ch] = -1;
	ship->shipSockfd = -1;
	// Will be changed upon a successful authentication
	ship->playerCount = 0xFFFF;
	construct0xA0(); // Removes inactive ships
}

void start_encryption(BANANA* connect)
{
	unsigned c, c3, c4, connectNum;
	BANANA* workConnect, * c5;

	// Limit the number of connections from an IP address to MAX_SIMULTANEOUS_CONNECTIONS.

	c3 = 0;

	for (c = 0;c < serverNumConnections;c++)
	{
		connectNum = serverConnectionList[c];
		workConnect = connections[connectNum];
		//debug ("%s comparing to %s", (char*) &workConnect->IP_Address[0], (char*) &connect->IP_Address[0]);
		if ((!strcmp(&workConnect->IP_Address[0], &connect->IP_Address[0])) &&
			(workConnect->plySockfd >= 0))
			c3++;
	}

	if (c3 > MAX_SIMULTANEOUS_CONNECTIONS)
	{
		// More than MAX_SIMULTANEOUS_CONNECTIONS connections from a certain IP address...
		// Delete oldest connection to server.
		c4 = 0xFFFFFFFF;
		c5 = NULL;
		for (c = 0;c < serverNumConnections;c++)
		{
			connectNum = serverConnectionList[c];
			workConnect = connections[connectNum];
			if ((!strcmp(&workConnect->IP_Address[0], &connect->IP_Address[0])) &&
				(workConnect->plySockfd >= 0))
			{
				if (workConnect->connected < c4)
				{
					c4 = workConnect->connected;
					c5 = workConnect;
				}
			}
		}
		if (c5)
		{
			workConnect = c5;
			initialize_connection(workConnect);
		}
	}

	memcpy(&connect->sndbuf[0], &Packet03[0], sizeof(Packet03));
	for (c = 0;c < 0x30;c++)
	{
		connect->sndbuf[0x68 + c] = (unsigned char)rand() % 255;
		connect->sndbuf[0x98 + c] = (unsigned char)rand() % 255;
	}
	connect->snddata += sizeof(Packet03);
	cipher_ptr = &connect->server_cipher;
	pso_crypt_table_init_bb(cipher_ptr, &connect->sndbuf[0x68]);
	cipher_ptr = &connect->client_cipher;
	pso_crypt_table_init_bb(cipher_ptr, &connect->sndbuf[0x98]);
	connect->crypt_on = 1;
	connect->sendCheck[SEND_PACKET_03] = 1;
	connect->connected = (unsigned)servertime;
}

void SendB1(BANANA* client)
{
	SYSTEMTIME rawtime;

	if ((client->guildcard) && (client->slotnum != -1))
	{
		GetSystemTime(&rawtime);
		*(long long*)&client->encryptbuf[0] = *(long long*)&PacketB1[0];
		memset(&client->encryptbuf[0x08], 0, 28);
		sprintf_s(&client->encryptbuf[8], _countof(client->encryptbuf) - 8, "%u:%02u:%02u: %02u:%02u:%02u.%03u", rawtime.wYear, rawtime.wMonth, rawtime.wDay,
			rawtime.wHour, rawtime.wMinute, rawtime.wSecond, rawtime.wMilliseconds);
		cipher_ptr = &client->server_cipher;
		encryptcopy(client, &client->encryptbuf[0], 0x24);
	}
	else
		client->todc = 1;
}

//Send1A文本信息代码
void Send1A(const char* mes, BANANA* client)
{
	unsigned short x1A_Len;

	memcpy(&Packet1AData[0], &Packet1A[0], sizeof(Packet1A));
	x1A_Len = sizeof(Packet1A);

	while (*mes != 0x00)
	{
		Packet1AData[x1A_Len++] = *(mes++);
		Packet1AData[x1A_Len++] = 0x00;
	}

	Packet1AData[x1A_Len++] = 0x00;
	Packet1AData[x1A_Len++] = 0x00;

	while (x1A_Len % 8)
		Packet1AData[x1A_Len++] = 0x00;
	*(unsigned short*)&Packet1AData[0] = x1A_Len;
	cipher_ptr = &client->server_cipher;
	encryptcopy(client, &Packet1AData[0], x1A_Len);
	client->todc = 1;
}

void SendA0(BANANA* client)
{
	if ((client->guildcard) && (client->slotnum != -1))
	{
		cipher_ptr = &client->server_cipher;
		encryptcopy(client, &PacketA0Data[0], *(unsigned short*)&PacketA0Data[0]);
	}
	else
		client->todc = 1;
}


void SendEE(const char* mes, BANANA* client)
{
	unsigned short xEE_Len;

	if ((client->guildcard) && (client->slotnum != -1))
	{
		memcpy(&PacketEEData[0], &PacketEE[0], sizeof(PacketEE));
		xEE_Len = sizeof(PacketEE);

		while (*mes != 0x00)
		{
			PacketEEData[xEE_Len++] = *(mes++);
			PacketEEData[xEE_Len++] = 0x00;
		}

		PacketEEData[xEE_Len++] = 0x00;
		PacketEEData[xEE_Len++] = 0x00;

		while (xEE_Len % 8)
			PacketEEData[xEE_Len++] = 0x00;
		*(unsigned short*)&PacketEEData[0] = xEE_Len;
		cipher_ptr = &client->server_cipher;
		encryptcopy(client, &PacketEEData[0], xEE_Len);
	}
}

void Send19(unsigned char ip1, unsigned char ip2, unsigned char ip3, unsigned char ip4, unsigned short ipp, BANANA* client)
{
	memcpy(&client->encryptbuf[0], &Packet19, sizeof(Packet19));
	client->encryptbuf[0x08] = ip1;
	client->encryptbuf[0x09] = ip2;
	client->encryptbuf[0x0A] = ip3;
	client->encryptbuf[0x0B] = ip4;
	*(unsigned short*)&client->encryptbuf[0x0C] = ipp;
	cipher_ptr = &client->server_cipher;
	encryptcopy(client, &client->encryptbuf[0], sizeof(Packet19));
}

unsigned char key_data_send[840 + 10] = { 0 };

//发送设置数据
void SendE2(BANANA* client)
{
	int key_exists = 0;

	if ((client->guildcard) && (!client->sendCheck[SEND_PACKET_E2]))
	{
		memcpy(&PacketE2Data[0], &E2_Base[0], 2808);

#ifdef NO_SQL

		ds_found = -1;
		for (ds = 0;ds < num_keydata;ds++)
		{
			if (key_data[ds]->guildcard == client->guildcard)
			{
				ds_found = ds;
				break;
			}
		}
		if (ds_found != -1)
			memcpy(&PacketE2Data[0x11C], &key_data[ds_found]->controls, 420);
		else
		{
			key_data[num_keydata] = malloc(sizeof(L_KEY_DATA));
			key_data[num_keydata]->guildcard = client->guildcard;
			memcpy(&key_data[num_keydata]->controls, &E2_Base[0x11C], 420);
			UpdateDataFile("keydata.dat", num_keydata, key_data[num_keydata], sizeof(L_KEY_DATA), 1);
			num_keydata++;
		}

#else

		sprintf_s(myQuery, _countof(myQuery), "SELECT * from key_data WHERE guildcard='%u'", client->guildcard);

		//printf ("MySQL query %s\n", myQuery );

		// Check to see if we've got some saved key data.检查一下我们是否保存了一些密匙数据

		if (!mysql_query(myData, &myQuery[0]))
		{
			myResult = mysql_store_result(myData);
			key_exists = (int)mysql_num_rows(myResult);
			if (key_exists)
			{
				//debug("密匙数据已找到, 对比中...");
				myRow = mysql_fetch_row(myResult);
				memcpy(&PacketE2Data[0x11C], myRow[1], 420);
			}
			else
			{
				//debug("密匙数据不存在...");
				mysql_real_escape_string(myData, &key_data_send[0], &E2_Base[0x11C], 420);
				sprintf_s(myQuery, _countof(myQuery), "INSERT INTO key_data (guildcard, controls) VALUES ('%u','%s')", client->guildcard, (char*)&key_data_send[0]);
				memcpy(&PacketE2Data[0x11C], &E2_Base[0x11C], 420);
				if (mysql_query(myData, &myQuery[0]))
					debug("无法将密钥信息插入数据库.");
			}
			mysql_free_result(myResult);
		}
		else
		{
			Send1A("Could not fetch key data.\n\nThere is a problem with the MySQL database.", client);
			client->todc = 1;
			return;
		}
#endif
		memset(&PacketE2Data[0xAF4], 0xFF, 4); // Enable dressing room, etc,.启用更衣室等
		cipher_ptr = &client->server_cipher;
		encryptcopy(client, &PacketE2Data[0], sizeof(PacketE2Data));
		client->sendCheck[SEND_PACKET_E2] = 0x01;
	}
	else
		client->todc = 1;
}

//文本长度
unsigned StringLength(const char* src)
{
	unsigned ch = 0;

	while (*src != 0x00)
	{
		ch++;
		src++;
	}
	return ch;
}

CHARDATA E7_Base, E7_Work;
unsigned char chardata[0x200];
unsigned char E7chardata[(sizeof(CHARDATA) * 2) + 100];
unsigned char startingStats[12 * 14];
unsigned char DefaultTeamFlagSlashes[4098];
unsigned char DefaultTeamFlag[2048];
unsigned char FlagSlashes[4098];

//角色创建相关代码
void AckCharacter_Creation(unsigned char slotnum, BANANA* client)
{
	unsigned short* n;
#ifndef NO_SQL
	int char_exists;
#endif
	unsigned short packetSize;
	MINICHAR* clientchar;
	CHARDATA* E7Base;
	CHARDATA* NewE7;
	unsigned short maxFace, maxHair, maxHairColorRed, maxHairColorBlue, maxHairColorGreen,
		maxCostume, maxSkin, maxHead;
	unsigned ch;

	packetSize = *(unsigned short*)&client->decryptbuf[0];
	if ((client->guildcard) && (slotnum < 4) && (client->sendingchars == 0) &&
		(packetSize == 0x88) && (client->decryptbuf[0x45] < CLASS_MAX))
	{
		clientchar = (MINICHAR*)&client->decryptbuf[0x00];
		clientchar->name[0] = 0x09; // Filter colored names 过滤彩色名称
		clientchar->name[1] = 0x00;
		clientchar->name[2] = 0x45;
		clientchar->name[3] = 0x00;
		clientchar->name[22] = 0; // Truncate names too long 截断名称太长
		clientchar->name[23] = 0;
		if ((clientchar->_class == CLASS_HUMAR) ||
			(clientchar->_class == CLASS_HUNEWEARL) ||
			(clientchar->_class == CLASS_RAMAR) ||
			(clientchar->_class == CLASS_RAMARL) ||
			(clientchar->_class == CLASS_FOMARL) ||
			(clientchar->_class == CLASS_FONEWM) ||
			(clientchar->_class == CLASS_FONEWEARL) ||
			(clientchar->_class == CLASS_FOMAR))
		{
			maxFace = 0x05;
			maxHair = 0x0A;
			maxHairColorRed = 0xFF;
			maxHairColorBlue = 0xFF;
			maxHairColorGreen = 0xFF;
			maxCostume = 0x11;
			maxSkin = 0x03;
			maxHead = 0x00;
		}
		else
		{
			maxFace = 0x00;
			maxHair = 0x00;
			maxHairColorRed = 0x00;
			maxHairColorBlue = 0x00;
			maxHairColorGreen = 0x00;
			maxCostume = 0x00;
			maxSkin = 0x18;
			maxHead = 0x04;
		}
		if (clientchar->skinID > 0x06)
			clientchar->skinID = 0x00;
		clientchar->nameColorTransparency = 0xFF;
		if (clientchar->sectionID > 0x09)
			clientchar->sectionID = 0x00;
		if (clientchar->proportionX > 0x3F800000)
			clientchar->proportionX = 0x3F800000;
		if (clientchar->proportionY > 0x3F800000)
			clientchar->proportionY = 0x3F800000;
		if (clientchar->face > maxFace)
			clientchar->face = 0x00;
		if (clientchar->hair > maxHair)
			clientchar->hair = 0x00;
		if (clientchar->hairColorRed > maxHairColorRed)
			clientchar->hairColorRed = 0x00;
		if (clientchar->hairColorBlue > maxHairColorBlue)
			clientchar->hairColorBlue = 0x00;
		if (clientchar->hairColorGreen > maxHairColorGreen)
			clientchar->hairColorGreen = 0x00;
		if (clientchar->costume > maxCostume)
			clientchar->costume = 0x00;
		if (clientchar->skin > maxSkin)
			clientchar->skin = 0x00;
		if (clientchar->head > maxHead)
			clientchar->head = 0x00;
		for (ch = 0;ch < 8;ch++)
			clientchar->unknown5[ch] = 0x00;
		clientchar->playTime = 0;

		if (client->dress_flag == 0)
		{
			/* Yeah!  MySQL! :D */

#ifndef NO_SQL

			mysql_real_escape_string(myData, &chardata[0], &client->decryptbuf[0x10], 0x78); //0x78 120字节的存储

#endif

			/* Let's construct the FULL character now... 现在我们来构造完整的角色*/
			//初始化一个角色
			E7Base = &E7_Base; //定义为一个空的角色
			NewE7 = &E7_Work; //定义为一个正在设定的角色
			memset(NewE7, 0, sizeof(CHARDATA)); //角色缓存器初始化
			NewE7->packetSize = 0x399C; //数据包大小 14752
										//NewE7->packetSize = 0x399C; //数据包大小 14748
			NewE7->command = 0x00E7; //指令初始化
			memset(&NewE7->flags, 0, 4); //缓存器初始化 占用4个字节
			//调换一下代码位置 以匹配字节结构
			NewE7->HPmat = 0; //血量
			NewE7->TPmat = 0; //魔力
			NewE7->lang = 0; //语言
			switch (clientchar->_class) //切换到职业选择
			{
			case CLASS_HUMAR: //人类男
			case CLASS_HUNEWEARL: //人类女
			case CLASS_HUCAST: //机器人战士
			case CLASS_HUCASEAL: //机器人女战士
								 // Saber战士
				NewE7->inventory[0].in_use = 0x01;
				NewE7->inventory[0].flags = 0x08;
				NewE7->inventory[0].item.data[1] = 0x01; //不一样的地方
				NewE7->inventory[0].item.itemid = 0x00010000;
				break;
			case CLASS_RAMAR: //人类男枪
			case CLASS_RACAST: //人类女枪
			case CLASS_RACASEAL: //机器人男枪
			case CLASS_RAMARL: //机器人女枪
							   // Handgun枪手
				NewE7->inventory[0].in_use = 0x01;
				NewE7->inventory[0].flags = 0x08;
				NewE7->inventory[0].item.data[1] = 0x06; //不一样的地方
				NewE7->inventory[0].item.itemid = 0x00010000;
				break;
			case CLASS_FONEWM: //新人类男法师
			case CLASS_FONEWEARL: //新人类女法师
			case CLASS_FOMARL: //人类女法师
			case CLASS_FOMAR: //人类男法师
							  // Cane法师
				NewE7->inventory[0].in_use = 0x01;
				NewE7->inventory[0].flags = 0x08;
				NewE7->inventory[0].item.data[1] = 0x0A; //不一样的地方
				NewE7->inventory[0].item.itemid = 0x00010000;
				break;
			default:
				break;
			}
							 // Frame 框架
			NewE7->inventory[1].in_use = 0x01;
			NewE7->inventory[1].flags = 0x08;
			NewE7->inventory[1].item.data[0] = 0x01;
			NewE7->inventory[1].item.data[1] = 0x01;
			NewE7->inventory[1].item.itemid = 0x00010001; //定义物品ID最大长度65537

														  // Mag 这里可以修改初始玛古的数据
			NewE7->inventory[2].in_use = 0x01;
			NewE7->inventory[2].flags = 0x08;
			NewE7->inventory[2].item.data[0] = 0x02;
			NewE7->inventory[2].item.data[2] = 0x05;
			NewE7->inventory[2].item.data[4] = 0xF4;
			NewE7->inventory[2].item.data[5] = 0x01;
			NewE7->inventory[2].item.data2[0] = 0x14; // 20% synchro 20%同步
			NewE7->inventory[2].item.itemid = 0x00010002;

			if ((clientchar->_class == CLASS_HUCAST) || (clientchar->_class == CLASS_HUCASEAL) ||
				(clientchar->_class == CLASS_RACAST) || (clientchar->_class == CLASS_RACASEAL))
				NewE7->inventory[2].item.data2[3] = (unsigned char)clientchar->skin;
			else
				NewE7->inventory[2].item.data2[3] = (unsigned char)clientchar->costume;

			if (NewE7->inventory[2].item.data2[3] > 0x11)
				NewE7->inventory[2].item.data2[3] -= 0x11;

			// Monomates 单体
			NewE7->inventory[3].in_use = 0x01;
			NewE7->inventory[3].item.data[0] = 0x03;
			NewE7->inventory[3].item.data[5] = 0x04;
			NewE7->inventory[3].item.itemid = 0x00010003;

			if ((clientchar->_class == CLASS_FONEWM) || (clientchar->_class == CLASS_FONEWEARL) ||
				(clientchar->_class == CLASS_FOMARL) || (clientchar->_class == CLASS_FOMAR))
			{ //对应匹配四种人类模型
			  // Monofluids 单流体？
				NewE7->techniques[0] = 0x00;
				NewE7->inventory[4].in_use = 0x01;
				NewE7->inventory[4].flags = 0x00;
				NewE7->inventory[4].item.data[0] = 0x03;
				NewE7->inventory[4].item.data[1] = 0x01;
				NewE7->inventory[4].item.data[2] = 0x00;
				NewE7->inventory[4].item.data[3] = 0x00;
				memset(&NewE7->inventory[4].item.data[4], 0x00, 8);
				NewE7->inventory[4].item.data[5] = 0x04;
				NewE7->inventory[4].item.itemid = 0x00010004;
				memset(&NewE7->inventory[3].item.data2[0], 0x00, 4);
				NewE7->inventoryUse = 5;
			}
			else {
				NewE7->inventoryUse = 4;
			}
			//定义空白背包 格数为小于30
			for (ch = NewE7->inventoryUse;ch < 30;ch++)
			{
				NewE7->inventory[ch].in_use = 0x00;
				NewE7->inventory[ch].item.data[1] = 0xFF;
				NewE7->inventory[ch].item.itemid = 0xFFFFFFFF;
			}

			memcpy(&NewE7->ATP, &startingStats[clientchar->_class * 14], 14); //14字节 对应ATP MST EVP HP DFP TP LCK ATA 的属性

																			  //初始化的设置信息吧
			*(long long*)&NewE7->option_flags = *(long long*)&E7Base->option_flags;

			//NewE7->level = 0x00; 设置等级为0
			NewE7->level = E7Base->level;
			//NewE7->XP = 0; 设置经验为0
			E7Base->meseta = 300000; //设定基础的美赛塔值
			NewE7->meseta = E7Base->meseta;
			memset(&NewE7->gcString[0], 0x20, 2);
			*(long long*)&NewE7->gcString[2] = *(long long*)&client->guildcard_string[0];

			//来自于小人物角色资料结构
			memcpy(&NewE7->unknown3, &clientchar->unknown2, 14); //和E5指令中的参数一样
																 //来自于小人物角色资料结构
																 // Will copy all 4 values.会复制4个值
			*(unsigned*)&NewE7->nameColorBlue = *(unsigned*)&clientchar->nameColorBlue;
			//来自于小人物角色资料结构
			NewE7->skinID = clientchar->skinID; //来自于小人物角色资料结构
			memcpy(&NewE7->unknown4, &clientchar->unknown3, 18); //来自于小人物角色资料结构
			NewE7->sectionID = clientchar->sectionID; //来自于小人物角色资料结构
			NewE7->_class = clientchar->_class; //来自于小人物角色资料结构
			NewE7->skinFlag = clientchar->skinFlag; //来自于小人物角色资料结构
			memcpy(&NewE7->unknown5, &clientchar->unknown4, 5); //来自于小人物角色资料结构
			NewE7->costume = clientchar->costume; //来自于小人物角色资料结构
			NewE7->skin = clientchar->skin; //来自于小人物角色资料结构
			NewE7->face = clientchar->face; //来自于小人物角色资料结构
			NewE7->head = clientchar->head; //来自于小人物角色资料结构
			NewE7->hair = clientchar->hair; //来自于小人物角色资料结构
			NewE7->hairColorRed = clientchar->hairColorRed; //来自于小人物角色资料结构
			NewE7->hairColorBlue = clientchar->hairColorBlue; //来自于小人物角色资料结构
			NewE7->hairColorGreen = clientchar->hairColorGreen; //来自于小人物角色资料结构
			NewE7->proportionX = clientchar->proportionX; //来自于小人物角色资料结构
			NewE7->proportionY = clientchar->proportionY; //来自于小人物角色资料结构

			n = (unsigned short*)&clientchar->name[4]; //来自于小人物角色资料结构
			for (ch = 0;ch < 10;ch++)
			{
				if (*n == 0x0000)
					break;
				if ((*n == 0x0009) || (*n == 0x000A))
					*n = 0x0020;
				n++;
			}
			memcpy(&NewE7->name, &clientchar->name, 24); //来自于小人物角色资料结构
			*(unsigned*)&NewE7->unknown6 = *(unsigned*)&clientchar->unknown5; //来自于小人物角色资料结构
			memcpy(&NewE7->keyConfig, &E7Base->keyConfig, 232);
			// TO DO: Give Foie to starting Forces.举手投足 未完成的 让佛恩开始变得强力
			memcpy(&NewE7->techniques, &E7Base->techniques, 20);
			memcpy(&NewE7->name3, &E7Base->name3, 16);
			*(unsigned*)&NewE7->options = *(unsigned*)&E7Base->options; //构建选项
																		  //以下开始要注意了
			memcpy(&NewE7->quest_data1, &E7Base->quest_data1, 520);
			NewE7->bankUse = E7Base->bankUse;
			NewE7->bankMeseta = E7Base->bankMeseta;
			memcpy(&NewE7->bankInventory, &E7Base->bankInventory, 24 * 200);
			//名片相关的
			NewE7->guildCard = client->guildcard;
			memcpy(&NewE7->friendName, &clientchar->name, 24);
			memcpy(&NewE7->unknown9, &E7Base->unknown9, 56);
			memcpy(&NewE7->friendText, &E7Base->friendText, 176);
			NewE7->reserved1 = 0x01;
			NewE7->reserved2 = 0x01;
			NewE7->sectionID2 = clientchar->sectionID; //来自于小人物角色资料结构
			NewE7->_class2 = clientchar->_class; //来自于小人物角色资料结构
			*(unsigned*)&NewE7->unknown10[0] = *(unsigned*)&E7Base->unknown10[0];
			memcpy(&NewE7->symbol_chats, &E7Base->symbol_chats, 1248);
			memcpy(&NewE7->shortcuts, &E7Base->shortcuts, 2624);
			memcpy(&NewE7->autoReply, &E7Base->autoReply, 344);
			memcpy(&NewE7->GCBoard, &E7Base->GCBoard, 172);
			memcpy(&NewE7->unknown12, &E7Base->unknown12, 200);
			memcpy(&NewE7->challengeData, &E7Base->challengeData, 320);
			memcpy(&NewE7->techConfig, &E7Base->techConfig, 40);
			memcpy(&NewE7->unknown13, &E7Base->unknown13, 40);
			memcpy(&NewE7->battleData, &E7Base->battleData, 92);
			memcpy(&NewE7->unknown14, &E7Base->unknown14, 276); //应该是和公会有关吧
																// TO DO: Actually, we should use the values from the database, but I haven't added columns for them yet... 实际上，我们应该使用数据库中的值，但是我还没有为它们添加列 
																// For now, we'll use the "base" packet's values. 现在，我们将使用“base”包的值 
			memcpy(&NewE7->keyConfigGlobal, &E7Base->keyConfigGlobal, 364);
			memcpy(&NewE7->joyConfigGlobal, &E7Base->joyConfigGlobal, 56);
			memcpy(&NewE7->serial_number, &E7Base->serial_number, 4);
			//创建一个空的角色公会信息 包含结构之后的而所有数据 长度 2108
			memcpy(&NewE7->teamID, &E7Base->teamID, 4);
			memcpy(&NewE7->teamInformation, &E7Base->teamInformation, 8);
			memcpy(&NewE7->privilegeLevel, &E7Base->privilegeLevel, sizeof(&E7Base->privilegeLevel));
			memcpy(&NewE7->reserved3, &E7Base->reserved3, sizeof(&E7Base->reserved3));
			memcpy(&NewE7->teamName, &E7Base->teamName, 28);
			memcpy(&NewE7->teamRank, &E7Base->teamRank, sizeof(&E7Base->teamRank));
			memcpy(&NewE7->teamFlag, &E7Base->teamFlag, 2048);
			memcpy(&NewE7->teamRewards, &E7Base->teamRewards, 8);

#ifndef NO_SQL

			mysql_real_escape_string(myData, &E7chardata[0], (unsigned char*)NewE7, sizeof(CHARDATA));

#endif

		}

		// Check to see if the character exists in that slot.

#ifdef NO_SQL

		ds_found = -1;
		free_record = -1;

		for (ds = 0;ds < num_characters;ds++)
		{
			if (character_data[ds]->guildcard == 0)
				free_record = ds;

			if ((character_data[ds]->guildcard == client->guildcard) &&
				(character_data[ds]->slot == slotnum))
			{
				ds_found = ds;
				break;
			}
		}

		if ((client->dress_flag) && (ds_found != -1))
		{
			debug("Update character %u", client->guildcard);
			NewE7 = &E7_Work;
			memcpy(NewE7, &character_data[ds_found]->data, sizeof(CHARDATA));
			memcpy(&NewE7->gcString[0], &clientchar->gcString[0], 0x68);
			*(long long*)&clientchar->unknown5[0] = *(long long*)&NewE7->unknown6[0];
			//NewE7->playTime = 0;
			memcpy(&character_data[ds_found]->header, &client->decryptbuf[0x10], 0x78);
			memcpy(&character_data[ds_found]->data, NewE7, sizeof(CHARDATA));
			UpdateDataFile("character.dat", ds_found, character_data[ds_found], sizeof(L_CHARACTER_DATA), 0);
		}
		else
		{
			if (ds_found == -1)
			{
				if (free_record != -1)
				{
					ds_found = free_record;
					new_record = 0;
				}
				else
				{
					ds_found = num_characters;
					new_record = 1;
					character_data[num_characters++] = malloc(sizeof(L_CHARACTER_DATA));
				}
			}
			else
				new_record = 0;
			character_data[ds_found]->guildcard = client->guildcard;
			character_data[ds_found]->slot = slotnum;
			memcpy(&character_data[ds_found]->data, NewE7, sizeof(CHARDATA));
			memcpy(&character_data[ds_found]->header, &client->decryptbuf[0x10], 0x78);
			UpdateDataFile("character.dat", ds_found, character_data[ds_found], sizeof(L_CHARACTER_DATA), new_record);
		}
		PacketE4[0x00] = 0x10;
		PacketE4[0x02] = 0xE4;
		PacketE4[0x03] = 0x00;
		PacketE4[0x08] = slotnum;
		PacketE4[0x0C] = 0x00;
		cipher_ptr = &client->server_cipher;
		encryptcopy(client, &PacketE4[0], sizeof(PacketE4));
		ds_found = -1;
		for (ds = 0;ds < num_security;ds++)
		{
			if (security_data[ds]->guildcard == client->guildcard)
			{
				ds_found = ds;
				security_data[ds]->slotnum = slotnum;
				UpdateDataFile("security.dat", ds, security_data[ds], sizeof(L_SECURITY_DATA), 0);
				break;
			}
		}

		if (ds_found == -1)
		{
			Send1A("Could not select character.", client);
			client->todc = 1;
		}

#else

		sprintf_s(myQuery, _countof(myQuery), "SELECT * from character_data WHERE guildcard='%u' AND slot='%u'", client->guildcard, slotnum);
		//printf ("探测 AckCharacter_Creation MySQL 查询 %s\n", myQuery );

		if (!mysql_query(myData, &myQuery[0]))
		{
			myResult = mysql_store_result(myData);
			char_exists = (int)mysql_num_rows(myResult);
			if (char_exists)
			{
				if (client->dress_flag == 0)
				{
					// Delete character if recreating...如果重建角色则删除原角色
					sprintf_s(myQuery, _countof(myQuery), "DELETE from character_data WHERE guildcard='%u' AND slot ='%u'", client->guildcard, slotnum);
					mysql_query(myData, &myQuery[0]);
					sprintf_s(myQuery, _countof(myQuery), "DELETE from challenge_data WHERE guildcard='%u' AND slot ='%u'", client->guildcard, slotnum);
					mysql_query(myData, &myQuery[0]);
					sprintf_s(myQuery, _countof(myQuery), "DELETE from battle_data WHERE guildcard='%u' AND slot ='%u'", client->guildcard, slotnum);
					mysql_query(myData, &myQuery[0]);
				}
				else
				{
					// Updating character only...只是更新角色
					myRow = mysql_fetch_row(myResult);
					NewE7 = &E7_Work;
					memcpy(NewE7, myRow[2], sizeof(CHARDATA));
					memcpy(&NewE7->gcString[0], &clientchar->gcString[0], 0x68);
					*(long long*)&clientchar->unknown5[0] = *(long long*)&NewE7->unknown6[0];
					//NewE7->playTime = 0;新玩家游戏时间归零
					mysql_real_escape_string(myData, &chardata[0], &client->decryptbuf[0x10], 0x78);
					mysql_real_escape_string(myData, &E7chardata[0], (unsigned char*)NewE7, sizeof(CHARDATA));
				}
			}
			mysql_free_result(myResult);
		}
		else
		{
			Send1A("Could not check character information.\n\nThere is a problem with the MySQL database.", client);
			client->todc = 1;
			return;
		}
		if (client->dress_flag == 0)
			sprintf_s(myQuery, _countof(myQuery), "INSERT INTO character_data (guildcard,slot,data,header) VALUES ('%u','%u','%s','%s')", client->guildcard, slotnum, (char*)&E7chardata[0], (char*)&chardata[0]);
		else
		{
			debug("更新角色中...");
			sprintf_s(myQuery, _countof(myQuery), "UPDATE character_data SET data='%s', header='%s' WHERE guildcard='%u' AND slot='%u'", (char*)&E7chardata[0], (char*)&chardata[0], client->guildcard, slotnum);
		}
		if (!mysql_query(myData, &myQuery[0]))
		{
			PacketE4[0x00] = 0x10;
			PacketE4[0x02] = 0xE4;
			PacketE4[0x03] = 0x00;
			PacketE4[0x08] = slotnum;
			PacketE4[0x0C] = 0x00;
			cipher_ptr = &client->server_cipher;
			encryptcopy(client, &PacketE4[0], sizeof(PacketE4));
			sprintf_s(myQuery, _countof(myQuery), "UPDATE security_data SET slotnum = '%u' WHERE guildcard = '%u'", slotnum, client->guildcard);
			if (mysql_query(myData, &myQuery[0]))
			{
				Send1A("Could not select character.", client);
				client->todc = 1;
			}
		}
		else
		{
			Send1A("Could not save character to database.\nPlease contact the server administrator.", client);
			client->todc = 1;
		}
#endif
	}
	else
		client->todc = 1;
}

//用于角色选取界面获取小角色信息
void SendE4_E5(unsigned char slotnum, unsigned char selecting, BANANA* client)
{
	int char_exists = 0;
	MINICHAR* mc;

	if ((client->guildcard) && (slotnum < 0x04))
	{
#ifdef NO_SQL
		ds_found = -1;
		for (ds = 0;ds < num_characters;ds++)
		{
			if ((character_data[ds]->guildcard == client->guildcard) &&
				(character_data[ds]->slot == slotnum))
			{
				ds_found = ds;
				break;
			}
		}
		if (ds_found != -1)
		{
			char_exists = 1;
			if (!selecting)
			{
				mc = (MINICHAR*)&PacketE5[0x00];
				*(unsigned short*)&PacketE5[0x10] = character_data[ds_found]->data.level;  // Updated level
				memcpy(&PacketE5[0x14], &character_data[ds_found]->data.gcString[0], 0x70); // Updated data
				*(unsigned*)&PacketE5[0x84] = character_data[ds_found]->data.playTime;  // Updated playtime
				if (mc->skinFlag)
				{
					// In case we got bots that crashed themselves...
					mc->skin = 0;
					mc->head = 0;
					mc->hair = 0;
				}
			}
		}


#else
		sprintf_s(myQuery, _countof(myQuery), "SELECT * from character_data WHERE guildcard='%u' AND slot='%u'", client->guildcard, slotnum);
		//printf ("MySQL query %s\n", myQuery );

		// Check to see if the character exists in that slot.

		if (!mysql_query(myData, &myQuery[0]))
		{
			myResult = mysql_store_result(myData);
			char_exists = (int)mysql_num_rows(myResult);

			if (char_exists)
			{
				if (!selecting)
				{
					myRow = mysql_fetch_row(myResult);
					mc = (MINICHAR*)&PacketE5[0x00];
					memcpy(&PacketE5[0x10], myRow[2] + 0x36C, 2);  // Updated level 更新等级 16 2 (876-877) 2 2
																   //上面的意思是&PacketE5[0x10] 第16开始 myRow 2 的 876 877 写进去
					memcpy(&PacketE5[0x14], myRow[2] + 0x378, 0x70);  // Updated data 更新数据 20 2 (888-991)
																	  //从gcString[10]
					memcpy(&PacketE5[0x84], myRow[2] + 0x3E0, 4);     // Updated playtime 更新游戏时间 132 2 (992-995) 4 4
																	  //
																	  //memcpy(&PacketE5[0x84], myRow[2] + 0x3E0, 4);     // Updated playtime 更新游戏时间
					if (mc->skinFlag)
					{
						// In case we got bots that crashed themselves...万一我们有机器人撞毁自己
						mc->skin = 0;
						mc->head = 0;
						mc->hair = 0;
					}
				}
			}
			mysql_free_result(myResult);
		}
		else
		{
			Send1A("Could not check character information.\n\nThere is a problem with the MySQL database.", client);
			client->todc = 1;
			return;
		}
#endif

		if (!selecting)
		{
			if (char_exists)
			{
				PacketE5[0x00] = 0x88;
				PacketE5[0x02] = 0xE5;
				PacketE5[0x03] = 0x00;
				PacketE5[0x08] = slotnum;
				PacketE5[0x0C] = 0x00;
				if (client->sendCheck[SEND_PACKET_E5] < 0x04)
				{
					cipher_ptr = &client->server_cipher;
					encryptcopy(client, &PacketE5[0], sizeof(PacketE5));
					client->sendCheck[SEND_PACKET_E5] ++;
				}
				else
					client->todc = 1;
			}
			else
			{
				PacketE4[0x00] = 0x10;
				PacketE4[0x02] = 0xE4;
				PacketE4[0x03] = 0x00;
				PacketE4[0x08] = slotnum;
				PacketE4[0x0C] = 0x02;
				if (client->sendCheck[SEND_PACKET_E4] < 0x04)
				{
					cipher_ptr = &client->server_cipher;
					encryptcopy(client, &PacketE4[0], sizeof(PacketE4));
					client->sendCheck[SEND_PACKET_E4]++;
				}
				else
					client->todc = 1;
			}
		}
		else
		{
			if (char_exists)
			{
				PacketE4[0x00] = 0x10;
				PacketE4[0x02] = 0xE4;
				PacketE4[0x03] = 0x00;
				PacketE4[0x08] = slotnum;
				PacketE4[0x0C] = 0x01;
				if ((client->sendCheck[SEND_PACKET_E4] < 0x04) && (client->sendingchars == 0))
				{
					cipher_ptr = &client->server_cipher;
					encryptcopy(client, &PacketE4[0], sizeof(PacketE4));
#ifdef NO_SQL
					ds_found = -1;
					for (ds = 0;ds < num_security;ds++)
					{
						if (security_data[ds]->guildcard == client->guildcard)
						{
							ds_found = ds;
							security_data[ds]->slotnum = slotnum;
							UpdateDataFile("security.dat", ds, security_data[ds], sizeof(L_SECURITY_DATA), 0);
							break;
						}
					}

					if (ds_found == -1)
					{
						Send1A("Could not select character.", client);
						client->todc = 1;
					}
#else
					sprintf_s(myQuery, _countof(myQuery), "UPDATE security_data SET slotnum = '%u' WHERE guildcard = '%u'", slotnum, client->guildcard);
					if (mysql_query(myData, &myQuery[0]))
					{
						Send1A("Could not select character.", client);
						client->todc = 1;
					}
#endif
					client->sendCheck[SEND_PACKET_E4]++;
				}
				else
					client->todc = 1;
			}
			else
				client->todc = 1;
		}
	}
	else
		client->todc = 1;
}

//接收公会卡
void SendE8(BANANA* client)
{
	if ((client->guildcard) && (!client->sendCheck[SEND_PACKET_E8]))
	{
		cipher_ptr = &client->server_cipher;
		encryptcopy(client, &PacketE8[0], sizeof(PacketE8));
		client->sendCheck[SEND_PACKET_E8] = 1;
	}
	else
		client->todc = 1;
}

//设置公会卡信息
void SendEB(unsigned char subCommand, unsigned char EBOffset, BANANA* client)
{
	unsigned CalcOffset;

	if ((client->guildcard) && (client->sendCheck[SEND_PACKET_EB] < 17))
	{
		client->sendCheck[SEND_PACKET_EB]++;
		switch (subCommand)
		{
		case 0x01:
			cipher_ptr = &client->server_cipher;
			encryptcopy(client, &PacketEB01[0], PacketEB01_Total);
			break;
		case 0x02:
			cipher_ptr = &client->server_cipher;
			CalcOffset = (unsigned)EBOffset * 26636;
			if (CalcOffset < PacketEB02_Total)
			{
				if (PacketEB02_Total - CalcOffset >= 26636)
					encryptcopy(client, &PacketEB02[CalcOffset], 26636);
				else
					encryptcopy(client, &PacketEB02[CalcOffset], PacketEB02_Total - CalcOffset);
			}
			else
				client->todc = 1;
			break;
		}
	}
	else
		client->todc = 1;
}

//检查用户是否拥有公会卡ID
void SendDC(int sendChecksum, unsigned char PacketNum, BANANA* client)
{
	unsigned gc_ofs = 0,
		total_guilds = 0;
	unsigned friendid;
	unsigned short sectionid, _class;
	unsigned GCChecksum = 0;
	unsigned ch;
	unsigned CalcOffset;
	int numguilds = 0;
	unsigned to_send;

	if ((client->guildcard) && (client->sendCheck[SEND_PACKET_DC] < 0x04))
	{
		client->sendCheck[SEND_PACKET_DC]++;
		if (sendChecksum)
		{
#ifdef NO_SQL
			for (ds = 0;ds < num_guilds;ds++)
			{
				if (guild_data[ds]->accountid == client->guildcard)
				{
					if (total_guilds < 40)
					{
						friendid = guild_data[ds]->friendid;
						sectionid = guild_data[ds]->sectionid;
						_class = guild_data[ds]->pclass;
						for (ch = 0;ch < 444;ch++)
							client->guildcard_data[gc_ofs + ch] = 0x00;
						*(unsigned*)&client->guildcard_data[gc_ofs] = friendid;
						memcpy(&client->guildcard_data[gc_ofs + 0x04], &guild_data[ds]->friendname, 0x18);
						memcpy(&client->guildcard_data[gc_ofs + 0x54], &guild_data[ds]->friendtext, 0xB0);
						client->guildcard_data[gc_ofs + 0x104] = 0x01;
						client->guildcard_data[gc_ofs + 0x106] = (unsigned char)sectionid;
						*(unsigned short*)&client->guildcard_data[gc_ofs + 0x107] = _class;
						// comment @ 0x10C
						memcpy(&client->guildcard_data[gc_ofs + 0x10C], &guild_data[ds]->comment, 0x44);
						total_guilds++;
						gc_ofs += 444;
					}
					else
						break;
				}
			}
#else
			sprintf_s(myQuery, _countof(myQuery), "SELECT * from guild_data WHERE accountid='%u' ORDER BY priority", client->guildcard);

			//printf("探测 SendDC 指令的用途\n");
			//printf ("MySQL query %s\n", myQuery );

			// Check to see if the account has any guild cards.
			//检查帐户是否有公会卡

			if (!mysql_query(myData, &myQuery[0]))
			{

				myResult = mysql_store_result(myData);
				numguilds = (int)mysql_num_rows(myResult);

				if (numguilds)
				{
					while ((myRow = mysql_fetch_row(myResult)) && (total_guilds < 40))
					{
						friendid = atoi(myRow[1]);
						sectionid = (unsigned short)atoi(myRow[5]);
						_class = (unsigned short)atoi(myRow[6]);
						for (ch = 0;ch < 444;ch++)
							client->guildcard_data[gc_ofs + ch] = 0x00;
						*(unsigned*)&client->guildcard_data[gc_ofs] = friendid;
						memcpy(&client->guildcard_data[gc_ofs + 0x04], myRow[2], 0x18);
						memcpy(&client->guildcard_data[gc_ofs + 0x54], myRow[3], 0xB0);
						client->guildcard_data[gc_ofs + 0x104] = 0x01;
						client->guildcard_data[gc_ofs + 0x106] = (unsigned char)sectionid;
						*(unsigned short*)&client->guildcard_data[gc_ofs + 0x107] = _class;
						// comment @ 0x10C
						memcpy(&client->guildcard_data[gc_ofs + 0x10C], myRow[7], 0x44);
						total_guilds++;
						gc_ofs += 444;
					}
				}
				mysql_free_result(myResult);
			}
			else
			{
				Send1A("Could not check guild card information.\n\nThere is a problem with the MySQL database.", client);
				client->todc = 1;
				return;
			}
#endif

			if (total_guilds)
			{
				ch = 0x1F74 + (total_guilds * 444);
				memcpy(&PacketDC_Check[0x1F74], &client->guildcard_data[0], total_guilds * 444);
			}
			else
				ch = 0x1F74;
			for (ch;ch < 26624;ch++)
				PacketDC_Check[ch] = 0x00;
			PacketDC_Check[0x06] = 0x01;
			PacketDC_Check[0x07] = 0x02;
			GCChecksum = (unsigned)CalculateChecksum(&PacketDC_Check[0], 54672);
			PacketDC01[0x00] = 0x14;
			PacketDC01[0x02] = 0xDC;
			PacketDC01[0x03] = 0x01;
			PacketDC01[0x08] = 0x01;
			PacketDC01[0x0C] = 0x90;
			PacketDC01[0x0D] = 0xD5;
			*(unsigned*)&PacketDC01[0x10] = GCChecksum;
			cipher_ptr = &client->server_cipher;
			encryptcopy(client, &PacketDC01[0], sizeof(PacketDC01));
		}
		else
		{
			CalcOffset = ((unsigned)PacketNum * 26624);
			if (PacketNum > 0x02)
				client->todc = 1;
			else
			{
				if (PacketNum < 0x02)
					to_send = 26640;
				else
					to_send = 1440;
				*(unsigned short*)&PacketDC02[0x00] = to_send;
				PacketDC02[0x02] = 0xDC;
				PacketDC02[0x03] = 0x02;
				PacketDC02[0x0C] = PacketNum;
				memcpy(&client->encryptbuf[0x00], &PacketDC02[0], 0x10);
				memcpy(&client->encryptbuf[0x10], &PacketDC_Check[CalcOffset], to_send);
				cipher_ptr = &client->server_cipher;
				encryptcopy(client, &client->encryptbuf[0x00], to_send);
			}
		}
	}
	else
		client->todc = 1;
}

/* Ship start authentication */
//舰船服务器认证Key数据代码
const unsigned char RC4publicKey[32] = {
	103, 196, 247, 176, 71, 167, 89, 233, 200, 100, 044, 209, 190, 231, 83, 42,
	6, 95, 151, 28, 140, 243, 130, 61, 107, 234, 243, 172, 77, 24, 229, 156
};

//章节掉落表
void ShipSend0F(unsigned char episode, unsigned char part, ORANGE* ship)
{
	ship->encryptbuf[0x00] = 0x0F;
	ship->encryptbuf[0x01] = episode;
	ship->encryptbuf[0x02] = part;
	switch (episode)
	{
	case 0x01:
		memcpy(&ship->encryptbuf[0x03], &rt_tables_ep1[(sizeof(rt_tables_ep1) >> 3) * part], sizeof(rt_tables_ep1) >> 1);
		break;
	case 0x02:
		memcpy(&ship->encryptbuf[0x03], &rt_tables_ep2[(sizeof(rt_tables_ep2) >> 3) * part], sizeof(rt_tables_ep2) >> 1);
		break;
	case 0x03:
		memcpy(&ship->encryptbuf[0x03], &rt_tables_ep4[(sizeof(rt_tables_ep4) >> 3) * part], sizeof(rt_tables_ep4) >> 1);
		break;
	}
	compressShipPacket(ship, &ship->encryptbuf[0], 3 + (sizeof(rt_tables_ep1) >> 1));
}

//怪物出现几率
void ShipSend10(ORANGE* ship)
{
	ship->encryptbuf[0x00] = 0x10;
	ship->encryptbuf[0x01] = 0x00;
	memcpy(&ship->encryptbuf[0x02], &mob_rate[0], 32);
	compressShipPacket(ship, &ship->encryptbuf[0], 34);
}

void ShipSend11(ORANGE* ship)
{
	ship->encryptbuf[0x00] = 0x11;
	ship->encryptbuf[0x01] = 0x00;
	ship->encryptbuf[0x02] = 0x00;
	ship->encryptbuf[0x03] = 0x00;
	compressShipPacket(ship, &ship->encryptbuf[0], 4);
}

//RC4key
void ShipSend00(ORANGE* ship)
{
	unsigned char ch, ch2;

	ch2 = 0;

	for (ch = 0x18;ch < 0x58;ch += 2) // change 32 bytes of the key
	{
		ShipPacket00[ch] = (unsigned char)rand() % 255;
		ShipPacket00[ch + 1] = (unsigned char)rand() % 255;
		ship->key_change[ch2 + (ShipPacket00[ch] % 4)] = ShipPacket00[ch + 1];
		ch2 += 4;
	}
	compressShipPacket(ship, &ShipPacket00[0], sizeof(ShipPacket00));
	// use the public key to get the ship's index first...
	memcpy(&ship->user_key[0], &RC4publicKey[0], 32);
	prepare_key(&ship->user_key[0], 32, &ship->cs_key);
	prepare_key(&ship->user_key[0], 32, &ship->sc_key);
	ship->crypt_on = 1;
}

/* Ship authentication result */
//角色名称颜色
void ShipSend02(unsigned char result, ORANGE* ship)
{
	unsigned si, ch, shipNum;
	ORANGE* tempShip;

	ship->encryptbuf[0x00] = 0x02;
	ship->encryptbuf[0x01] = result;
	si = 0xFFFFFFFF;
	if (result == 0x01)
	{
		for (ch = 0;ch < serverNumShips;ch++)
		{
			shipNum = serverShipList[ch];
			tempShip = ships[shipNum];
			if (tempShip->shipSockfd == ship->shipSockfd)
			{
				si = shipNum;
				ship->shipID = 0xFFFFFFFF - shipNum;
				construct0xA0();
				break;
			}
		}
	}
	*(unsigned*)&ship->encryptbuf[0x02] = si;
	*(unsigned*)&ship->encryptbuf[0x06] = *(unsigned*)&ship->shipAddr[0];
	*(unsigned*)&ship->encryptbuf[0x0A] = quest_numallows;
	memcpy(&ship->encryptbuf[0x0E], quest_allow, quest_numallows * 4);
	si = 0x0E + (quest_numallows * 4);
	*(unsigned*)&ship->encryptbuf[si] = normalName;
	si += 4;
	*(unsigned*)&ship->encryptbuf[si] = localName;
	si += 4;
	*(unsigned*)&ship->encryptbuf[si] = globalName;
	si += 4;
	compressShipPacket(ship, &ship->encryptbuf[0x00], si);
}

//防止用户在不同的船出现
void ShipSend08(unsigned gcn, ORANGE* ship)
{
	// Tell the other ships this user logged on and to disconnect him/her if they're still active...
	ship->encryptbuf[0x00] = 0x08;
	ship->encryptbuf[0x01] = 0x00;
	*(unsigned*)&ship->encryptbuf[0x02] = gcn;
	compressShipPacket(ship, &ship->encryptbuf[0x00], 0x06);
}

//发送舰船列表代码
void ShipSend0D(unsigned char command, ORANGE* ship)
{
	unsigned shipNum;
	ORANGE* tship;

	switch (command)
	{
	case 0x00:
	{
		unsigned ch;
		// Send ship list data.
		unsigned short tempdw;

		tempdw = *(unsigned short*)&PacketA0Data[0];
		// Ship requesting the ship list.
		memcpy(&ship->encryptbuf[0x00], &ship->decryptbuf[0x04], 6);
		memcpy(&ship->encryptbuf[0x06], &PacketA0Data[0], tempdw);
		ship->encryptbuf[0x01] = 0x01;
		tempdw += 6;
		for (ch = 0;ch < serverNumShips;ch++)
		{
			shipNum = serverShipList[ch];
			tship = ships[shipNum];
			if ((tship->shipSockfd >= 0) && (tship->authed == 1))
			{
				*(unsigned*)&ship->encryptbuf[tempdw] = tship->shipID;
				tempdw += 4;
				*(unsigned*)&ship->encryptbuf[tempdw] = *(unsigned*)&tship->shipAddr[0];
				tempdw += 4;
				*(unsigned short*)&ship->encryptbuf[tempdw] = (unsigned short)tship->shipPort;
				tempdw += 2;
			}
		}
		//display_packet (&ship->encryptbuf[0x00], tempdw);
		compressShipPacket(ship, &ship->encryptbuf[0x00], tempdw);
	}
	break;
	default:
		break;
	}
}

//用于修复物品数据
void FixItem(ITEM* i)
{
	unsigned ch3;

	if (i->data[0] == 2) // Mag
	{
		MAG* m;
		short mDefense, mPower, mDex, mMind;
		int total_levels;

		m = (MAG*)&i->data[0];

		if (m->synchro > 120)
			m->synchro = 120;

		if (m->synchro < 0)
			m->synchro = 0;

		if (m->IQ > 200)
			m->IQ = 200;

		if ((m->defense < 0) || (m->power < 0) || (m->dex < 0) || (m->mind < 0))
			total_levels = 201;
		else
		{

			mDefense = m->defense / 100;
			mPower = m->power / 100;
			mDex = m->dex / 100;
			mMind = m->mind / 100;

			total_levels = mDefense + mPower + mDex + mMind;
		}

		if ((total_levels > 200) || (m->level > 200))
		{
			// Mag fails IRL, initialize it

			m->defense = 500;
			m->power = 0;
			m->dex = 0;
			m->mind = 0;
			m->level = 5;
			m->blasts = 0;
			m->IQ = 0;
			m->synchro = 20;
			m->mtype = 0;
			m->PBflags = 0;
		}
	}

	if (i->data[0] == 0) // Weapon
	{
		signed char percent_table[6];
		signed char percent;
		unsigned max_percents, num_percents;
		int srank;

		if ((i->data[1] == 0x33) ||  // SJS & Lame max 2 percents
			(i->data[1] == 0xAB))
			max_percents = 2;
		else
			max_percents = 3;

		srank = 0;
		memset(&percent_table[0], 0, 6);
		num_percents = 0;

		for (ch3 = 6;ch3 <= 4 + (max_percents * 2);ch3 += 2)
		{
			if (i->data[ch3] & 128)
			{
				srank = 1; // S-Rank S武器
				break;
			}

			if ((i->data[ch3]) &&
				(i->data[ch3] < 0x06))
			{
				// Percents over 100 or under -100 get set to 0 超过100或负数100都设为0
				percent = (char)i->data[ch3 + 1];
				if ((percent > 100) || (percent < -100))
					percent = 0;
				// Save percent 保存百分比
				percent_table[i->data[ch3]] =
					percent;
			}
		}

		if (!srank)
		{
			for (ch3 = 6;ch3 <= 4 + (max_percents * 2);ch3 += 2)
			{
				// Reset all %s 重设所有变量
				i->data[ch3] = 0;
				i->data[ch3 + 1] = 0;
			}

			for (ch3 = 1;ch3 <= 5;ch3++)
			{
				// Rebuild %s 重新构造
				if (percent_table[ch3])
				{
					i->data[6 + (num_percents * 2)] = ch3;
					i->data[7 + (num_percents * 2)] = (unsigned char)percent_table[ch3];
					num_percents++;
					if (num_percents == max_percents)
						break;
				}
			}
		}
	}
}


unsigned char gcname[24 * 2];
unsigned char gctext[176 * 2];
unsigned short gccomment[45] = { 0x305C };
unsigned char check_key[32];
unsigned char check_key2[32];

void ShipProcessPacket(ORANGE* ship)
{
	unsigned cv, shipNum;
	int pass;
	unsigned char sv;
	unsigned shop_checksum;
	int key_exists;
#ifndef NO_CONNECT_TEST
	int tempfd;
	struct sockaddr_in sa;
#endif

	switch (ship->decryptbuf[0x04])
	{
	case 0x01:
		// Authentication
		//
		// Remember to reconstruct the A0 packet upon a successful authentication!
		// Also, reset the playerCount.
		//
		//身份验证
		//
		//记住在成功的身份验证后重建A0数据包！
		//另外，重置playerCount。
		//
		pass = 1;
		for (sv = 0x06;sv < 0x14;sv++)
			if (ship->decryptbuf[sv] != ShipPacket00[sv - 0x04])
			{
				// Yadda
				pass = 0;
				break;
			}
		if (pass == 0)
		{
			printf("舰船服务器已连接,但收到无效的授权值.\n");
			ShipSend02(0x00, ship);
			ship->todc = 1;
		}
		else
		{
			//unsigned ch, sameIP;
			unsigned ch2, shipOK;
			//ORANGE* tship;

			shipOK = 1;

			memcpy(&ship->name[0], &ship->decryptbuf[0x2C], 12);
			ship->name[13] = 0x00;
			ship->playerCount = *(unsigned*)&ship->decryptbuf[0x38];
			if (ship->decryptbuf[0x3C] == 0x00)
				*(unsigned*)&ship->shipAddr[0] = *(unsigned*)&ship->listenedAddr[0];
			else
				*(unsigned*)&ship->shipAddr[0] = *(unsigned*)&ship->decryptbuf[0x3C];
			ship->shipAddr[5] = 0;
			ship->shipPort = *(unsigned short*)&ship->decryptbuf[0x40];

			/*
			for (ch=0;ch<serverNumShips;ch++)
			{
			shipNum = serverShipList[ch];
			tship = ships[shipNum];
			if ((tship->shipSockfd >= 0) && (tship->authed == 1))
			{
			sameIP = 1;
			for (ch2=0;ch2<4;ch2++)
			if (ship->shipAddr[ch2] != tship->shipAddr[ch2])
			sameIP = 0;
			if (sameIP == 1)
			{
			printf ("舰船IP地址已经注册了.  断开连接...\n");
			ShipSend02 (0x02, ship);
			ship->todc = 1;
			shipOK = 0;
			}
			}
			}*/

			if (shipOK == 1)
			{
				// if (ship->shipAddr[0] == 10)  shipOK = 0;
				// if ((ship->shipAddr[0] == 192) && (ship->shipAddr[1] == 168)) shipOK = 0;
				// if ((ship->shipAddr[0] == 127) && (ship->shipAddr[1] == 0) &&
				//    (ship->shipAddr[2] == 0)   && (ship->shipAddr[3] == 1)) shipOK = 0;

				shop_checksum = *(unsigned*)&ship->decryptbuf[0x42];
				memcpy(&check_key[0], &ship->decryptbuf[0x4A], 32);

				if (shop_checksum != 0xa3552fda)
				{
					ShipSend02(0x04, ship);
					ship->todc = 1;
					shipOK = 0;
				}
				else
					if (shipOK)
					{
						ship->key_index = *(unsigned*)&ship->decryptbuf[0x46];

						// update max ship key count on the fly
#ifdef NO_SQL
						max_ship_keys = 0;
						for (ds = 0;ds < num_shipkeys;ds++)
						{
							if (ship_data[ds]->idx >= max_ship_keys)
								max_ship_keys = ship_data[ds]->idx;
						}
#else

						sprintf_s(myQuery, _countof(myQuery), "SELECT * from ship_data");

						if (!mysql_query(myData, &myQuery[0]))
						{
							unsigned key_rows;

							myResult = mysql_store_result(myData);
							key_rows = (int)mysql_num_rows(myResult);
							max_ship_keys = 0;
							while (key_rows)
							{
								myRow = mysql_fetch_row(myResult);
								if ((unsigned)atoi(myRow[1]) > max_ship_keys)
									max_ship_keys = atoi(myRow[1]);
								key_rows--;
							}
							mysql_free_result(myResult);
						}
						else
						{
							ship->key_index = 0xFFFF; // MySQL problem, don't allow the ship to connect.
							printf("无法查询密钥数据库.\n");
						}

#endif

						if ((ship->key_index) && (ship->key_index <= max_ship_keys))
						{
							if (keys_in_use[ship->key_index])  // key already in use?
							{
								ShipSend02(0x06, ship);
								ship->todc = 1;
								shipOK = 0;
							}
							else
							{
								key_exists = 0;
#ifdef NO_SQL
								ds_found = -1;

								for (ds = 0;ds < num_shipkeys;ds++)
								{
									if (ship_data[ds]->idx == ship->key_index)
									{
										ds_found = ds;
										for (ch2 = 0;ch2 < 32;ch2++)
											if (ship_data[ds]->rc4key[ch2] != check_key[ch2])
											{
												ds_found = -1;
												break;
											}
										break;
									}
								}

								if (ds_found != -1)
									key_exists = 1;
								else
								{
									ShipSend02(0x05, ship); // Ship key doesn't exist
									ship->todc = 1;
									shipOK = 0;
								}
#else
								sprintf_s(myQuery, _countof(myQuery), "SELECT * from ship_data WHERE idx='%u'", ship->key_index);

								if (!mysql_query(myData, &myQuery[0]))
								{
									myResult = mysql_store_result(myData);
									key_exists = (int)mysql_num_rows(myResult);
									myRow = mysql_fetch_row(myResult);
									memcpy(&check_key2[0], myRow[0], 32); // 1024-bit key
									for (ch2 = 0;ch2 < 32;ch2++)
										if (check_key2[ch2] != check_key[ch2])
										{
											key_exists = 0;
											break;
										}
								}
								else
								{
									ShipSend02(0x05, ship); // Could not query MySQL or ship key doesn't exist
									mysql_free_result(myResult);
									ship->todc = 1;
									shipOK = 0;
								}
#endif

								if (key_exists)
								{
#ifndef NO_CONNECT_TEST
									tempfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

									memset(&sa, 0, sizeof(sa));
									sa.sin_family = AF_INET;
									*(unsigned*)&sa.sin_addr.s_addr = *(unsigned*)&ship->shipAddr[0];
									sa.sin_port = htons((unsigned short)ship->shipPort);

									if ((tempfd >= 0) && (connect(tempfd, (struct sockaddr*)&sa, sizeof(sa)) >= 0))
									{
										closesocket(tempfd);
#endif

										printf("舰船 %s (%u.%u.%u.%u:%u) 已成功登记注册.\n玩家数量: %u\n船舶密钥索引: %u\n", ship->name,
											ship->shipAddr[0], ship->shipAddr[1], ship->shipAddr[2], ship->shipAddr[3], ship->shipPort,
											ship->playerCount, ship->key_index);

										ship->authed = 1;
										ShipSend02(0x01, ship);  // At this point, we should change keys...

#ifdef NO_SQL
										memcpy(&ship->user_key[0], &ship_data[ds_found]->rc4key, 128);
#else
										memcpy(&ship->user_key[0], myRow[0], 128); // 1024-bit key
										mysql_free_result(myResult);
#endif

										// change keys

										for (ch2 = 0;ch2 < 128;ch2++)
											if (ship->key_change[ch2] != -1)
												ship->user_key[ch2] = (unsigned char)ship->key_change[ch2]; // update the key

										prepare_key(&ship->user_key[0], sizeof(ship->user_key), &ship->cs_key);
										prepare_key(&ship->user_key[0], sizeof(ship->user_key), &ship->sc_key);
#ifndef NO_CONNECT_TEST
									}
									else
									{
										debug("无法连接至已登记在册的舰船...  连接断开...\n");
										ShipSend02(0x03, ship);
										ship->todc = 1;
										shipOK = 0;
									}
#endif

								}
								else
								{
									ShipSend02(0x05, ship);
									ship->todc = 1;
									shipOK = 0;
								}

							}
						}
						else
							initialize_ship(ship);
					}
			}
		}
		break;
		//printf("探测 ShipProcessPacket 0x01 指令的用途\n");
	case 0x03:
		//WriteLog("登陆服务器未知 0x03");
		printf("探测 ShipProcessPacket 0x03 指令的用途\n");
		break;
	case 0x04:// 角色公会数据相关指令
		switch (ship->decryptbuf[0x05])
		{
			// 发送完整的角色数据 应该是换船的时候触发
		case 0x00:
		{
			// Send full player data here.
			unsigned guildcard;
			unsigned short slotnum;
			unsigned size;
			int char_exists = 0;
			int bank_exists = 0;
			int teamid;
			unsigned short privlevel;
			CHARDATA* PlayerData;
			unsigned shipid;
			int sockfd;

			guildcard = *(unsigned*)&ship->decryptbuf[0x06];

			slotnum = *(unsigned short*)&ship->decryptbuf[0x0A];
			sockfd = *(int*)&ship->decryptbuf[0x0C];
			shipid = *(unsigned*)&ship->decryptbuf[0x10];

			ship->encryptbuf[0x00] = 0x04;

			*(unsigned*)&ship->encryptbuf[0x02] = *(unsigned*)&ship->decryptbuf[0x06];
			*(unsigned short*)&ship->encryptbuf[0x06] = *(unsigned short*)&ship->decryptbuf[0x0A];
			*(unsigned*)&ship->encryptbuf[0x08] = *(unsigned*)&ship->decryptbuf[0x0C];

#ifdef NO_SQL
			size = 0x0C;

			ds_found = -1;

			for (ds = 0;ds < num_characters;ds++)
			{
				if ((character_data[ds]->guildcard == guildcard) &&
					(character_data[ds]->slot == slotnum))
				{
					ds_found = ds;
					break;
				}
			}

			if (ds_found == -1)
				ship->encryptbuf[0x01] = 0x02; // fail
			else
			{
				PlayerData = (CHARDATA*)&ship->encryptbuf[0x0C];
				memcpy(&ship->encryptbuf[0x0C], &character_data[ds_found]->data, sizeof(CHARDATA));
				size += sizeof(CHARDATA);
				ship->encryptbuf[0x01] = 0x01; // success

											   // Copy common bank into packet.

				ds_found = -1;

				for (ds = 0;ds < num_bankdata;ds++)
				{
					if (bank_data[ds]->guildcard == guildcard)
					{
						ds_found = ds;
						memcpy(&ship->encryptbuf[0x0C + sizeof(CHARDATA)], &bank_data[ds]->common_bank, sizeof(BANK));
						break;
					}
				}

				if (ds_found == -1)
				{
					// Common bank needs to be created.

					bank_data[num_bankdata] = malloc(sizeof(L_BANK_DATA));
					bank_data[num_bankdata]->guildcard = guildcard;
					memcpy(&bank_data[num_bankdata]->common_bank, &empty_bank, sizeof(BANK));
					memcpy(&ship->encryptbuf[0x0C + sizeof(CHARDATA)], &empty_bank, sizeof(BANK));
					UpdateDataFile("bank.dat", num_bankdata, bank_data[num_bankdata], sizeof(L_BANK_DATA), 1);
					num_bankdata++;
				}

				size += sizeof(BANK);

				ds_found = 1;
				for (ds = 0;ds < num_accounts;ds++)
				{
					if (account_data[ds]->guildcard == guildcard)
					{
						memcpy(&account_data[ds]->lastchar[0], &PlayerData->name[0], 24);
						teamid = account_data[ds]->teamid;
						privlevel = account_data[ds]->privlevel;
						UpdateDataFile("account.dat", ds, account_data[ds], sizeof(L_ACCOUNT_DATA), 0);
						break;
					}
				}

				if (teamid != -1)
				{
					//debug ("Retrieving some shit... ");
					// Store the team information in the E7 packet...
					PlayerData->guildCard2 = PlayerData->guildCard;
					PlayerData->teamID = (unsigned)teamid;
					PlayerData->privilegeLevel = privlevel;
					ds_found = -1;
					for (ds = 0;ds < num_teams;ds++)
					{
						if (team_data[ds]->teamid == teamid)
						{
							PlayerData->teamName[0] = 0x09;
							PlayerData->teamName[2] = 0x45;
							memcpy(&PlayerData->teamName[4], team_data[ds]->name, 24);
							memcpy(&PlayerData->teamFlag[0], team_data[ds]->flag, 2048);
							break;
						}
					}
				}
				else
					memset(&PlayerData->guildCard2, 0, 0x83C);

				PlayerData->unknown15 = 0x00986C84; // ??
				memset(&PlayerData->teamRewards[0], 0xFF, 4);
			}

			compressShipPacket(ship, &ship->encryptbuf[0x00], size);
#else

			sprintf_s(myQuery, _countof(myQuery), "SELECT * from character_data WHERE guildcard='%u' AND slot='%u'", guildcard, slotnum);

			//debug ("MySQL查询正在执行中... %s ", &myQuery[0] );

			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);
				char_exists = (int)mysql_num_rows(myResult);

				//数据包大小 12 字节
				size = 0x0C;

				//定义PlayerData为角色数据查询
				PlayerData = (CHARDATA*)&ship->encryptbuf[0x0C];

				//更新银行仓库信息（包括公共银行）
				if (char_exists)
				{
					myRow = mysql_fetch_row(myResult);
					memcpy(&ship->encryptbuf[0x0C], myRow[2], sizeof(CHARDATA));
					size += sizeof(CHARDATA);
					ship->encryptbuf[0x01] = 0x01; // success 成功
												   // Get the common bank or create it 获得公共银行或创建它
					sprintf_s(myQuery, _countof(myQuery), "SELECT * from bank_data WHERE guildcard='%u'", guildcard);

					//debug ("MySQL query executing ... %s ", &myQuery[0] );

					if (!mysql_query(myData, &myQuery[0]))
					{
						myResult = mysql_store_result(myData);
						bank_exists = (int)mysql_num_rows(myResult);

						if (bank_exists)
						{
							// Copy bank
							myRow = mysql_fetch_row(myResult);
							memcpy(&ship->encryptbuf[0x0C + sizeof(CHARDATA)], myRow[1], sizeof(BANK));
						}
						else
						{
							// Create bank
							memcpy(&ship->encryptbuf[0x0C + sizeof(CHARDATA)], &empty_bank, sizeof(BANK));
							mysql_real_escape_string(myData, &E7chardata[0], (unsigned char*)&empty_bank, sizeof(BANK));

							sprintf_s(myQuery, _countof(myQuery), "INSERT into bank_data (guildcard, data) VALUES ('%u','%s')", guildcard, (char*)&E7chardata[0]);
							if (mysql_query(myData, &myQuery[0]))
							{
								debug("无法为公会卡 %u 创建公用银行.", guildcard);
								return;
							}
						}
					}
					else
					{
						// Something goofed up, let's just copy the blank bank. 乱哄哄的，让我们复制空白银行 
						memcpy(&ship->encryptbuf[0x0C + sizeof(CHARDATA)], &empty_bank, sizeof(BANK));
					}

					size += sizeof(BANK);

					// Update the last used character info... 更新上一次使用时的角色信息

					mysql_real_escape_string(myData, &E7chardata[0], (unsigned char*)&PlayerData->name[0], 24);
					sprintf_s(myQuery, _countof(myQuery), "UPDATE account_data SET lastchar = '%s' WHERE guildcard = '%u'", (char*)&E7chardata[0], PlayerData->guildCard);
					mysql_query(myData, &myQuery[0]);

				}
				else {
					ship->encryptbuf[0x01] = 0x02; // 失败指令？
				}

				mysql_free_result(myResult);

				int challengeData_exists = 0;
				//查询挑战数据
				sprintf_s(myQuery, _countof(myQuery), "SELECT * from challenge_data WHERE guildcard='%u' AND slot='%u'", PlayerData->guildCard, slotnum);
				if (!mysql_query(myData, &myQuery[0]))
				{
					myResult = mysql_store_result(myData);
					challengeData_exists = (int)mysql_num_rows(myResult);

					//如果在数据中搜索到角色数据
					if (challengeData_exists)
					{
						myRow = mysql_fetch_row(myResult);
						memcpy(&ship->encryptbuf[0x0C + 0x2CC0], myRow[2], 320);//传送到相应的位置即可
						//display_packet(&ship->encryptbuf[0x0C + 0x2CC0], 320);
						//debug("为公会卡 %u 复制挑战数据.", guildcard);
					}
					else
					{
						memcpy(&ship->encryptbuf[0x0C + 0x2CC0], &PlayerData->challengeData, 320);//传送到相应的位置即可
						mysql_real_escape_string(myData, &E7chardata[0], (unsigned char*)&PlayerData->challengeData[0], 320);
						sprintf_s(myQuery, _countof(myQuery), "INSERT into challenge_data (guildcard, slot, data) VALUES ('%u','%u','%s')", PlayerData->guildCard, slotnum, (char*)&E7chardata[0]);
						//debug("完成 character_data 数据表操作... ");
						if (mysql_query(myData, &myQuery[0]))
						{
							debug("无法为公会卡 %u 创建挑战数据.", guildcard);
						}
					}
					sprintf_s(myQuery, _countof(myQuery), "DELETE from challenge_data WHERE guildcard='%u' AND slot ='%u'", PlayerData->guildCard, slotnum);
					mysql_query(myData, &myQuery[0]);

				}

				size += 320;

				mysql_free_result(myResult);

				int battleData_exists = 0;

				sprintf_s(myQuery, _countof(myQuery), "SELECT * from battle_data WHERE guildcard='%u' AND slot='%u'", PlayerData->guildCard, slotnum);
				//如果未开始查询
				if (!mysql_query(myData, &myQuery[0]))
				{
					myResult = mysql_store_result(myData);
					battleData_exists = (int)mysql_num_rows(myResult);

					//如果在数据中搜索到角色数据
					if (battleData_exists)
					{
						myRow = mysql_fetch_row(myResult);
						memcpy(&ship->encryptbuf[0x0C + 0x2E50], myRow[2], 92);//传送到相应的位置即可
					}
					else
					{
						memcpy(&ship->encryptbuf[0x0C + 0x2E50], &PlayerData->battleData, 92);//传送到相应的位置即可
						mysql_real_escape_string(myData, &E7chardata[0], (unsigned char*)&PlayerData->battleData[0], 92);
						sprintf_s(myQuery, _countof(myQuery), "INSERT into battle_data (guildcard, slot, data) VALUES ('%u','%u','%s')", PlayerData->guildCard, slotnum, (char*)&E7chardata[0]);
						if (mysql_query(myData, &myQuery[0]))
						{
							debug("无法为公会卡 %u 创建对战数据.", guildcard);
						}
					}
					sprintf_s(myQuery, _countof(myQuery), "DELETE from battle_data WHERE guildcard='%u' AND slot ='%u'", PlayerData->guildCard, slotnum);
					mysql_query(myData, &myQuery[0]);

				}

				size += 92;

				mysql_free_result(myResult);


				//debug("保存用户 %u 槽位:(%02x) 的数据信息", guildcard, slotnum);

				//解包舰船发来的数据 0x01 开始 如果不等于 2
				if (ship->encryptbuf[0x01] != 0x02)
				{
					//公会信息查询 teamid 公会ID ,privlevel 公会权限
					sprintf_s(myQuery, _countof(myQuery), "SELECT teamid,privlevel from account_data WHERE guildcard='%u'", guildcard);

					//如果未开始查询
					if (!mysql_query(myData, &myQuery[0]))
					{
						myResult = mysql_store_result(myData);
						myRow = mysql_fetch_row(myResult);

						teamid = atoi(myRow[0]); //(表示 ascii to integer)是把字符串myRow[0]转换成整型数teamid
						privlevel = atoi(myRow[1]);//(表示 ascii to integer)是把字符串myRow[1]转换成整型数privlevel

						mysql_free_result(myResult);

						//如果在公会中或拥有公会
						if (teamid != -1)
						{
							//debug ("正在检索一些信息 team_data 数据表操作... ");
							// Store the team information in the E7 packet...将团队信息存储在E7数据包中。。。
							PlayerData->serial_number = PlayerData->serial_number;
							PlayerData->teamID = (unsigned)teamid;
							PlayerData->privilegeLevel = privlevel;
							sprintf_s(myQuery, _countof(myQuery), "SELECT name,flag from team_data WHERE teamid = '%i'", teamid);
							if (!mysql_query(myData, &myQuery[0]))
							{
								myResult = mysql_store_result(myData);
								myRow = mysql_fetch_row(myResult);
								PlayerData->teamName[0] = 0x09;
								PlayerData->teamName[2] = 0x45;
								memcpy(&PlayerData->teamName[4], myRow[0], 24);
								memcpy(&PlayerData->teamFlag[0], myRow[1], 2048);

								mysql_free_result(myResult);
							}
						}
						else
							memset(&PlayerData->serial_number, 0, 0x83C);
					}
					else
						ship->encryptbuf[0x01] = 0x02; // fail

					PlayerData->teamRank = 0x00986C84; // ?? 应该是排行榜未完成的参数了
					memset(&PlayerData->teamRewards[0], 0xFF, 4);
				}				//解包舰船发来的数据 0x01 开始 如果不等于 2
				compressShipPacket(ship, &ship->encryptbuf[0x00], size);
			}
			else
				debug("无法为用户 %u 更新银行数据", guildcard);

#endif
		}
		//printf("探测 ShipProcessPacket 0x04 指令的用途 用户恢复银行 工会 数据\n");
		break;
		case 0x02:
		{
			unsigned guildcard, ch2;
			unsigned short slotnum;
			CHARDATA* character;

			unsigned short maxFace, maxHair, maxHairColorRed, maxHairColorBlue, maxHairColorGreen,
				maxCostume, maxSkin, maxHead;

			guildcard = *(unsigned*)&ship->decryptbuf[0x06];
			slotnum = *(unsigned short*)&ship->decryptbuf[0x0A];

			character = (CHARDATA*)&ship->decryptbuf[0x0C];

			// Update common bank (A common bank SHOULD exist since they've logged on...) 
			// 更新共同银行（一个共同的银行应该存在，因为他们已经登录） 

#ifdef NO_SQL

			for (ds = 0;ds < num_bankdata;ds++)
			{
				if (bank_data[ds]->guildcard == guildcard)
				{
					memcpy(&bank_data[ds]->common_bank, &ship->decryptbuf[0x0C + sizeof(CHARDATA)], sizeof(BANK));
					UpdateDataFile("bank.dat", ds, bank_data[ds], sizeof(L_BANK_DATA), 0);
					break;
				}
			}

#else
			mysql_real_escape_string(myData, &E7chardata[0], (unsigned char*)&ship->decryptbuf[0x0C + sizeof(CHARDATA)], sizeof(BANK));

			sprintf_s(myQuery, _countof(myQuery), "UPDATE bank_data set data = '%s' WHERE guildcard = '%u'", (char*)&E7chardata[0], guildcard);
			if (mysql_query(myData, &myQuery[0]))
			{
				debug("无法保存公会卡 %u 的仓库数据. \n", guildcard);
				return;
			}
#endif

			// Repair malformed data修复格式错误的数据


			character->name[0] = 0x09; // Filter colored names
			character->name[1] = 0x00;
			character->name[2] = 0x45;
			character->name[3] = 0x00;
			character->name[22] = 0x00;
			character->name[23] = 0x00; // Truncate names too long.
			if (character->level > 199) // Bad levels?
				character->level = 199;

			if ((character->_class == CLASS_HUMAR) ||
				(character->_class == CLASS_HUNEWEARL) ||
				(character->_class == CLASS_RAMAR) ||
				(character->_class == CLASS_RAMARL) ||
				(character->_class == CLASS_FOMARL) ||
				(character->_class == CLASS_FONEWM) ||
				(character->_class == CLASS_FONEWEARL) ||
				(character->_class == CLASS_FOMAR))
			{
				maxFace = 0x05;
				maxHair = 0x0A;
				maxHairColorRed = 0xFF;
				maxHairColorBlue = 0xFF;
				maxHairColorGreen = 0xFF;
				maxCostume = 0x11;
				maxSkin = 0x03;
				maxHead = 0x00;
			}
			else
			{
				maxFace = 0x00;
				maxHair = 0x00;
				maxHairColorRed = 0x00;
				maxHairColorBlue = 0x00;
				maxHairColorGreen = 0x00;
				maxCostume = 0x00;
				maxSkin = 0x18;
				maxHead = 0x04;
			}
			character->nameColorTransparency = 0xFF;
			if (character->sectionID > 0x09)
				character->sectionID = 0x00;
			if (character->proportionX > 0x3F800000)
				character->proportionX = 0x3F800000;
			if (character->proportionY > 0x3F800000)
				character->proportionY = 0x3F800000;
			if (character->face > maxFace)
				character->face = 0x00;
			if (character->hair > maxHair)
				character->hair = 0x00;
			if (character->hairColorRed > maxHairColorRed)
				character->hairColorRed = 0x00;
			if (character->hairColorBlue > maxHairColorBlue)
				character->hairColorBlue = 0x00;
			if (character->hairColorGreen > maxHairColorGreen)
				character->hairColorGreen = 0x00;
			if (character->costume > maxCostume)
				character->costume = 0x00;
			if (character->skin > maxSkin)
				character->skin = 0x00;
			if (character->head > maxHead)
				character->head = 0x00;

			// Let's fix hacked mags and weapons
			//让我们修复黑客攻击的武器和武器
			for (ch2 = 0;ch2 < character->inventoryUse;ch2++)
			{
				if (character->inventory[ch2].in_use)
					FixItem(&character->inventory[ch2].item);
			}

			for (ch2 = 0;ch2 < character->bankUse;ch2++)
			{
				if (character->inventory[ch2].in_use)
					FixItem((ITEM*)&character->bankInventory[ch2]);
			}

#ifdef NO_SQL
			for (ds = 0;ds < num_characters;ds++)
			{
				if ((character_data[ds]->guildcard == guildcard) &&
					(character_data[ds]->slot == slotnum))
				{
					memcpy(&character_data[ds]->data, &ship->decryptbuf[0x0C], sizeof(CHARDATA));
					UpdateDataFile("character.dat", ds, character_data[ds], sizeof(L_CHARACTER_DATA), 0);
					break;
				}
			}
#else

			mysql_real_escape_string(myData, &E7chardata[0], (unsigned char*)&ship->decryptbuf[0x0C], sizeof(CHARDATA));
			sprintf_s(myQuery, _countof(myQuery), "UPDATE character_data set data = '%s' WHERE guildcard = '%u' AND slot = '%u'", (char*)&E7chardata[0], guildcard, slotnum);
			if (mysql_query(myData, &myQuery[0]))
			{
				debug("无法保存用户 %u 槽位:(%02x) 的角色信息", guildcard, slotnum);
				return;
			}
			else
#endif
			{
				ship->encryptbuf[0x00] = 0x04;
				ship->encryptbuf[0x01] = 0x03;
				*(unsigned*)&ship->encryptbuf[0x02] = guildcard;
				// Send the OK, data saved.
				// 发送确认,数据已保存
				compressShipPacket(ship, &ship->encryptbuf[0x00], 0x06);
			}
#ifdef NO_SQL
			for (ds = 0;ds < num_keydata;ds++)
			{
				if (key_data[ds]->guildcard == guildcard)
				{
					memcpy(&key_data[ds]->controls, &ship->decryptbuf[0x2FCC], 420);
					UpdateDataFile("keydata.dat", ds, key_data[ds], sizeof(L_KEY_DATA), 0);
					break;
				}
			}
#else
			mysql_real_escape_string(myData, &E7chardata[0], (unsigned char*)&ship->decryptbuf[0x2FCC], 420); //12236
			sprintf_s(myQuery, _countof(myQuery), "UPDATE key_data set controls = '%s' WHERE guildcard = '%u'", (char*)&E7chardata[0], guildcard);
			if (mysql_query(myData, &myQuery[0]))
				debug(" 无法保存公会卡用户 %u 的控制信息 ", guildcard);

#endif
		}
		}
		//printf("探测 ShipProcessPacket 0x02 指令的用途 用户恢复背包和仓库\n");
		break;
	case 0x05://用于挑战数据
		printf("未知 ShipProcessPacket 0x05 指令\n");
		break;
	case 0x06://用于对战数据
		printf("未知 ShipProcessPacket 0x06 指令\n");
		break;
	case 0x07://好友相关代码
		switch (ship->decryptbuf[0x05])
		{
		case 0x00:
			// Add a guild card here.在这里加一张公会卡
		{
			unsigned clientGcn, friendGcn;
			int gc_priority;
#ifdef NO_SQL
			unsigned ch2;
#endif
#ifndef NO_SQL
			unsigned num_gcs;
			int gc_exists;
			unsigned char friendSecID, friendClass;
#endif

			clientGcn = *(unsigned*)&ship->decryptbuf[0x06];
			friendGcn = *(unsigned*)&ship->decryptbuf[0x0A];

#ifdef NO_SQL
			ds_found = -1;
			for (ds = 0;ds < num_guilds;ds++)
			{
				if ((guild_data[ds]->accountid == clientGcn) &&
					(guild_data[ds]->friendid == friendGcn))
				{
					ds_found = ds;
					break;
				}
			}
			gc_priority = 0;
			ch2 = 0;
			free_record = -1;
			for (ds = 0;ds < num_guilds;ds++)
			{
				if (guild_data[ds]->accountid == clientGcn)
				{
					ch2++;
					if (guild_data[ds]->priority > gc_priority)
						gc_priority = guild_data[ds]->priority;
				}
				if (guild_data[ds]->accountid == 0)
					free_record = ds;
			}
			gc_priority++;
			if ((ch2 < 40) || (ds_found != -1))
			{
				if (ds_found != -1)
					new_record = 0;
				else
				{
					if (free_record != -1)
						ds_found = free_record;
					else
					{
						new_record = 1;
						ds_found = num_guilds;
						guild_data[num_guilds++] = malloc(sizeof(L_GUILD_DATA));
					}
				}

				guild_data[ds_found]->sectionid = ship->decryptbuf[0xD6];
				guild_data[ds_found]->pclass = ship->decryptbuf[0xD7];

				gccomment[44] = 0x0000;

				guild_data[ds_found]->accountid = clientGcn;
				guild_data[ds_found]->friendid = friendGcn;
				guild_data[ds_found]->reserved = 257;
				memcpy(&guild_data[ds_found]->friendname[0], &ship->decryptbuf[0x0E], 24);
				memcpy(&guild_data[ds_found]->friendtext[0], &ship->decryptbuf[0x26], 176);
				memcpy(&guild_data[ds_found]->comment[0], &gccomment[0], 90);
				guild_data[ds_found]->priority = gc_priority;
				UpdateDataFile("guild.dat", ds_found, guild_data[ds_found], sizeof(L_GUILD_DATA), new_record);
			}
			else
			{
				// Card list full.
				ship->encryptbuf[0x00] = 0x07;
				ship->encryptbuf[0x01] = 0x00;
				*(unsigned*)&ship->encryptbuf[0x02] = clientGcn;
				compressShipPacket(ship, &ship->encryptbuf[0x00], 6);
			}

#else

			// Delete guild card if it exists... 删除公会卡（如果存在） 

			sprintf_s(myQuery, _countof(myQuery), "SELECT * from guild_data WHERE accountid='%u' AND friendid='%u'", clientGcn, friendGcn);

			//printf ("MySQL query %s\n", myQuery );

			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);
				gc_exists = (int)mysql_num_rows(myResult);
				mysql_free_result(myResult);
				if (gc_exists)
				{
					sprintf_s(myQuery, _countof(myQuery), "DELETE from guild_data WHERE accountid='%u' AND friendid='%u'", clientGcn, friendGcn);
					mysql_query(myData, &myQuery[0]);
				}
			}
			else
			{
				debug("无法删除用户 %u 的现有名片卡信息", clientGcn);
				return;
			}

			gc_priority = 1;

			sprintf_s(myQuery, _countof(myQuery), "SELECT * from guild_data WHERE accountid='%u'", clientGcn);

			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);

				num_gcs = (int)mysql_num_rows(myResult);
				mysql_free_result(myResult);

				if (num_gcs)
				{
					sprintf_s(myQuery, _countof(myQuery), "SELECT * from guild_data WHERE accountid='%u' ORDER by priority DESC", clientGcn);
					if (!mysql_query(myData, &myQuery[0]))
					{
						myResult = mysql_store_result(myData);
						myRow = mysql_fetch_row(myResult);
						gc_priority = atoi(myRow[8]) + 1;
						mysql_free_result(myResult);
					}
				}
			}
			else
			{
				debug("无法为用户 %u 选择现有的名片卡信息", clientGcn);
				return;
			}

			if (num_gcs < 40)
			{
				mysql_real_escape_string(myData, &gcname[0], &ship->decryptbuf[0x0E], 24);
				mysql_real_escape_string(myData, &gctext[0], &ship->decryptbuf[0x26], 176);

				friendSecID = ship->decryptbuf[0xD6];
				friendClass = ship->decryptbuf[0xD7];

				gccomment[44] = 0x0000;

				sprintf_s(myQuery, _countof(myQuery), "INSERT INTO guild_data (accountid,friendid,friendname,friendtext,sectionid,class,comment,priority) VALUES ('%u','%u','%s','%s','%u','%u','%s','%u')",
					clientGcn, friendGcn, (char*)&gcname[0], (char*)&gctext[0], friendSecID, friendClass, (char*)&gccomment[0], gc_priority);

				if (mysql_query(myData, &myQuery[0]))
					debug("无法将名片卡插入用户 %u 的数据库", clientGcn);
			}
			else
			{
				// Card list full.名片卡已满
				ship->encryptbuf[0x00] = 0x07;
				ship->encryptbuf[0x01] = 0x00;
				*(unsigned*)&ship->encryptbuf[0x02] = clientGcn;
				compressShipPacket(ship, &ship->encryptbuf[0x00], 6);
			}
#endif
		}
		break;
		case 0x01:
			// Delete a guild card here.
		{
			unsigned clientGcn, deletedGcn;

			clientGcn = *(unsigned*)&ship->decryptbuf[0x06];
			deletedGcn = *(unsigned*)&ship->decryptbuf[0x0A];

#ifdef NO_SQL
			for (ds = 0;ds < num_guilds;ds++)
			{
				if ((guild_data[ds]->accountid == clientGcn) &&
					(guild_data[ds]->friendid == deletedGcn))
				{
					guild_data[ds]->accountid = 0;
					UpdateDataFile("guild.dat", ds, guild_data[ds], sizeof(L_GUILD_DATA), 0);
					break;
				}
			}
#else

			sprintf_s(myQuery, _countof(myQuery), "DELETE from guild_data WHERE accountid = '%u' AND friendid = '%u'", clientGcn, deletedGcn);
			if (mysql_query(myData, &myQuery[0]))
				debug("无法为玩家 %u 删除名片卡", clientGcn);
#endif
		}
		break;
		case 0x02:
			// Modify guild card comment. 更新名片卡评论
		{
			unsigned clientGcn, friendGcn;

			clientGcn = *(unsigned*)&ship->decryptbuf[0x06];
			friendGcn = *(unsigned*)&ship->decryptbuf[0x0A];
#ifdef NO_SQL
			for (ds = 0;ds < num_guilds;ds++)
			{
				if ((guild_data[ds]->accountid == clientGcn) &&
					(guild_data[ds]->friendid == friendGcn))
				{
					memcpy(&guild_data[ds]->comment[0], &ship->decryptbuf[0x0E], 0x44);
					guild_data[ds]->comment[34] = 0; // ??
					UpdateDataFile("guild.dat", ds, guild_data[ds], sizeof(L_GUILD_DATA), 0);
				}
			}
#else
			mysql_real_escape_string(myData, &gctext[0], &ship->decryptbuf[0x0E], 0x44);

			sprintf_s(myQuery, _countof(myQuery), "UPDATE guild_data set comment = '%s' WHERE accountid = '%u' AND friendid = '%u'", (char*)&gctext[0], clientGcn, friendGcn);

			if (mysql_query(myData, &myQuery[0]))
				debug("无法为用户 %u 更新名片卡评论", clientGcn);
#endif
		}
		break;
		case 0x03:
			// Sort guild card 整理名片卡
		{
			unsigned clientGcn, gcn1, gcn2;
			int priority1, priority2, priority_save;
#ifdef NO_SQL
			L_GUILD_DATA tempgc;
#endif

			priority1 = -1;
			priority2 = -1;

			clientGcn = *(unsigned*)&ship->decryptbuf[0x06];
			gcn1 = *(unsigned*)&ship->decryptbuf[0x0A];
			gcn2 = *(unsigned*)&ship->decryptbuf[0x0E];

#ifdef NO_SQL
			for (ds = 0;ds < num_guilds;ds++)
			{
				if ((guild_data[ds]->accountid == clientGcn) &&
					(guild_data[ds]->friendid == gcn1))
				{
					priority1 = guild_data[ds]->priority;
					ds_found = ds;
					break;
				}
			}
#else
			sprintf_s(myQuery, _countof(myQuery), "SELECT * from guild_data WHERE accountid='%u' AND friendid='%u'", clientGcn, gcn1);

			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);
				if (mysql_num_rows(myResult))
				{
					myRow = mysql_fetch_row(myResult);
					priority1 = atoi(myRow[8]);
				}
				mysql_free_result(myResult);
			}
			else
			{
				debug("Could not select existing guild card information for user %u", clientGcn);
				return;
			}
#endif

#ifdef NO_SQL
			for (ds = 0;ds < num_guilds;ds++)
			{
				if ((guild_data[ds]->accountid == clientGcn) &&
					(guild_data[ds]->friendid == gcn2))
				{
					priority2 = guild_data[ds]->priority;
					ds2 = ds;
					break;
				}
			}
#else
			sprintf_s(myQuery, _countof(myQuery), "SELECT * from guild_data WHERE accountid='%u' AND friendid='%u'", clientGcn, gcn2);

			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);
				if (mysql_num_rows(myResult))
				{
					myRow = mysql_fetch_row(myResult);
					priority2 = atoi(myRow[8]);
				}
				mysql_free_result(myResult);
			}
			else
			{
				debug("Could not select existing guild card information for user %u", clientGcn);
				return;
			}
#endif

			if ((priority1 != -1) && (priority2 != -1))
			{
				priority_save = priority1;
				priority1 = priority2;
				priority2 = priority_save;

#ifdef NO_SQL
				guild_data[ds_found]->priority = priority2;
				guild_data[ds2]->priority = priority_save;
				UpdateDataFile("guild.dat", ds_found, guild_data[ds2], sizeof(L_GUILD_DATA), 0);
				UpdateDataFile("guild.dat", ds2, guild_data[ds_found], sizeof(L_GUILD_DATA), 0);
				memcpy(&tempgc, guild_data[ds_found], sizeof(L_GUILD_DATA));
				memcpy(guild_data[ds_found], guild_data[ds2], sizeof(L_GUILD_DATA));
				memcpy(guild_data[ds2], &tempgc, sizeof(L_GUILD_DATA));
#else
				sprintf_s(myQuery, _countof(myQuery), "UPDATE guild_data SET priority = '%u' WHERE accountid = '%u' AND friendid = '%u'", priority1, clientGcn, gcn1);
				if (mysql_query(myData, &myQuery[0]))
					debug("Could not update guild card sort information for user %u", clientGcn);
				sprintf_s(myQuery, _countof(myQuery), "UPDATE guild_data SET priority = '%u' WHERE accountid = '%u' AND friendid = '%u'", priority2, clientGcn, gcn2);
				if (mysql_query(myData, &myQuery[0]))
					debug("Could not update guild card sort information for user %u", clientGcn);
#endif
			}
		}
		break;
		}
		break;
	case 0x08://搜索舰船中名片
		switch (ship->decryptbuf[0x05])
		{
		case 0x01:
			// Ship requesting a guild search 在舰船中发送一个名片搜索申请
		{
			unsigned clientGcn, friendGcn, ch, teamid;
			int gc_exists = 0;
			ORANGE* tship;

			friendGcn = *(unsigned*)&ship->decryptbuf[0x06];
			clientGcn = *(unsigned*)&ship->decryptbuf[0x0A];
			teamid = *(unsigned*)&ship->decryptbuf[0x12];

			// First let's be sure our friend has this person's guild card....

#ifdef NO_SQL
			for (ds = 0;ds < num_guilds;ds++)
			{
				if ((guild_data[ds]->accountid == clientGcn) &&
					(guild_data[ds]->friendid == friendGcn))
				{
					gc_exists = 1;
					break;
				}
			}
#else

			sprintf_s(myQuery, _countof(myQuery), "SELECT * from guild_data WHERE accountid='%u' AND friendid='%u'", clientGcn, friendGcn);

			//printf ("MySQL query %s\n", myQuery );

			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);
				gc_exists = (int)mysql_num_rows(myResult);
				mysql_free_result(myResult);
			}
			else
			{
				debug("无法为用户 %u 选择已存在的公会卡信息", clientGcn);
				return;
			}
#endif

#ifdef NO_SQL
			if ((gc_exists == 0) && (teamid != 0))
			{
				for (ds = 0;ds < num_accounts;ds++)
				{
					if ((account_data[ds]->guildcard == friendGcn) &&
						(account_data[ds]->teamid == teamid))
					{
						gc_exists = 1;
						break;
					}
				}
			}
#else
			if ((gc_exists == 0) && (teamid != 0))
			{
				// Well, they don't appear to have this person's guild card... so let's check the team list...
				sprintf_s(myQuery, _countof(myQuery), "SELECT * from account_data WHERE guildcard='%u' AND teamid='%u'", friendGcn, teamid);

				//printf ("MySQL query %s\n", myQuery );

				if (!mysql_query(myData, &myQuery[0]))
				{
					myResult = mysql_store_result(myData);
					gc_exists = (int)mysql_num_rows(myResult);
					mysql_free_result(myResult);
				}
				else
				{
					debug("无法为用户 %u 选择角色信息", friendGcn);
					return;
				}
			}
#endif

			if (gc_exists)
			{
				// 确认!  Let's tell the ships to do the search...
				for (ch = 0;ch < serverNumShips;ch++)
				{
					shipNum = serverShipList[ch];
					tship = ships[shipNum];
					if ((tship->shipSockfd >= 0) && (tship->authed == 1))
						compressShipPacket(tship, &ship->decryptbuf[0x04], 0x1D);
				}
			}
		}
		break;
		case 0x02:
			// Got a hit on a guild search
			cv = *(unsigned*)&ship->decryptbuf[0x0A];
			cv--;
			if (cv < serverMaxShips)
			{
				ORANGE* tship;

				tship = ships[cv];
				if ((tship->shipSockfd >= 0) && (tship->authed == 1))
					compressShipPacket(tship, &ship->decryptbuf[0x04], 0x140);
			}
			break;
		case 0x03:
			// Send mail
		{
			unsigned clientGcn, friendGcn, ch, teamid;
			int gc_exists = 0;
			ORANGE* tship;

			friendGcn = *(unsigned*)&ship->decryptbuf[0x36];
			clientGcn = *(unsigned*)&ship->decryptbuf[0x12];
			teamid = *(unsigned*)&ship->decryptbuf[0x462];

			// First let's be sure our friend has this person's guild card....

#ifdef NO_SQL
			for (ds = 0;ds < num_guilds;ds++)
			{
				if ((guild_data[ds]->accountid == clientGcn) &&
					(guild_data[ds]->friendid == friendGcn))
				{
					gc_exists = 1;
					break;
				}
			}
#else
			sprintf_s(myQuery, _countof(myQuery), "SELECT * from guild_data WHERE accountid='%u' AND friendid='%u'", clientGcn, friendGcn);

			//printf ("MySQL query %s\n", myQuery );

			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);
				gc_exists = (int)mysql_num_rows(myResult);
				mysql_free_result(myResult);
			}
			else
			{
				debug("无法为用户 %u 选择现有名片信息", clientGcn);
				return;
			}
#endif


#ifdef NO_SQL
			if ((gc_exists == 0) && (teamid != 0))
			{
				for (ds = 0;ds < num_accounts;ds++)
				{
					if ((account_data[ds]->guildcard == friendGcn) &&
						(account_data[ds]->teamid == teamid))
					{
						gc_exists = 1;
						break;
					}
				}
			}
#else
			if ((gc_exists == 0) && (teamid != 0))
			{
				// Well, they don't appear to have this person's guild card... so let's check the team list...
				sprintf_s(myQuery, _countof(myQuery), "SELECT * from account_data WHERE guildcard='%u' AND teamid='%u'", friendGcn, teamid);

				//printf ("MySQL query %s\n", myQuery );

				if (!mysql_query(myData, &myQuery[0]))
				{
					myResult = mysql_store_result(myData);
					gc_exists = (int)mysql_num_rows(myResult);
					mysql_free_result(myResult);
				}
				else
				{
					debug("无法为用户 %u 选择现有帐户信息", friendGcn);
					return;
				}
			}
#endif

			if (gc_exists)
			{
				// 确认!  Let's tell the ships to do the search...
				for (ch = 0;ch < serverNumShips;ch++)
				{
					shipNum = serverShipList[ch];
					tship = ships[shipNum];
					if ((tship->shipSockfd >= 0) && (tship->authed == 1))
						compressShipPacket(tship, &ship->decryptbuf[0x04], 0x45E);
				}
			}
		}
		break;
		case 0x04:
			// Flag account 旗帜账户
			printf("未知 ShipProcessPacket 0x0A 指令\n");
			break;
		case 0x05:
			// Ban guild card. 封禁公会卡号码
			break;
		case 0x06:
			// Ban IP address. 封禁IP地址
			break;
		case 0x07:
			// Ban HW info. 封禁HW信息
			break;
		}
		break;
	case 0x09:
		// Reserved for team functions.为公会功能预留
		switch (ship->decryptbuf[0x05])
		{
		case 0x00:
		{
			// Create Team 创建公会
			//
			// 0x06 = Team name (pre-stripped) 团队名称 (预分离)
			// 0x1E = Guild card of creator 创建者的公会卡
			//
			//
			unsigned char CreateResult;
			int team_exists, teamid;
			unsigned highid;
			unsigned gcn;
#ifndef NO_SQL
			unsigned char TeamNameCheck[50];
#else
			unsigned short char_check;
			int match;
#endif

			gcn = *(unsigned*)&ship->decryptbuf[0x1E];
			// First check to see if the team exists...

			team_exists = 0;
			highid = 0;

#ifdef NO_SQL
			free_record = -1;
			for (ds = 0;ds < num_teams;ds++)
			{
				if (team_data[ds]->owner == 0)
					free_record = ds;
				if (team_data[ds]->teamid >= highid)
					highid = team_data[ds]->teamid;
				match = 1;
				for (ds2 = 0;ds2 < 12;ds2++)
				{
					char_check = *(unsigned short*)&ship->decryptbuf[0x06 + (ds2 * 2)];
					if (team_data[ds]->name[ds2] != char_check)
					{
						match = 0;
						break;
					}
					if (char_check == 0x0000)
						break;
				}
				if (match)
				{
					team_exists = 1;
					break;
				}
			}
#else
			mysql_real_escape_string(myData, &TeamNameCheck[0], &ship->decryptbuf[0x06], 24);
			sprintf_s(myQuery, _countof(myQuery), "SELECT * from team_data WHERE name='%s'", (char*)&TeamNameCheck[0]);
			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);
				team_exists = (int)mysql_num_rows(myResult);
				mysql_free_result(myResult);
			}
			else
				CreateResult = 1;
#endif

			if (!team_exists)
			{
				// It doesn't... but it will now. :)
#ifdef NO_SQL
				if (free_record != -1)
				{
					new_record = 0;
					ds_found = free_record;
				}
				else
				{
					new_record = 1;
					ds_found = num_teams;
					team_data[num_teams++] = malloc(sizeof(L_TEAM_DATA));
				}
				memcpy(&team_data[ds_found]->name[0], &ship->decryptbuf[0x06], 24);
				memcpy(&team_data[ds_found]->flag, &DefaultTeamFlag, 2048);
				team_data[ds_found]->owner = gcn;
				highid++;
				team_data[ds_found]->teamid = teamid = highid;
				UpdateDataFile("team.dat", ds_found, team_data[ds_found], sizeof(L_TEAM_DATA), new_record);
				CreateResult = 0;
				for (ds = 0;ds < num_accounts;ds++)
				{
					if (account_data[ds]->guildcard == gcn)
					{
						account_data[ds]->teamid = highid;
						account_data[ds]->privlevel = 0x40;
						UpdateDataFile("account.dat", ds, account_data[ds], sizeof(L_ACCOUNT_DATA), 0);
						break;
					}
				}
#else
				sprintf_s(myQuery, _countof(myQuery), "INSERT into team_data (name,owner,flag) VALUES ('%s','%u','%s')", (char*)&TeamNameCheck[0], gcn, (char*)&DefaultTeamFlagSlashes[0]);
				if (!mysql_query(myData, &myQuery[0]))
				{
					teamid = (unsigned)mysql_insert_id(myData);
					sprintf_s(myQuery, _countof(myQuery), "UPDATE account_data SET teamid='%u', privlevel='%u' WHERE guildcard='%u'", teamid, 0x40, gcn);
					if (mysql_query(myData, &myQuery[0]))
						CreateResult = 1;
					else
						CreateResult = 0;
				}
				else
					CreateResult = 1;
#endif
			}
			else
				CreateResult = 2;
			// 1 = MySQL error
			// 2 = Team Exists
			ship->encryptbuf[0x00] = 0x09;
			ship->encryptbuf[0x01] = 0x00;
			ship->encryptbuf[0x02] = CreateResult;
			*(unsigned*)&ship->encryptbuf[0x03] = gcn;
			memcpy(&ship->encryptbuf[0x07], &DefaultTeamFlag[0], 0x800);
			memcpy(&ship->encryptbuf[0x807], &ship->decryptbuf[0x06], 24);
			*(unsigned*)&ship->encryptbuf[0x81F] = teamid;
			compressShipPacket(ship, &ship->encryptbuf[0x00], 0x823);
		}
		break;
		case 0x01:
			// Update Team Flag 更新公会旗帜
			//
			// 0x06 = Flag (2048/0x800) bytes 貌似是图像
			// 0x806 = Guild card of invoking person
			//
		{
			unsigned teamid;

			teamid = *(unsigned*)&ship->decryptbuf[0x806];
#ifdef NO_SQL
			for (ds = 0;ds < num_teams;ds++)
			{
				if (team_data[ds]->teamid == teamid)
				{
					memcpy(&team_data[ds]->flag, &ship->decryptbuf[0x06], 0x800);
					ship->encryptbuf[0x00] = 0x09;
					ship->encryptbuf[0x01] = 0x01;
					ship->encryptbuf[0x02] = 0x01;
					*(unsigned*)&ship->encryptbuf[0x03] = teamid;
					memcpy(&ship->encryptbuf[0x07], &ship->decryptbuf[0x06], 0x800);
					compressShipPacket(ship, &ship->encryptbuf[0x00], 0x807);
					UpdateDataFile("team.dat", ds, team_data[ds], sizeof(L_TEAM_DATA), 0);
					break;
				}
			}
#else
			mysql_real_escape_string(myData, &FlagSlashes[0], &ship->decryptbuf[0x06], 0x800);
			sprintf_s(myQuery, _countof(myQuery), "UPDATE team_data SET flag='%s' WHERE teamid='%u'", (char*)&FlagSlashes[0], teamid);
			if (!mysql_query(myData, &myQuery[0]))
			{
				ship->encryptbuf[0x00] = 0x09; //0
				ship->encryptbuf[0x01] = 0x01; //1
				ship->encryptbuf[0x02] = 0x01; //2
				*(unsigned*)&ship->encryptbuf[0x03] = teamid;//3
				memcpy(&ship->encryptbuf[0x07], &ship->decryptbuf[0x06], 0x800);//7 6 2048
				compressShipPacket(ship, &ship->encryptbuf[0x00], 0x807);//Dst Src 2055
			}
			else
			{
				debug("Could not update team flag for team %u", teamid);
				return;
			}
#endif

		}
		break;
		case 0x02:
			// Dissolve Team
			//
			// 0x06 = Guild card of invoking person
			//
		{
			unsigned teamid;

			teamid = *(unsigned*)&ship->decryptbuf[0x06];
#ifdef NO_SQL
			for (ds = 0;ds < num_teams;ds++)
			{
				if (team_data[ds]->teamid == teamid)
				{
					team_data[ds]->teamid = 0;
					team_data[ds]->owner = 0;
					UpdateDataFile("team.dat", ds, team_data[ds], sizeof(L_TEAM_DATA), 0);
					break;
				}
			}

			for (ds = 0;ds < num_accounts;ds++)
			{
				if (account_data[ds]->teamid == teamid)
					account_data[ds]->teamid = -1;
				UpdateDataFile("account.dat", ds, account_data[ds], sizeof(L_ACCOUNT_DATA), 0);
			}
#else
			sprintf_s(myQuery, _countof(myQuery), "DELETE from team_data WHERE teamid='%u'", teamid);
			mysql_query(myData, &myQuery[0]);
			sprintf_s(myQuery, _countof(myQuery), "UPDATE account_data SET teamid='-1' WHERE teamid='%u'", teamid);
			mysql_query(myData, &myQuery[0]);
#endif
			ship->encryptbuf[0x00] = 0x09;
			ship->encryptbuf[0x01] = 0x02;
			ship->encryptbuf[0x02] = 0x01;
			*(unsigned*)&ship->encryptbuf[0x03] = teamid;
			compressShipPacket(ship, &ship->encryptbuf[0x00], 7);
		}
		break;
		case 0x03:
			// Remove member 移除会员
			//
			// 0x06 = Team ID 公会ID
			// 0x0A = Guild card 公会卡
			//
		{
			unsigned gcn, teamid;

			teamid = *(unsigned*)&ship->decryptbuf[0x06];
			gcn = *(unsigned*)&ship->decryptbuf[0x0A];
#ifdef NO_SQL
			for (ds = 0;ds < num_accounts;ds++)
			{
				if ((account_data[ds]->guildcard == gcn) &&
					(account_data[ds]->teamid == teamid))
				{
					account_data[ds]->teamid = -1;
					UpdateDataFile("account.dat", ds, account_data[ds], sizeof(L_ACCOUNT_DATA), 0);
					break;
				}
			}
#else
			sprintf_s(myQuery, _countof(myQuery), "UPDATE account_data SET teamid='-1' WHERE guildcard='%u' AND teamid = '%u'", gcn, teamid);
			mysql_query(myData, &myQuery[0]);
#endif
			ship->encryptbuf[0x00] = 0x09;
			ship->encryptbuf[0x01] = 0x03;
			ship->encryptbuf[0x02] = 0x01;
			compressShipPacket(ship, &ship->encryptbuf[0x00], 3);
		}
		break;
		case 0x04:
			// Team Chat 公会聊天
		{
			ORANGE* tship;
			unsigned size, ch;

			size = *(unsigned*)&ship->decryptbuf[0x00];
			size -= 4;

			// Just pass the packet along... 直接发送数据
			for (ch = 0;ch < serverNumShips;ch++)
			{
				shipNum = serverShipList[ch];
				tship = ships[shipNum];
				if ((tship->shipSockfd >= 0) && (tship->authed == 1))
					compressShipPacket(tship, &ship->decryptbuf[0x04], size);
			}
		}
		break;
		case 0x05:
			// Request team list 请求公会列表
		{
			unsigned teamid, packet_offset;
			int num_mates;
#ifndef NO_SQL
			int ch;
			unsigned guildcard, privlevel;
#else
			unsigned save_offset;
#endif


			teamid = *(unsigned*)&ship->decryptbuf[0x06];
			ship->encryptbuf[0x00] = 0x09;
			ship->encryptbuf[0x01] = 0x05;
			*(unsigned*)&ship->encryptbuf[0x02] = teamid;
			*(unsigned*)&ship->encryptbuf[0x06] = *(unsigned*)&ship->decryptbuf[0x0A];
			// @ 0x0A store the size
			ship->encryptbuf[0x0C] = 0xEA;
			ship->encryptbuf[0x0D] = 0x09;
			memset(&ship->encryptbuf[0x0E], 0, 4);
			packet_offset = 0x12;
#ifdef NO_SQL
			save_offset = packet_offset;
			packet_offset += 4;
			num_mates = 0;
			for (ds = 0;ds < num_accounts;ds++)
			{
				if (account_data[ds]->teamid == teamid)
				{
					num_mates++;
					*(unsigned*)&ship->encryptbuf[packet_offset] = num_mates;
					packet_offset += 4;
					*(unsigned*)&ship->encryptbuf[packet_offset] = account_data[ds]->privlevel;
					packet_offset += 4;
					*(unsigned*)&ship->encryptbuf[packet_offset] = account_data[ds]->guildcard;
					packet_offset += 4;
					memcpy(&ship->encryptbuf[packet_offset], &account_data[ds]->lastchar, 24);
					packet_offset += 24;
					memset(&ship->encryptbuf[packet_offset], 0, 8);
					packet_offset += 8;
				}
			}
			*(unsigned*)&ship->encryptbuf[save_offset] = num_mates;
			packet_offset -= 0x0A;
			*(unsigned short*)&ship->encryptbuf[0x0A] = (unsigned short)packet_offset;
			packet_offset += 0x0A;
			compressShipPacket(ship, &ship->encryptbuf[0x00], packet_offset);
#else
			sprintf_s(myQuery, _countof(myQuery), "SELECT guildcard,privlevel,lastchar from account_data WHERE teamid='%u'", teamid);
			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);
				num_mates = (int)mysql_num_rows(myResult);
				*(unsigned*)&ship->encryptbuf[packet_offset] = num_mates;
				packet_offset += 4;
				for (ch = 1;ch <= num_mates;ch++)
				{
					myRow = mysql_fetch_row(myResult);
					guildcard = atoi(myRow[0]);
					privlevel = atoi(myRow[1]);
					*(unsigned*)&ship->encryptbuf[packet_offset] = ch;
					packet_offset += 4;
					*(unsigned*)&ship->encryptbuf[packet_offset] = privlevel;
					packet_offset += 4;
					*(unsigned*)&ship->encryptbuf[packet_offset] = guildcard;
					packet_offset += 4;
					memcpy(&ship->encryptbuf[packet_offset], myRow[2], 24);
					packet_offset += 24;
					memset(&ship->encryptbuf[packet_offset], 0, 8);
					packet_offset += 8;
				}
				mysql_free_result(myResult);
				packet_offset -= 0x0A;
				*(unsigned short*)&ship->encryptbuf[0x0A] = (unsigned short)packet_offset;
				packet_offset += 0x0A;
				compressShipPacket(ship, &ship->encryptbuf[0x00], packet_offset);
			}
			else
			{
				debug("未能找到 %u 队伍信息", teamid);
				return;
			}
#endif
		}
		break;
		case 0x06://公会成员提升
				  // Promote member
				  //
				  // 0x06 = Team ID
				  // 0x0A = Guild card
				  // 0x0B = New level
				  //
		{
			unsigned gcn, teamid;
			unsigned char privlevel;

			teamid = *(unsigned*)&ship->decryptbuf[0x06];
			gcn = *(unsigned*)&ship->decryptbuf[0x0A];
			privlevel = (unsigned char)ship->decryptbuf[0x0E];
#ifdef NO_SQL
			for (ds = 0;ds < num_accounts;ds++)
			{
				if ((account_data[ds]->guildcard == gcn) &&
					(account_data[ds]->teamid == teamid))
				{
					account_data[ds]->privlevel = privlevel;
					UpdateDataFile("account.dat", ds, account_data[ds], sizeof(L_ACCOUNT_DATA), 0);
					break;
				}
			}

			if (privlevel == 0x40)
			{
				for (ds = 0;ds < num_accounts;ds++)
				{
					if (team_data[ds]->teamid == teamid)
					{
						team_data[ds]->owner = gcn;
						UpdateDataFile("team.dat", ds, team_data[ds], sizeof(L_TEAM_DATA), 0);
						break;
					}
				}
			}
#else
			sprintf_s(myQuery, _countof(myQuery), "UPDATE account_data SET privlevel='%u' WHERE guildcard='%u' AND teamid='%u'", privlevel, gcn, teamid);
			mysql_query(myData, &myQuery[0]);
			if (privlevel == 0x40)  // Master Transfer
				sprintf_s(myQuery, _countof(myQuery), "UPDATE team_data SET owner='%u' WHERE teamid='%u'", gcn, teamid);
			mysql_query(myData, &myQuery[0]);
#endif
			ship->encryptbuf[0x00] = 0x09;
			ship->encryptbuf[0x01] = 0x06;
			ship->encryptbuf[0x02] = 0x01;
			compressShipPacket(ship, &ship->encryptbuf[0x00], 3);
		}
		break;
		case 0x07: //新增公会会员
				   // Add member
				   //
				   // 0x06 = Team ID
				   // 0x0A = Guild card
				   //
		{
			unsigned gcn, teamid;

			teamid = *(unsigned*)&ship->decryptbuf[0x06];
			gcn = *(unsigned*)&ship->decryptbuf[0x0A];
#ifdef NO_SQL
			for (ds = 0;ds < num_accounts;ds++)
			{
				if (account_data[ds]->guildcard == gcn)
				{
					account_data[ds]->teamid = teamid;
					account_data[ds]->privlevel = 0;
					UpdateDataFile("account.dat", ds, account_data[ds], sizeof(L_ACCOUNT_DATA), 0);
					break;
				}
			}
#else
			sprintf_s(myQuery, _countof(myQuery), "UPDATE account_data SET teamid='%u', privlevel='0' WHERE guildcard='%u'", teamid, gcn);
			mysql_query(myData, &myQuery[0]);
#endif
			ship->encryptbuf[0x00] = 0x09;
			ship->encryptbuf[0x01] = 0x07;
			ship->encryptbuf[0x02] = 0x01;
			compressShipPacket(ship, &ship->encryptbuf[0x00], 3);
		}
		break;
		}
		break;
	case 0x0A:
		printf("未知 ShipProcessPacket 0x0A 指令\n");
		break;
	case 0x0B:// Checking authentication... 检查认证信息
		switch (ship->decryptbuf[0x05])
		{
		case 0x00:
		{
			int security_client_thirtytwo, security_thirtytwo_check;
			long long security_client_sixtyfour, security_sixtyfour_check;
			unsigned char fail_to_auth = 0;
			unsigned gcn;
			unsigned char slotnum;
			unsigned char isgm;

			gcn = *(unsigned*)&ship->decryptbuf[0x06];
#ifdef NO_SQL
			for (ds = 0;ds < num_security;ds++)
			{
				if (security_data[ds]->guildcard == gcn)
				{
					int found_match;

					security_thirtytwo_check = security_data[ds]->thirtytwo;
					security_sixtyfour_check = security_data[ds]->sixtyfour;
					slotnum = security_data[ds]->slotnum;
					isgm = security_data[ds]->isgm;

					found_match = 0;

					security_client_sixtyfour = *(long long*)&ship->decryptbuf[0x0E];
					if (security_client_sixtyfour == security_sixtyfour_check)
						found_match = 1;

					security_client_sixtyfour = *(long long*)&ship->decryptbuf[0x16];
					if (security_client_sixtyfour == security_sixtyfour_check)
						found_match = 1;

					security_client_sixtyfour = *(long long*)&ship->decryptbuf[0x1E];
					if (security_client_sixtyfour == security_sixtyfour_check)
						found_match = 1;

					security_client_sixtyfour = *(long long*)&ship->decryptbuf[0x26];
					if (security_client_sixtyfour == security_sixtyfour_check)
						found_match = 1;

					security_client_sixtyfour = *(long long*)&ship->decryptbuf[0x2E];
					if (security_client_sixtyfour == security_sixtyfour_check)
						found_match = 1;

					if (found_match == 0)
						fail_to_auth = 1;

					security_client_thirtytwo = *(unsigned*)&ship->decryptbuf[0x0A];

					if (security_client_thirtytwo != security_thirtytwo_check)
						fail_to_auth = 1;

					break;
				}
			}
#else
			sprintf_s(myQuery, _countof(myQuery), "SELECT * from security_data WHERE guildcard='%u'", gcn);
			// Nom nom nom
			if (!mysql_query(myData, &myQuery[0]))
			{
				int num_rows, found_match;

				found_match = 0;

				myResult = mysql_store_result(myData);
				num_rows = (int)mysql_num_rows(myResult);

				if (num_rows)
				{
					myRow = mysql_fetch_row(myResult);

					security_thirtytwo_check = atoi(myRow[1]);
					memcpy(&security_sixtyfour_check, myRow[2], 8);
					slotnum = atoi(myRow[3]);
					isgm = atoi(myRow[4]);

					security_client_sixtyfour = *(long long*)&ship->decryptbuf[0x0E];
					if (security_client_sixtyfour == security_sixtyfour_check)
						found_match = 1;

					security_client_sixtyfour = *(long long*)&ship->decryptbuf[0x16];
					if (security_client_sixtyfour == security_sixtyfour_check)
						found_match = 1;

					security_client_sixtyfour = *(long long*)&ship->decryptbuf[0x1E];
					if (security_client_sixtyfour == security_sixtyfour_check)
						found_match = 1;

					security_client_sixtyfour = *(long long*)&ship->decryptbuf[0x26];
					if (security_client_sixtyfour == security_sixtyfour_check)
						found_match = 1;

					security_client_sixtyfour = *(long long*)&ship->decryptbuf[0x2E];
					if (security_client_sixtyfour == security_sixtyfour_check)
						found_match = 1;

					if (found_match == 0)
						fail_to_auth = 1;
				}
				else
					fail_to_auth = 1;

				security_client_thirtytwo = *(unsigned*)&ship->decryptbuf[0x0A];

				if (security_client_thirtytwo != security_thirtytwo_check)
					fail_to_auth = 1;

				mysql_free_result(myResult);
			}
			else
				fail_to_auth = 1;
#endif
			ship->encryptbuf[0x00] = 0x0B;
			ship->encryptbuf[0x01] = fail_to_auth;
			*(unsigned*)&ship->encryptbuf[0x02] = gcn;
			ship->encryptbuf[0x06] = slotnum;
			ship->encryptbuf[0x07] = isgm;
			*(unsigned*)&ship->encryptbuf[0x08] = security_thirtytwo_check;
			*(long long*)&ship->encryptbuf[0x0C] = security_sixtyfour_check;
			compressShipPacket(ship, &ship->encryptbuf[0x00], 0x14);
		}
		break;
		}
		break;
		//printf("探测 ShipProcessPacket 0x0B 指令的用途\n");
	case 0x0C:
		printf("未知 ShipProcessPacket 0x0C 指令\n");
		break;
	case 0x0D:// Various commands related to ship transfers... 各种与船舶转移有关的命令。。。 
		ShipSend0D(ship->decryptbuf[0x05], ship);
		break;
	case 0x0E:// Update player count. 更新玩家统计
	{
		unsigned updateID;

		updateID = *(unsigned*)&ship->decryptbuf[0x06];
		updateID--;

		if (updateID < serverMaxShips)
			ships[updateID]->playerCount = *(unsigned*)&ship->decryptbuf[0x0A];

		construct0xA0();
	}
	break;
	case 0x0F: //应该是 EP1 EP2 EP4
		switch (ship->decryptbuf[0x05])
		{
		case 0x00:
			keys_in_use[ship->key_index] = 1;
			ShipSend0F(0x01, 0x00, ship);
			break;
		case 0x01:
			if (ship->decryptbuf[0x06] == 0)
				ShipSend0F(0x01, 0x01, ship);
			else
				ShipSend0F(0x02, 0x00, ship);
			break;
		case 0x02:
			if (ship->decryptbuf[0x06] == 0)
				ShipSend0F(0x02, 0x01, ship);
			else
				ShipSend0F(0x03, 0x00, ship);
			break;
		case 0x03:
			if (ship->decryptbuf[0x06] == 0)
				ShipSend0F(0x03, 0x01, ship);
			else
				ShipSend10(ship);
			break;
		default:
			break;
		}
		break;
	case 0x11: //与舰船ping代码
		ship->last_ping = (unsigned)servertime;
		ship->sent_ping = 0;
		//printf("与舰船ping的代码\n");
		break;
	case 0x12: //全球公告
			   // Global announcement
	{
		ORANGE* tship;
		unsigned size, ch;

		size = *(unsigned*)&ship->decryptbuf[0x00];
		size -= 4;

		// Just pass the packet along...
		for (ch = 0;ch < serverNumShips;ch++)
		{
			shipNum = serverShipList[ch];
			tship = ships[shipNum];
			if ((tship->shipSockfd >= 0) && (tship->authed == 1))
				compressShipPacket(tship, &ship->decryptbuf[0x04], size);
		}
	}
	break;
	case 0x13: //未知 尝试用在恢复角色的挑战模式数据上
	{
		printf("探测0x13用途在哪");
	}
	break;
	default:
		// Unknown packet received from ship?
		// 从舰船服务器接收到的未知数据包？
		printf("从舰船服务器接收到的未知数据包 会造成与舰船断开连接\n");
		ship->todc = 1;
		break;
	}
}

//角色数据包传输代码
void CharacterProcessPacket(BANANA* client)
{
	char username[17];
	char password[34];
	char hwinfo[18];
	unsigned short clientver;
	char md5password[34] = { 0 };
	unsigned char MDBuffer[17] = { 0 };
	unsigned gcn;
	unsigned ch;
	unsigned selected;
	unsigned shipNum;
	int security_client_thirtytwo, security_thirtytwo_check;
	long long security_client_sixtyfour, security_sixtyfour_check;
#ifdef NO_SQL
	long long truehwinfo;
#endif

	switch (client->decryptbuf[0x02])
	{
	case 0x05:
		printf("用户进入跃迁轨道.\n");
		client->todc = 1;
		//printf("探测 CharacterProcessPacket 0x05 指令 的用途\n");
		break;
	case 0x10:
		if ((client->guildcard) && (client->slotnum != -1))
		{
			ORANGE* tship;

			selected = *(unsigned*)&client->decryptbuf[0x0C];
			for (ch = 0;ch < serverNumShips;ch++)
			{
				shipNum = serverShipList[ch];
				tship = ships[shipNum];

				if ((tship->shipSockfd >= 0) && (tship->authed == 1) &&
					(tship->shipID == selected))
				{
					Send19(tship->shipAddr[0], tship->shipAddr[1],
						tship->shipAddr[2], tship->shipAddr[3],
						tship->shipPort, client);
					break;
				}
			}
		}
		//printf("探测 CharacterProcessPacket 0x10 指令 的用途\n");
		break;
	case 0x1D:
		// Do nothing.啥也不做
		printf("探测 CharacterProcessPacket 0x1D 指令 的用途\n");
		break;
	case 0x93: //客户端相应数据包 用于用户认证账户信息
		if (!client->sendCheck[RECEIVE_PACKET_93]) //如果未接到93指令
		{
			int fail_to_auth = 0;

			clientver = *(unsigned short*)&client->decryptbuf[0x10];
			memcpy(&username[0], &client->decryptbuf[0x1C], 17);
			memcpy(&password[0], &client->decryptbuf[0x4C], 17);
			memset(&hwinfo[0], 0, 18);
#ifdef NO_SQL
			* (long long*)&client->hwinfo[0] = *(long long*)&client->decryptbuf[0x84];
			truehwinfo = *(long long*)&client->decryptbuf[0x84];
			fail_to_auth = 2; // default fail with wrong username
			for (ds = 0;ds < num_accounts;ds++)
			{
				if (!strcmp(&account_data[ds]->username[0], &username[0]))
				{
					fail_to_auth = 0;
					sprintf(&password[strlen(password)], "_%u_salt", account_data[ds]->regtime);
					MDString(&password[0], &MDBuffer[0]);
					for (ch = 0;ch < 16;ch++)
						sprintf(&md5password[ch * 2], "%02x", (unsigned char)MDBuffer[ch]);
					md5password[32] = 0;
					if (!strcmp(&md5password[0], &account_data[ds]->password[0]))
					{
						if (account_data[ds]->isbanned)
							fail_to_auth = 3;
						if (!account_data[ds]->isactive)
							fail_to_auth = 5;
						if (!fail_to_auth)
							gcn = account_data[ds]->guildcard;
						if (client->decryptbuf[0x10] != PSO_CLIENT_VER)
							fail_to_auth = 7;
						client->isgm = account_data[ds]->isgm;
					}
					else
						fail_to_auth = 2;
					break;
				}
			}

			// DO HW BAN LATER

			if (!fail_to_auth)
			{
				for (ds = 0;ds < num_security;ds++)
				{
					if (security_data[ds]->guildcard == gcn)
					{
						int found_match;

						client->dress_flag = 0;
						for (ch = 0;ch < MAX_DRESS_FLAGS;ch++)
						{
							if (dress_flags[ch].guildcard == gcn)
								client->dress_flag = 1;
						}

						security_thirtytwo_check = security_data[ds]->thirtytwo;
						security_sixtyfour_check = security_data[ds]->sixtyfour;
						client->slotnum = security_data[ds]->slotnum;

						found_match = 0;

						security_client_sixtyfour = *(long long*)&client->decryptbuf[0x8C];
						if (security_client_sixtyfour == security_sixtyfour_check)
							found_match = 1;

						security_client_sixtyfour = *(long long*)&client->decryptbuf[0x94];
						if (security_client_sixtyfour == security_sixtyfour_check)
							found_match = 1;

						security_client_sixtyfour = *(long long*)&client->decryptbuf[0x9C];
						if (security_client_sixtyfour == security_sixtyfour_check)
							found_match = 1;

						security_client_sixtyfour = *(long long*)&client->decryptbuf[0xA4];
						if (security_client_sixtyfour == security_sixtyfour_check)
							found_match = 1;

						security_client_sixtyfour = *(long long*)&client->decryptbuf[0xAC];
						if (security_client_sixtyfour == security_sixtyfour_check)
							found_match = 1;

						if (found_match == 0)
							fail_to_auth = 6;

						security_client_thirtytwo = *(unsigned*)&client->decryptbuf[0x18];

						if (security_client_thirtytwo == 0)
							client->sendingchars = 1;
						else
						{
							client->sendingchars = 0;
							if (security_client_thirtytwo != security_thirtytwo_check)
								fail_to_auth = 6;
						}
						break;
					}
				}
			}

#else
			mysql_real_escape_string(myData, &hwinfo[0], &client->decryptbuf[0x84], 8);
			memcpy(&client->hwinfo[0], &hwinfo[0], 18);
			sprintf_s(myQuery, _countof(myQuery), "SELECT * from account_data WHERE username='%s'", username);

			// Check to see if that account already exists. //查看账户是否已存在了
			if (!mysql_query(myData, &myQuery[0]))
			{
				int num_rows, max_fields;

				myResult = mysql_store_result(myData);
				num_rows = (int)mysql_num_rows(myResult);

				if (num_rows)
				{
					myRow = mysql_fetch_row(myResult);
					max_fields = mysql_num_fields(myResult);
					sprintf_s(&password[strlen(password)], _countof(password) - strlen(password), "_%s_salt", myRow[3]);
					MDString(&password[0], &MDBuffer[0]);
					for (ch = 0;ch < 16;ch++)
						sprintf_s(&md5password[ch * 2], _countof(md5password) - ch * 2, "%02x", (unsigned char)MDBuffer[ch]);
					md5password[32] = 0;
					if (!strcmp(&md5password[0], myRow[1]))
					{
						if (!strcmp("1", myRow[8]))
							fail_to_auth = 3;
						if (!strcmp("1", myRow[9]))
							fail_to_auth = 4;
						if (!strcmp("0", myRow[10]))
							fail_to_auth = 5;
						if (!fail_to_auth)
							gcn = atoi(myRow[6]);
						if (client->decryptbuf[0x10] != PSO_CLIENT_VER)
							fail_to_auth = 7;
						client->isgm = atoi(myRow[7]);
					}
					else
						fail_to_auth = 2;
				}
				else
					fail_to_auth = 2;

				mysql_free_result(myResult);
			}
			else
				fail_to_auth = 1; // MySQL error.

								  // Hardware info ban check...

			sprintf_s(myQuery, _countof(myQuery), "SELECT * from hw_bans WHERE hwinfo='%s'", hwinfo);
			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);
				if ((int)mysql_num_rows(myResult))
					fail_to_auth = 3;
				mysql_free_result(myResult);
			}
			else
				fail_to_auth = 1;

			if (fail_to_auth == 0)
			{
				sprintf_s(myQuery, _countof(myQuery), "SELECT * from security_data WHERE guildcard='%u'", gcn);
				// Nom nom nom
				if (!mysql_query(myData, &myQuery[0]))
				{
					int num_rows, found_match;

					found_match = 0;

					myResult = mysql_store_result(myData);
					num_rows = (int)mysql_num_rows(myResult);

					if (num_rows)
					{
						myRow = mysql_fetch_row(myResult);

						client->dress_flag = 0;
						for (ch = 0;ch < MAX_DRESS_FLAGS;ch++)
						{
							if (dress_flags[ch].guildcard == gcn)
								client->dress_flag = 1;
						}

						security_thirtytwo_check = atoi(myRow[1]);
						memcpy(&security_sixtyfour_check, myRow[2], 8);
						client->slotnum = atoi(myRow[3]);

						security_client_sixtyfour = *(long long*)&client->decryptbuf[0x8C];
						if (security_client_sixtyfour == security_sixtyfour_check)
							found_match = 1;

						security_client_sixtyfour = *(long long*)&client->decryptbuf[0x94];
						if (security_client_sixtyfour == security_sixtyfour_check)
							found_match = 1;

						security_client_sixtyfour = *(long long*)&client->decryptbuf[0x9C];
						if (security_client_sixtyfour == security_sixtyfour_check)
							found_match = 1;

						security_client_sixtyfour = *(long long*)&client->decryptbuf[0xA4];
						if (security_client_sixtyfour == security_sixtyfour_check)
							found_match = 1;

						security_client_sixtyfour = *(long long*)&client->decryptbuf[0xAC];
						if (security_client_sixtyfour == security_sixtyfour_check)
							found_match = 1;

						if (found_match == 0)
						{
							debug("未能找到 64-bit 信息.");
							fail_to_auth = 6;
						}
					}
					else
						fail_to_auth = 6;

					security_client_thirtytwo = *(unsigned*)&client->decryptbuf[0x18];

					if (security_client_thirtytwo == 0)
						client->sendingchars = 1;
					else
					{
						client->sendingchars = 0;
						if (security_client_thirtytwo != security_thirtytwo_check)
						{
							fail_to_auth = 6;
							debug("未能找到 32-bit 信息.");
							debug("正在寻找 %i, 拥有 %i", security_thirtytwo_check, security_client_thirtytwo);
						}
					}
					mysql_free_result(myResult);
				}
			}
#endif

			switch (fail_to_auth)
			{
			case 0x00:
				// OK
				memcpy(&client->encryptbuf[0], &PacketE6[0], sizeof(PacketE6));
				*(unsigned*)&client->encryptbuf[0x10] = gcn;
				client->guildcard = gcn;
				_itoa_s(gcn, client->guildcard_string, _countof(client->guildcard_string), 10); /* auth'd, bitch */
																								// Store some security shit in the E6 packet.
				*(long long*)&client->encryptbuf[0x38] = security_sixtyfour_check;
				if (security_thirtytwo_check == 0)
				{
					for (ch = 0;ch < 4;ch++)
						MDBuffer[ch] = (unsigned char)(rand() % 256);
					security_thirtytwo_check = *(unsigned*)&MDBuffer[0];
#ifdef NO_SQL
					for (ds = 0;ds < num_security;ds++)
					{
						if (security_data[ds]->guildcard == gcn)
						{
							security_data[ds]->thirtytwo = security_thirtytwo_check;
							UpdateDataFile("security.dat", ds, security_data[ds], sizeof(L_SECURITY_DATA), 0);
							break;
						}
					}
#else
					sprintf_s(myQuery, _countof(myQuery), "UPDATE security_data set thirtytwo = '%i' WHERE guildcard = '%u'", security_thirtytwo_check, gcn);
					// Nom, nom, nom.
					if (mysql_query(myData, &myQuery[0]))
					{
						Send1A("Couldn't update security information in MySQL database.\nPlease contact the server administrator.", client);
						client->todc = 1;
						return;
					}
#endif
				}
				*(unsigned*)&client->encryptbuf[0x14] = security_thirtytwo_check;
				cipher_ptr = &client->server_cipher;
				encryptcopy(client, &client->encryptbuf[0], sizeof(PacketE6));
				if (client->slotnum != -1)
				{
					if (client->decryptbuf[0x16] != 0x04)
					{
						Send1A("Client/Server synchronization error.", client);
						client->todc = 1;
					}
					else
					{
						// User has completed the login process, after updating the SQL info with their
						// access information, give 'em the ship select screen.
#ifdef NO_SQL
						for (ds = 0;ds < num_accounts;ds++)
						{
							if (account_data[ds]->guildcard == gcn)
							{
								memcpy(&account_data[ds]->lastip[0], &client->IP_Address[0], 16);
								account_data[ds]->lasthwinfo = truehwinfo;
								UpdateDataFile("account.dat", ds, account_data[ds], sizeof(L_ACCOUNT_DATA), 0);
								break;
							}
						}
#else
						sprintf_s(myQuery, _countof(myQuery), "UPDATE account_data set lastip = '%s', lasthwinfo = '%s' WHERE username = '%s'", client->IP_Address, hwinfo, username);
						mysql_query(myData, &myQuery[0]);
#endif
						client->lastTick = (unsigned)servertime;
						SendB1(client);
						SendA0(client);
						SendEE(&Welcome_Message[0], client);
					}
				}
				break;
			case 0x01:
				// MySQL error.
				Send1A("There is a problem with the MySQL database.\n\nPlease contact the server administrator.", client);
				break;
			case 0x02:
				// Username or password incorrect.
				Send1A("Username or password is incorrect.", client);
				break;
			case 0x03:
				// Account is banned.
				Send1A("You are banned from this server.", client);
				break;
			case 0x04:
				// Already logged on.
				Send1A("This account is already logged on.\n\nPlease wait 120 seconds and try again.", client);
				break;
			case 0x05:
				// Account has not completed e-mail validation.
				Send1A("Please complete the registration of this account through\ne-mail validation.\n\nThank you.", client);
				break;
			case 0x06:
				// Security violation.
				Send1A("Security violation.", client);
				break;
			case 0x07:
				// Client version too old.
				Send1A("Your client executable is too old.\nPlease update your client through the patch server.", client);
				break;
			default:
				Send1A("Unknown error.", client);
				break;
			}
			client->sendCheck[RECEIVE_PACKET_93] = 0x01;
		}
		//printf("探测 CharacterProcessPacket 0x93 指令 的用途\n");
		break;
	case 0xDC:
		switch (client->decryptbuf[0x03])
		{
		case 0x03:
			// Send another chunk of guild card data.发送另一区块的公会卡数据
			if ((client->decryptbuf[0x08] == 0x01) && (client->decryptbuf[0x10] == 0x01))
				SendDC(0x00, client->decryptbuf[0x0C], client);
			break;
		default:
			break;
		}
		//printf("探测 CharacterProcessPacket 0xDC 指令 的用途\n");
		break;
	case 0xE0://跳过补丁界面 进入选择角色前触发
			  // The gamepad, keyboard config, and other options....游戏板和其他选项。。。。
			  //printf("探测 CharacterProcessPacket 0xE0 指令 的用途\n");
		SendE2(client);
		break;
	case 0xE3:
		// Client selecting or requesting character.客户端选择或请求角色数据
		SendE4_E5(client->decryptbuf[0x08], client->decryptbuf[0x0C], client);
		//printf("探测 CharacterProcessPacket 0xE3 指令 的用途\n");
		break;
	case 0xE5:
		// Create a character in slot.在角色槽位中创建角色
		// Check invalid data and store character in MySQL store.检查在数据库中无效的角色数据并进行恢复
		AckCharacter_Creation(client->decryptbuf[0x08], client);
		//printf("探测 CharacterProcessPacket 0xE5 指令 的用途\n");
		break;
	case 0xE8:
		switch (client->decryptbuf[0x03])
		{
		case 0x01:
			// Client just communicating the expected guild card checksum.客户端只需传递预期的公会卡校验和
			//(Ignoring for now.) 目前暂时忽略
			SendE8(client);
			break;
		case 0x03:
			// Client requesting guild card checksum.
			// 客户端请求预设的公会卡校验和
			SendDC(0x01, 0, client);
			break;
		default:
			break;
		}
		//printf("探测 CharacterProcessPacket 0xE8 指令 的用途\n");
		break;
	case 0xEB:
		switch (client->decryptbuf[0x03])
		{
		case 0x03:
			// Send another chunk of the parameter files.
			SendEB(0x02, client->decryptbuf[0x04], client);
			break;
		case 0x04:
			// Send header for parameter files.
			SendEB(0x01, 0x00, client);
			break;
		}
		//printf("探测 CharacterProcessPacket 0xEB 指令 的用途\n");
		break;
	case 0xEC:
		if (client->decryptbuf[0x08] == 0x02)
		{
			// Set the dressing room flag (Don't overwrite the character...)
			// 设置更衣室的标记 不重写角色
			for (ch = 0;ch < MAX_DRESS_FLAGS;ch++)
				if (dress_flags[ch].guildcard == 0)
				{
					dress_flags[ch].guildcard = client->guildcard;
					dress_flags[ch].flagtime = (unsigned)servertime;
					break;
					if (ch == (MAX_DRESS_FLAGS - 1))
					{
						Send1A("Unable to save dress flag.", client);
						client->todc = 1;
					}
				}
			client->dress_flag = 1;
		}
		//printf("探测 CharacterProcessPacket 0xEC 指令 的用途\n");
		break;
	default:
		break;
	}
}

//登陆服务器数据包输出
void LoginProcessPacket(BANANA* client)
{
	char username[17];
	char password[34];
	long long security_sixtyfour_check;
	char hwinfo[18];
	unsigned short clientver;
	char md5password[34] = { 0 };
	unsigned char MDBuffer[17] = { 0 };
	unsigned gcn;
	unsigned ch, connectNum, shipNum;
#ifdef NO_SQL
	long long truehwinfo;
#endif
	ORANGE* tship;
#ifndef NO_SQL
	char security_sixtyfour_binary[18];
#endif


	/* Only packet we're expecting during the login is 0x93 and 0x05.  在登录过程中，我们只需要0x93和0x05。 */

	switch (client->decryptbuf[0x02])
	{
	case 0x05:
		printf("用户已离开跃迁.\n");
		client->todc = 1;
		break;
	case 0x93:
		if (!client->sendCheck[RECEIVE_PACKET_93])
		{
			int fail_to_auth = 0;
			clientver = *(unsigned short*)&client->decryptbuf[0x10];
			memcpy(&username[0], &client->decryptbuf[0x1C], 17);
			memcpy(&password[0], &client->decryptbuf[0x4C], 17);
			memset(&hwinfo[0], 0, 18);
#ifdef NO_SQL
			* (long long*)&client->hwinfo[0] = *(long long*)&client->decryptbuf[0x84];
			truehwinfo = *(long long*)&client->decryptbuf[0x84];
			fail_to_auth = 2; // default fail with wrong username
			for (ds = 0;ds < num_accounts;ds++)
			{
				if (!strcmp(&account_data[ds]->username[0], &username[0]))
				{
					fail_to_auth = 0;
					sprintf(&password[strlen(password)], "_%u_salt", account_data[ds]->regtime);
					MDString(&password[0], &MDBuffer[0]);
					for (ch = 0;ch < 16;ch++)
						sprintf(&md5password[ch * 2], "%02x", (unsigned char)MDBuffer[ch]);
					md5password[32] = 0;
					if (!strcmp(&md5password[0], &account_data[ds]->password[0]))
					{
						if (account_data[ds]->isbanned)
							fail_to_auth = 3;
						if (!account_data[ds]->isactive)
							fail_to_auth = 5;
						if (!fail_to_auth)
							gcn = account_data[ds]->guildcard;
						if ((strcmp(&client->decryptbuf[0x8C], PSO_CLIENT_VER_STRING) != 0) || (client->decryptbuf[0x10] != PSO_CLIENT_VER))
							fail_to_auth = 7;
						client->isgm = account_data[ds]->isgm;
					}
					else
						fail_to_auth = 2;
					break;
				}
			}

			// DO HW BAN LATER

#else
			mysql_real_escape_string(myData, &hwinfo[0], &client->decryptbuf[0x84], 8);
			memcpy(&client->hwinfo[0], &hwinfo[0], 18);

			sprintf_s(myQuery, _countof(myQuery), "SELECT * from account_data WHERE username='%s'", username);

			// Check to see if that account already exists.
			if (!mysql_query(myData, &myQuery[0]))
			{
				int num_rows, max_fields;

				myResult = mysql_store_result(myData);
				num_rows = (int)mysql_num_rows(myResult);

				if (num_rows)
				{
					myRow = mysql_fetch_row(myResult);
					max_fields = mysql_num_fields(myResult);
					sprintf_s(&password[strlen(password)], _countof(password) - strlen(password), "_%s_salt", myRow[3]);
					MDString(&password[0], &MDBuffer[0]);
					for (ch = 0;ch < 16;ch++)
						sprintf_s(&md5password[ch * 2], _countof(md5password) - ch * 2, "%02x", (unsigned char)MDBuffer[ch]);
					md5password[32] = 0;
					if (!strcmp(&md5password[0], myRow[1]))
					{
						if (!strcmp("1", myRow[8]))
							fail_to_auth = 3;
						if (!strcmp("1", myRow[9]))
							fail_to_auth = 4;
						if (!strcmp("0", myRow[10]))
							fail_to_auth = 5;
						if (!fail_to_auth)
							gcn = atoi(myRow[6]);
						if ((strcmp(&client->decryptbuf[0x8C], PSO_CLIENT_VER_STRING) != 0) || (client->decryptbuf[0x10] != PSO_CLIENT_VER))
							fail_to_auth = 7;
						client->isgm = atoi(myRow[7]);
					}
					else
						fail_to_auth = 2;
				}
				else
					fail_to_auth = 2;
				mysql_free_result(myResult);
			}
			else
				fail_to_auth = 1; // MySQL error.

								  // Hardware info ban check...

			sprintf_s(myQuery, _countof(myQuery), "SELECT * from hw_bans WHERE hwinfo='%s'", hwinfo);
			if (!mysql_query(myData, &myQuery[0]))
			{
				myResult = mysql_store_result(myData);
				if ((int)mysql_num_rows(myResult))
					fail_to_auth = 3;
				mysql_free_result(myResult);
			}
			else
				fail_to_auth = 1;
#endif

			switch (fail_to_auth)
			{
			case 0x00:
				// OK

				// If guild card is connected to the login server already, disconnect it.

				for (ch = 0;ch < serverNumConnections;ch++)
				{
					connectNum = serverConnectionList[ch];
					if (connections[connectNum]->guildcard == gcn)
					{
						Send1A("This account has just logged on.\n\nYou are now being disconnected.", connections[connectNum]);
						connections[connectNum]->todc = 1;
						break;
					}
				}

				// If guild card is connected to ships, disconnect it.

				for (ch = 0;ch < serverNumShips;ch++)
				{
					shipNum = serverShipList[ch];
					tship = ships[shipNum];
					if ((tship->shipSockfd >= 0) && (tship->authed == 1))
						ShipSend08(gcn, tship);
				}


				memcpy(&client->encryptbuf[0], &PacketE6[0], sizeof(PacketE6));
				*(unsigned*)&client->encryptbuf[0x10] = gcn;

				// Store some security shit
				for (ch = 0;ch < 8;ch++)
					client->encryptbuf[0x38 + ch] = (unsigned char)rand() % 255;

				security_sixtyfour_check = *(long long*)&client->encryptbuf[0x38];

				// Nom, nom, nom.

#ifdef NO_SQL
				free_record = -1;
				new_record = 1;
				for (ds = 0;ds < num_security;ds++)
				{
					if (security_data[ds]->guildcard == gcn)
						security_data[ds]->guildcard = 0;
					UpdateDataFile("security.dat", ds, security_data[ds], sizeof(L_SECURITY_DATA), 0);
					if (security_data[ds]->guildcard == 0)
					{
						free_record = ds;
						new_record = 0;
					}
				}
				if (new_record)
				{
					free_record = num_security++;
					security_data[free_record] = malloc(sizeof(L_SECURITY_DATA));
				}
				security_data[free_record]->guildcard = gcn;
				security_data[free_record]->thirtytwo = 0;
				security_data[free_record]->sixtyfour = security_sixtyfour_check;
				security_data[free_record]->isgm = client->isgm;
				security_data[free_record]->slotnum = -1;
				UpdateDataFile("security.dat", free_record, security_data[free_record], sizeof(L_SECURITY_DATA), new_record);
#else
				sprintf_s(myQuery, _countof(myQuery), "DELETE from security_data WHERE guildcard = '%u'", gcn);
				mysql_query(myData, &myQuery[0]);
				mysql_real_escape_string(myData, &security_sixtyfour_binary[0], (char*)&security_sixtyfour_check, 8);
				sprintf_s(myQuery, _countof(myQuery), "INSERT INTO security_data (guildcard, thirtytwo, sixtyfour, isgm) VALUES ('%u','0','%s', '%u')", gcn, (char*)&security_sixtyfour_binary, client->isgm);
				if (mysql_query(myData, &myQuery[0]))
				{
					Send1A("Couldn't update security information in MySQL database.\nPlease contact the server administrator.", client);
					client->todc = 1;
					return;
				}
#endif

				cipher_ptr = &client->server_cipher;
				encryptcopy(client, &client->encryptbuf[0], sizeof(PacketE6));

				Send19(serverIP[0], serverIP[1], serverIP[2], serverIP[3], serverPort + 1, client);
				for (ch = 0;ch < MAX_DRESS_FLAGS;ch++)
				{
					if ((dress_flags[ch].guildcard == gcn) || ((unsigned)servertime - dress_flags[ch].flagtime > DRESS_FLAG_EXPIRY))
						dress_flags[ch].guildcard = 0;
				}
				break;
			case 0x01:
				// MySQL error.
				Send1A("There is a problem with the MySQL database.\n\nPlease contact the server administrator.", client);
				break;
			case 0x02:
				// Username or password incorrect.
				Send1A("Username or password is incorrect.", client);
				break;
			case 0x03:
				// Account is banned.
				Send1A("You are banned from this server.", client);
				break;
			case 0x04:
				// Already logged on.
				Send1A("This account is already logged on.\n\nPlease wait 120 seconds and try again.", client);
				break;
			case 0x05:
				// Account has not completed e-mail validation.
				Send1A("Please complete the registration of this account through\ne-mail validation.\n\nThank you.", client);
				break;
			case 0x07:
				// Client version too old.
				Send1A("Your client executable is too old.\nPlease update your client through the patch server.", client);
				break;
			default:
				Send1A("Unknown error.", client);
				break;
			}
			client->sendCheck[RECEIVE_PACKET_93] = 0x01;
		}
		break;
	default:
		client->todc = 1;
		break;
	}
}

#include "Load_files.h"

#ifdef NO_SQL

void UpdateDataFile(const char* filename, unsigned count, void* data, unsigned record_size, int new_record)
{
	FILE* fp;
	unsigned fs;

	fopen_s(&fp, filename, "r+b");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		fs = ftell(fp);
		if ((count * record_size) <= fs)
		{
			fseek(fp, count * record_size, SEEK_SET);
			fwrite(data, 1, record_size, fp);
		}
		else
			debug("无法在 %s 中查找要更新的记录", filename);
		fclose(fp);
	}
	else
	{
		fopen_s(&fp, filename, "wb");
		if (fp)
		{
			fwrite(data, 1, record_size, fp); // Has to be the first record...
			fclose(fp);
		}
		else
			debug("无法打开 %s 进行写入!\n", filename);
	}
}


void DumpDataFile(const char* filename, unsigned* count, void** data, unsigned record_size)
{
	FILE* fp;
	unsigned ch;

	printf("转储 \"%s\" ... ", filename);
	fopen_s(&fp, filename, "wb");
	if (fp)
	{
		for (ch = 0;ch < *count;ch++)
			fwrite(data[ch], 1, record_size, fp);
		fclose(fp);
	}
	else
		debug("无法打开 %s 进行写入!\n", filename);
	printf("完成!\n");
}

void LoadDataFile(const char* filename, unsigned* count, void** data, unsigned record_size)
{
	FILE* fp;
	unsigned ch;

	printf("正在载入 \"%s\" ... ", filename);
	fopen_s(&fp, filename, "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		*count = ftell(fp) / record_size;
		fseek(fp, 0, SEEK_SET);
		for (ch = 0;ch < *count;ch++)
		{
			data[ch] = malloc(record_size);
			if (!data[ch])
			{
				printf("内存不足!\nHit [ENTER]");
				gets_s(&dp[0], sizeof(dp));
				exit(1);
			}
			fread(data[ch], 1, record_size, fp);
		}
		fclose(fp);
	}
	printf("完成!\n");
}

#endif

//参数回调专用
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == MYWM_NOTIFYICON)
	{
		switch (lParam)
		{
		case WM_LBUTTONDBLCLK:
			switch (wParam)
			{
			case 100:
				if (program_hidden)
				{
					program_hidden = 0;
					ShowWindow(consoleHwnd, SW_NORMAL);
					SetForegroundWindow(consoleHwnd);
					SetFocus(consoleHwnd);
				}
				else
				{
					program_hidden = 1;
					ShowWindow(consoleHwnd, SW_HIDE);
				}
				return TRUE;
				break;
			}
			break;
		}
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}


/********************************************************
**
**  main  :-
**
********************************************************/

int main(int argc, char* argv[])
{
	unsigned ch, ch2;
	struct in_addr login_in;
	struct in_addr character_in;
	struct in_addr ship_in;
	struct sockaddr_in listen_in;
	unsigned listen_length;
	int login_sockfd = -1, character_sockfd = -1, ship_sockfd = -1;
	int pkt_len, pkt_c, bytes_sent;
	unsigned short this_packet;
	unsigned ship_this_packet;

	FILE* fp;
	//int wserror;
	unsigned char MDBuffer[17] = { 0 };
	unsigned connectNum, shipNum;
	HINSTANCE hinst;
	NOTIFYICONDATA nid = { 0 };
	WNDCLASS wc = { 0 };
	HWND hwndWindow;
	MSG msg;
	WSADATA winsock_data;

	consoleHwnd = GetConsoleWindow();
	hinst = GetModuleHandle(NULL);

	dp[0] = 0;

	memset(dp, 0, sizeof(dp));

	strcat_s(dp, _countof(dp), "Tethealla 登陆服务器 版本 ");
	strcat_s(dp, _countof(dp), SERVER_VERSION);
	strcat_s(dp, _countof(dp), " 作者 Sodaboy 编译 Sancaros");
	SetConsoleTitle(&dp[0]);

	printf("\n特提塞拉 登陆服务器 版本 %s  版权作者 (C) 2008  Terry Chatman Jr.\n", SERVER_VERSION);
	printf("\n编译 Sancaros. 2020.12\n");
	printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	printf("这个程序绝对没有保证: 详情参见说明\n");
	printf("请参阅GPL-3.0.TXT中的第15节\n");
	printf("这是免费软件,欢迎您重新发布\n");
	printf("在某些条件下,详见GPL-3.0.TXT.\n");
	/*
	for (ch=0;ch<5;ch++)
	{
	printf (".");
	Sleep (1000);
	}*/
	printf("\n\n");

	WSAStartup(MAKEWORD(1, 1), &winsock_data);

	srand((unsigned)time(NULL));

	memset(&dress_flags[0], 0, sizeof(DRESSFLAG) * MAX_DRESS_FLAGS);

	printf("正在从 tethealla.ini 文件中载入设置...");
	load_config_file();
	printf("  确认!\n");

	/* Set this up for later. 以后再做这个*/

	memset(&empty_bank, 0, sizeof(BANK));

	for (ch = 0;ch < 200;ch++)
		empty_bank.bankInventory[ch].itemid = 0xFFFFFFFF;

#ifdef NO_SQL
	LoadDataFile("account.dat", &num_accounts, &account_data[0], sizeof(L_ACCOUNT_DATA));
	LoadDataFile("bank.dat", &num_bankdata, &bank_data[0], sizeof(L_BANK_DATA));
	LoadDataFile("character.dat", &num_characters, &character_data[0], sizeof(L_CHARACTER_DATA));
	LoadDataFile("guild.dat", &num_guilds, &guild_data[0], sizeof(L_GUILD_DATA));
	LoadDataFile("hwbans.dat", &num_hwbans, &hw_bans[0], sizeof(L_HW_BANS));
	LoadDataFile("ipbans.dat", &num_ipbans, &ip_bans[0], sizeof(L_IP_BANS));
	LoadDataFile("keydata.dat", &num_keydata, &key_data[0], sizeof(L_KEY_DATA));
	LoadDataFile("security.dat", &num_security, &security_data[0], sizeof(L_SECURITY_DATA));
	LoadDataFile("shipkey.dat", &num_shipkeys, &ship_data[0], sizeof(L_SHIP_DATA));
	LoadDataFile("team.dat", &num_teams, &team_data[0], sizeof(L_TEAM_DATA));
#endif

	printf("正在载入 PlyLevelTbl.bin ...");
	fopen_s(&fp, "PlyLevelTbl.bin", "rb");
	if (!fp)
	{
		printf("文件 plyleveltbl.bin 缺失!\n");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}
	fread(&startingStats[0], 1, 12 * 14, fp);
	fclose(fp);
	printf(" 确认!\n");
	printf("正在载入 0xE2 基本数据包 ...");
	fopen_s(&fp, "e2base.bin", "rb");
	if (!fp)
	{
		printf("文件 e2base.bin 缺失!\n");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}
	fread(&E2_Base[0], 1, 2808, fp);
	fclose(fp);
	printf(" 确认!\n");
	printf("正在载入 0xE7 基本数据包 ...");
	fopen_s(&fp, "e7base.bin", "rb");
	if (!fp)
	{
		printf("文件 e7base.bin 缺失!\n");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}
	fread(&E7_Base, 1, sizeof(CHARDATA), fp);
	fclose(fp);
	printf(" 确认!\n");
	printf("正在加载 e8send.txt 中指定的参数文件...");
	construct0xEB();
	memcpy(&Packet03[0x54], &Message03[0], sizeof(Message03));
	printf("\n正在加载任务奖励物品...\n");
	LoadQuestAllow();
	LoadDropData();
	printf("... 完成!\n");
#ifdef DEBUG_OUTPUT
#ifndef NO_SQL
	printf("\nMySQL 数据库连接参数\n");
	printf("///////////////////////////\n");
	printf("地址: %s\n", mySQL_Host);
	printf("端口: %u\n", mySQL_Port);
	printf("用户: %s\n", mySQL_Username);
	printf("密码: %s\n", mySQL_Password);
	printf("数据库: %s\n", mySQL_Database);
#endif
#endif
	printf("\n登陆服务器参数\n");
	printf("///////////////////////\n");
	if (override_on)
		printf("注意: IP 覆盖功能已打开.\n服务器将绑定到 %u.%u.%u.%u 但将发送至下面列出的IP.\n", overrideIP[0], overrideIP[1], overrideIP[2], overrideIP[3]);
	printf("IP地址: %u.%u.%u.%u\n", serverIP[0], serverIP[1], serverIP[2], serverIP[3]);
	printf("登陆端口: %u\n", serverPort);
	printf("角色端口: %u\n", serverPort + 1);
	printf("最大连接: %u\n", serverMaxConnections);
	printf("最大舰船: %u\n\n", serverMaxShips);
	printf("内存分配 %u 字节用于连接...", sizeof(BANANA) * serverMaxConnections);
	for (ch = 0;ch < serverMaxConnections;ch++)
	{
		connections[ch] = malloc(sizeof(BANANA));
		if (!connections[ch])
		{
			printf("内存不足!\n");
			printf("按下 [回车键] 退出");
			gets_s(&dp[0], sizeof(dp));
			exit(1);
		}
		initialize_connection(connections[ch]);
	}
	printf(" 确认!\n");
	printf("内存分配 %u 字节用于舰船...", sizeof(ORANGE) * serverMaxShips);
	memset(&ships, 0, 4 * serverMaxShips);
	for (ch = 0;ch < serverMaxShips;ch++)
	{
		ships[ch] = malloc(sizeof(ORANGE));
		if (!ships[ch])
		{
			printf("内存不足!\n");
			printf("按下 [回车键] 退出");
			gets_s(&dp[0], sizeof(dp));
			exit(1);
		}
		initialize_ship(ships[ch]);
	}
	printf(" 确认!\n\n");
	printf("构建默认舰船列表数据包 ...");
	construct0xA0();
	printf("  确认!\n\n");

#ifndef NO_SQL
	printf("连接至 MySQL 数据库 ...");

	if ((myData = mysql_init((MYSQL*)0)) &&
		mysql_real_connect(myData, &mySQL_Host[0], &mySQL_Username[0], &mySQL_Password[0], NULL, mySQL_Port,
			NULL, 0))
	{
		if (mysql_select_db(myData, &mySQL_Database[0]) < 0) {
			printf("无法选定 %s 数据库 !\n", mySQL_Database);
			mysql_close(myData);
			return 2;
		}
	}
	else {
		printf("无法连接到mysql服务器 (%s) 端口 %d !\nmysql_error = %s\n",
			mySQL_Host, mySQL_Port, mysql_error(myData));

		mysql_close(myData);
		return 1;
	}

	printf("  确认!\n\n");

	printf("设置会话等待时间超时 wait_timeout ...");

	/* Set MySQL to time out after 7 days of inactivity... lulz :D */

	sprintf(&myQuery[0], "SET SESSION wait_timeout = 604800");
	mysql_query(myData, &myQuery[0]);
	printf("  确认!\n\n");

#endif

	printf("正在获取最大舰船密钥计数... ");

#ifdef NO_SQL

	max_ship_keys = 0;
	for (ds = 0;ds < num_shipkeys;ds++)
	{
		if (ship_data[ds]->idx >= max_ship_keys)
			max_ship_keys = ship_data[ds]->idx;
	}

#else

	sprintf_s(myQuery, _countof(myQuery), "SELECT * from ship_data");

	if (!mysql_query(myData, &myQuery[0]))
	{
		unsigned key_rows;

		myResult = mysql_store_result(myData);
		key_rows = (int)mysql_num_rows(myResult);
		max_ship_keys = 0;
		while (key_rows)
		{
			myRow = mysql_fetch_row(myResult);
			if ((unsigned)atoi(myRow[1]) > max_ship_keys)
				max_ship_keys = atoi(myRow[1]);
			key_rows--;
		}
		printf("(%u) ", max_ship_keys);
		mysql_free_result(myResult);

	}
	else
	{
		printf("无法查询密钥数据库.\n");
	}

#endif

	printf(" 确认!\n");

	printf("正在载入 default.flag ...");
	fopen_s(&fp, "default.flag", "rb");
	if (!fp)
	{
		printf("文件 default.flag 缺失!\n");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}
	fread(&DefaultTeamFlag[0], 1, 2048, fp);
	fclose(fp);
#ifndef NO_SQL
	mysql_real_escape_string(myData, &DefaultTeamFlagSlashes[0], &DefaultTeamFlag[0], 2048);
#endif
	printf(" 确认!\n");

	/* Open the PSO BB Login Server Port... */

	printf("正在打开服务器端口 %u 用于登陆.\n", serverPort);

#ifdef USEADDR_ANY
	login_in.s_addr = INADDR_ANY;
#else
	if (override_on)
		*(unsigned*)&login_in.s_addr = *(unsigned*)&overrideIP[0];
	else
		*(unsigned*)&login_in.s_addr = *(unsigned*)&serverIP[0];
#endif
	login_sockfd = tcp_sock_open(login_in, serverPort);

	tcp_listen(login_sockfd);

	/* Open the PSO BB Character Server Port... */

	printf("正在打开角色数据端口 %u 用于连接.\n", serverPort + 1);

#ifdef USEADDR_ANY
	character_in.s_addr = INADDR_ANY;
#else
	if (override_on)
		*(unsigned*)&character_in.s_addr = *(unsigned*)&overrideIP[0];
	else
		*(unsigned*)&character_in.s_addr = *(unsigned*)&serverIP[0];
#endif
	character_sockfd = tcp_sock_open(character_in, serverPort + 1);

	tcp_listen(character_sockfd);

	/* Open the Ship Port... */

	printf("正在打开舰船端口 3455 用于连接.\n");

#ifdef USEADDR_ANY
	ship_in.s_addr = INADDR_ANY;
#else
	if (override_on)
		*(unsigned*)&ship_in.s_addr = *(unsigned*)&overrideIP[0];
	else
		*(unsigned*)&ship_in.s_addr = *(unsigned*)&serverIP[0];
#endif
	ship_sockfd = tcp_sock_open(ship_in, 3455);

	tcp_listen(ship_sockfd);

	if ((login_sockfd < 0) || (character_sockfd < 0) || (ship_sockfd < 0))
	{
		printf("无法打开连接端口.\n");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}

	printf("\n监听中...\n");

	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hIcon = LoadIcon(hinst, IDI_APPLICATION);
	wc.hCursor = LoadCursor(hinst, IDC_ARROW);
	wc.hInstance = hinst;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = "Sancaros";
	wc.style = CS_HREDRAW | CS_VREDRAW;

	if (!RegisterClass(&wc))
	{
		printf("注册类失败.\n");
		exit(1);
	}

	hwndWindow = CreateWindow("Sancaros", "hidden window", WS_MINIMIZE, 1, 1, 1, 1,
		NULL,
		NULL,
		hinst,
		NULL);

	backupHwnd = hwndWindow;

	if (!hwndWindow)
	{
		printf("无法创建窗口.");
		exit(1);
	}

	ShowWindow(hwndWindow, SW_HIDE);
	UpdateWindow(hwndWindow);
	ShowWindow(consoleHwnd, SW_HIDE);
	UpdateWindow(consoleHwnd);

	nid.cbSize = sizeof(nid);
	nid.hWnd = hwndWindow;
	nid.uID = 100;
	nid.uCallbackMessage = MYWM_NOTIFYICON;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.hIcon = LoadIcon(hinst, MAKEINTRESOURCE(IDI_ICON1));
	nid.szTip[0] = 0;
	strcat(&nid.szTip[0], "Tethealla 登陆服务器 ");
	strcat(&nid.szTip[0], SERVER_VERSION);
	strcat(&nid.szTip[0], " - 双击以显示/隐藏");
	Shell_NotifyIcon(NIM_ADD, &nid);


#ifdef NO_SQL

	lastdump = (unsigned)time(NULL);

#endif

	for (;;)
	{
		int nfds = 0;

		/* Ping pong?! */

		servertime = time(NULL);

		/* Process the system tray icon */

		if (backupHwnd != hwndWindow)
		{
			debug("hwndWindow 已损坏...");
			display_packet((unsigned char*)&hwndWindow, sizeof(HWND));
			hwndWindow = backupHwnd;
			WriteLog("hwndWindow 已损坏 %s", (char*)&dp[0]);
		}

		if (PeekMessage(&msg, hwndWindow, 0, 0, 1))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}


#ifdef NO_SQL

		if ((unsigned)servertime - lastdump > 600)
		{
			printf("Refreshing account and ship key databases...\n");
			for (ch = 0;ch < num_accounts;ch++)
				free(account_data[ch]);
			num_accounts = 0;
			LoadDataFile("account.dat", &num_accounts, &account_data[0], sizeof(L_ACCOUNT_DATA));
			for (ch = 0;ch < num_shipkeys;ch++)
				free(ship_data[ch]);
			num_shipkeys = 0;
			LoadDataFile("shipkey.dat", &num_shipkeys, &ship_data[0], sizeof(L_SHIP_DATA));
			lastdump = (unsigned)servertime;
		}

#endif

		/* Clear socket activity flags. */

		FD_ZERO(&ReadFDs);
		FD_ZERO(&WriteFDs);
		FD_ZERO(&ExceptFDs);

		for (ch = 0;ch < serverNumConnections;ch++)
		{
			connectNum = serverConnectionList[ch];
			workConnect = connections[connectNum];

			if (workConnect->plySockfd >= 0)
			{
				if (workConnect->packetdata)
				{
					this_packet = *(unsigned short*)&workConnect->packet[workConnect->packetread];
					memcpy(&workConnect->decryptbuf[0], &workConnect->packet[workConnect->packetread], this_packet);

					switch (workConnect->login)
					{
					case 0x01:
						// Login server 登陆服务器连接数据库
						LoginProcessPacket(workConnect);
						break;
					case 0x02:
						// Character server 角色服务器连接数据库
						CharacterProcessPacket(workConnect);
						break;
					}
					workConnect->packetread += this_packet;
					if (workConnect->packetread == workConnect->packetdata)
						workConnect->packetread = workConnect->packetdata = 0;
				}

				if (workConnect->lastTick != (unsigned)servertime)
				{
					if (workConnect->lastTick > (unsigned)servertime)
						ch2 = 1;
					else
						ch2 = 1 + ((unsigned)servertime - workConnect->lastTick);
					workConnect->lastTick = (unsigned)servertime;
					workConnect->packetsSec /= ch2;
					workConnect->toBytesSec /= ch2;
					workConnect->fromBytesSec /= ch2;
				}

				/*
					if (((unsigned) servertime - workConnect->connected >= 300) ||
					(workConnect->connected > (unsigned)servertime))
				{
					Send1A("You have been idle for too long.  Disconnecting...", workConnect);
					workConnect->todc = 1;
				}
					*/

				FD_SET(workConnect->plySockfd, &ReadFDs);
				nfds = max(nfds, workConnect->plySockfd);
				FD_SET(workConnect->plySockfd, &ExceptFDs);
				nfds = max(nfds, workConnect->plySockfd);
				if (workConnect->snddata - workConnect->sndwritten)
				{
					FD_SET(workConnect->plySockfd, &WriteFDs);
					nfds = max(nfds, workConnect->plySockfd);
				}
			}
		}

		for (ch = 0;ch < serverNumShips;ch++)
		{
			shipNum = serverShipList[ch];
			workShip = ships[shipNum];

			if (workShip->shipSockfd >= 0)
			{
				// Send a ping request to the ship when 30 seconds passes...
				if (((unsigned)servertime - workShip->last_ping >= 30) && (workShip->sent_ping == 0))
				{
					workShip->sent_ping = 1;
					ShipSend11(workShip);
				}

				// If it's been over a minute since we've heard from a ship, terminate 
				// the connection with it.

				if ((unsigned)servertime - workShip->last_ping > 60)
				{
					printf("%s 舰船数据连接Ping超时.\n", workShip->name);
					initialize_ship(workShip);
				}
				else
				{
					if (workShip->packetdata)
					{
						ship_this_packet = *(unsigned*)&workShip->packet[workShip->packetread];
						memcpy(&workShip->decryptbuf[0], &workShip->packet[workShip->packetread], ship_this_packet);

						ShipProcessPacket(workShip);

						workShip->packetread += ship_this_packet;

						if (workShip->packetread == workShip->packetdata)
							workShip->packetread = workShip->packetdata = 0;
					}


					/* Limit time of authorization to 60 seconds... */

					if ((workShip->authed == 0) && ((unsigned)servertime - workShip->connected >= 60))
						workShip->todc = 1;


					FD_SET(workShip->shipSockfd, &ReadFDs);
					nfds = max(nfds, workShip->shipSockfd);
					if (workShip->snddata - workShip->sndwritten)
					{
						FD_SET(workShip->shipSockfd, &WriteFDs);
						nfds = max(nfds, workShip->shipSockfd);
					}
				}
			}
		}

		FD_SET(login_sockfd, &ReadFDs);
		nfds = max(nfds, login_sockfd);
		FD_SET(character_sockfd, &ReadFDs);
		nfds = max(nfds, character_sockfd);
		FD_SET(ship_sockfd, &ReadFDs);
		nfds = max(nfds, ship_sockfd);

		/* Check sockets for activity. */

		if (select(nfds + 1, &ReadFDs, &WriteFDs, &ExceptFDs, &select_timeout) > 0)
		{
			if (FD_ISSET(login_sockfd, &ReadFDs))
			{
				// Someone's attempting to connect to the login server.
				ch = free_connection();
				if (ch != 0xFFFF)
				{
					listen_length = sizeof(listen_in);
					workConnect = connections[ch];
					if ((workConnect->plySockfd = tcp_accept(login_sockfd, (struct sockaddr*)&listen_in, &listen_length)) >= 0)
					{
						workConnect->connection_index = ch;
						serverConnectionList[serverNumConnections++] = ch;
						memcpy(&workConnect->IP_Address[0], inet_ntoa(listen_in.sin_addr), 16);
						printf("已接受来自 %s:%u 的登录连接\n", workConnect->IP_Address, listen_in.sin_port);
						start_encryption(workConnect);
						/* Doin' login process... */
						workConnect->login = 1;
					}
				}
			}

			if (FD_ISSET(character_sockfd, &ReadFDs))
			{
				// Someone's attempting to connect to the character server.
				ch = free_connection();
				if (ch != 0xFFFF)
				{
					listen_length = sizeof(listen_in);
					workConnect = connections[ch];
					if ((workConnect->plySockfd = tcp_accept(character_sockfd, (struct sockaddr*)&listen_in, &listen_length)) >= 0)
					{
						workConnect->connection_index = ch;
						serverConnectionList[serverNumConnections++] = ch;
						memcpy(&workConnect->IP_Address[0], inet_ntoa(listen_in.sin_addr), 16);
						printf("已接受来自 %s:%u 的角色数据连接\n", inet_ntoa(listen_in.sin_addr), listen_in.sin_port);

						if ((fp = fopen("loginCount.txt", "w")) == NULL)
						{
							printf("loginCount.txt 文件不存在.\n");
						}
						else
						{
							fprintf(fp, "%u", serverNumConnections);
							fclose(fp);
						}
						start_encryption(workConnect);
						/* Doin' character process... */
						workConnect->login = 2;
					}
				}
			}

			if (FD_ISSET(ship_sockfd, &ReadFDs))
			{
				// A ship is attempting to connect to the ship transfer port.
				ch = free_shipconnection();
				if (ch != 0xFFFF)
				{
					listen_length = sizeof(listen_in);
					workShip = ships[ch];
					if ((workShip->shipSockfd = tcp_accept(ship_sockfd, (struct sockaddr*)&listen_in, &listen_length)) >= 0)
					{
						workShip->connection_index = ch;
						serverShipList[serverNumShips++] = ch;
						printf("已接受来自 %s:%u 的舰船连接\n", inet_ntoa(listen_in.sin_addr), listen_in.sin_port);
						*(unsigned*)&workShip->listenedAddr[0] = *(unsigned*)&listen_in.sin_addr;
						workShip->connected = workShip->last_ping = (unsigned)servertime;
						ShipSend00(workShip);
					}
				}
			}

			// Process client connections

			for (ch = 0;ch < serverNumConnections;ch++)
			{
				connectNum = serverConnectionList[ch];
				workConnect = connections[connectNum];

				if (workConnect->plySockfd >= 0)
				{
					if (FD_ISSET(workConnect->plySockfd, &ReadFDs))
					{
						// Read shit.
						if ((pkt_len = recv(workConnect->plySockfd, &tmprcv[0], TCP_BUFFER_SIZE - 1, 0)) <= 0)
						{
							/*
							wserror = WSAGetLastError();
							printf ("Could not read data from client...\n");
							printf ("Socket Error %u.\n", wserror );
							*/
							initialize_connection(workConnect);
						}
						else
						{
							workConnect->fromBytesSec += (unsigned)pkt_len;
							// Work with it.
							for (pkt_c = 0;pkt_c < pkt_len;pkt_c++)
							{
								workConnect->rcvbuf[workConnect->rcvread++] = tmprcv[pkt_c];

								if (workConnect->rcvread == 8)
								{
									/* Decrypt the packet header after receiving 8 bytes. */

									cipher_ptr = &workConnect->client_cipher;

									decryptcopy(&workConnect->peekbuf[0], &workConnect->rcvbuf[0], 8);

									/* Make sure we're expecting a multiple of 8 bytes. */

									workConnect->expect = *(unsigned short*)&workConnect->peekbuf[0];

									if (workConnect->expect % 8)
										workConnect->expect += (8 - (workConnect->expect % 8));

									if (workConnect->expect > TCP_BUFFER_SIZE)
									{
										initialize_connection(workConnect);
										break;
									}
								}

								if ((workConnect->rcvread == workConnect->expect) && (workConnect->expect != 0))
								{
									if (workConnect->packetdata + workConnect->expect > TCP_BUFFER_SIZE)
									{
										initialize_connection(workConnect);
										break;
									}
									else
									{
										/* Decrypt the rest of the data if needed. */

										cipher_ptr = &workConnect->client_cipher;

										*(long long*)&workConnect->packet[workConnect->packetdata] = *(long long*)&workConnect->peekbuf[0];

										if (workConnect->rcvread > 8)
											decryptcopy(&workConnect->packet[workConnect->packetdata + 8], &workConnect->rcvbuf[8], workConnect->expect - 8);

										this_packet = *(unsigned short*)&workConnect->peekbuf[0];
										workConnect->packetdata += this_packet;

										workConnect->packetsSec++;

										if ((workConnect->packetsSec > 40) ||
											(workConnect->fromBytesSec > 15000) ||
											(workConnect->toBytesSec > 500000))
										{
											printf("%u 由于可能的DDOS而断开连接. (p/s: %u, tb/s: %u, fb/s: %u)\n", workConnect->guildcard, workConnect->packetsSec, workConnect->toBytesSec, workConnect->fromBytesSec);
											initialize_connection(workConnect);
											break;
										}

										workConnect->rcvread = 0;
									}
								}
							}
						}
					}

					if (FD_ISSET(workConnect->plySockfd, &WriteFDs))
					{
						// Write shit.

						bytes_sent = send(workConnect->plySockfd, &workConnect->sndbuf[workConnect->sndwritten],
							workConnect->snddata - workConnect->sndwritten, 0);
						if (bytes_sent == SOCKET_ERROR)
						{
							/*
							wserror = WSAGetLastError();
							printf ("Could not send data to client...\n");
							printf ("Socket Error %u.\n", wserror );
							*/
							initialize_connection(workConnect);
						}
						else
						{
							workConnect->sndwritten += bytes_sent;
							workConnect->toBytesSec += (unsigned)bytes_sent;
						}

						if (workConnect->sndwritten == workConnect->snddata)
							workConnect->sndwritten = workConnect->snddata = 0;
					}

					if (workConnect->todc)
					{
						if (workConnect->snddata - workConnect->sndwritten)
							send(workConnect->plySockfd, &workConnect->sndbuf[workConnect->sndwritten],
								workConnect->snddata - workConnect->sndwritten, 0);
						initialize_connection(workConnect);
					}

					if (FD_ISSET(workConnect->plySockfd, &ExceptFDs)) // Exception?
						initialize_connection(workConnect);
				}
			}

			// Process ship connections

			for (ch = 0;ch < serverNumShips;ch++)
			{
				shipNum = serverShipList[ch];
				workShip = ships[shipNum];

				if (workShip->shipSockfd >= 0)
				{
					if (FD_ISSET(workShip->shipSockfd, &ReadFDs))
					{
						// Read shit.
						if ((pkt_len = recv(workShip->shipSockfd, &tmprcv[0], PACKET_BUFFER_SIZE - 1, 0)) <= 0)
						{
							/*
							wserror = WSAGetLastError();
							printf ("Could not read data from client...\n");
							printf ("Socket Error %u.\n", wserror );
							*/
							printf("与 %s 船失去连接...\n", workShip->name);
							initialize_ship(workShip);
						}
						else
						{
							// Work with it.
							for (pkt_c = 0;pkt_c < pkt_len;pkt_c++)
							{
								workShip->rcvbuf[workShip->rcvread++] = tmprcv[pkt_c];

								if (workShip->rcvread == 4)
								{
									/* Read out how much data we're expecting this packet. */
									workShip->expect = *(unsigned*)&workShip->rcvbuf[0];

									if (workShip->expect > TCP_BUFFER_SIZE)
									{
										printf("与 %s 船失去连接...\n", workShip->name);
										initialize_ship(workShip); /* This shouldn't happen, lol. */
									}
								}

								if ((workShip->rcvread == workShip->expect) && (workShip->expect != 0))
								{
									decompressShipPacket(workShip, &workShip->decryptbuf[0], &workShip->rcvbuf[0]);

									workShip->expect = *(unsigned*)&workShip->decryptbuf[0];

									if (workShip->packetdata + workShip->expect < PACKET_BUFFER_SIZE)
									{
										memcpy(&workShip->packet[workShip->packetdata], &workShip->decryptbuf[0], workShip->expect);
										workShip->packetdata += workShip->expect;
									}
									else
									{
										initialize_ship(workShip);
										break;
									}
									workShip->rcvread = 0;
								}
							}
						}
					}

					if (FD_ISSET(workShip->shipSockfd, &WriteFDs))
					{
						// Write shit.

						bytes_sent = send(workShip->shipSockfd, &workShip->sndbuf[workShip->sndwritten],
							workShip->snddata - workShip->sndwritten, 0);
						if (bytes_sent == SOCKET_ERROR)
						{
							/*
							wserror = WSAGetLastError();
							printf ("Could not send data to client...\n");
							printf ("Socket Error %u.\n", wserror );
							*/
							printf("与 %s 船失去连接...\n", workShip->name);
							initialize_ship(workShip);
						}
						else
							workShip->sndwritten += bytes_sent;

						if (workShip->sndwritten == workShip->snddata)
							workShip->sndwritten = workShip->snddata = 0;

					}

					if (workShip->todc)
					{
						if (workShip->snddata - workShip->sndwritten)
							send(workShip->shipSockfd, &workShip->sndbuf[workShip->sndwritten],
								workShip->snddata - workShip->sndwritten, 0);
						printf("终止与舰船的连接...\n");
						initialize_ship(workShip);
					}

				}
			}
		}
	}
#ifndef NO_SQL
	mysql_close(myData);
#endif
	return 0;
}


void send_to_server(int sock, char* packet)
{
	int pktlen;

	pktlen = strlen(packet);

	if (send(sock, packet, pktlen, 0) != pktlen)
	{
		printf("send_to_server(): 失败");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}

}

int receive_from_server(int sock, char* packet)
{
	int pktlen;

	if ((pktlen = recv(sock, packet, TCP_BUFFER_SIZE - 1, 0)) <= 0)
	{
		printf("receive_from_server(): 失败");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}
	packet[pktlen] = 0;
	return pktlen;
}

void tcp_listen(int sockfd)
{
	if (listen(sockfd, 10) < 0)
	{
		debug_perror("无法监听连接状态");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}
}

int tcp_accept(int sockfd, struct sockaddr* client_addr, int* addr_len)
{
	int fd;

	if ((fd = accept(sockfd, client_addr, addr_len)) < 0)
		debug_perror("无法接受连接");

	return (fd);
}

int tcp_sock_connect(char* dest_addr, int port)
{
	int fd;
	struct sockaddr_in sa;

	/* Clear it out */
	memset((void*)&sa, 0, sizeof(sa));

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	/* Error */
	if (fd < 0)
		debug_perror("无法创建套接字");
	else
	{

		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = inet_addr(dest_addr);
		sa.sin_port = htons((unsigned short)port);

		if (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0)
			debug_perror("无法建立TCP连接");
		else
			debug("tcp_sock_connect %s:%u", inet_ntoa(sa.sin_addr), sa.sin_port);
	}
	return(fd);
}

/*****************************************************************************/
int tcp_sock_open(struct in_addr ip, int port)
{
	int fd, turn_on_option_flag = 1, rcSockopt;

	struct sockaddr_in sa;

	/* Clear it out */
	memset((void*)&sa, 0, sizeof(sa));

	fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	/* Error */
	if (fd < 0) {
		debug_perror("无法创建套接字");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}

	sa.sin_family = AF_INET;
	memcpy((void*)&sa.sin_addr, (void*)&ip, sizeof(struct in_addr));
	sa.sin_port = htons((unsigned short)port);

	/* Reuse port */

	rcSockopt = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&turn_on_option_flag, sizeof(turn_on_option_flag));

	/* bind() the socket to the interface */
	if (bind(fd, (struct sockaddr*)&sa, sizeof(struct sockaddr)) < 0) {
		debug_perror("无法绑定端口");
		printf("按下 [回车键] 退出");
		gets_s(&dp[0], sizeof(dp));
		exit(1);
	}

	return(fd);
}

/*****************************************************************************
* same as debug_perror but writes to debug output.
*
*****************************************************************************/
void debug_perror(char* msg) {
	debug("%s : %s\n", msg, strerror_s(error_buffer, _countof(error_buffer), errno));
}
/*****************************************************************************/
void debug(char* fmt, ...)
{
#define MAX_MESG_LEN 1024

	va_list args;
	char text[MAX_MESG_LEN];

	va_start(args, fmt);
	vsprintf_s(text, _countof(text), fmt, args);
	strcat_s(text, _countof(text), "\r\n");
	va_end(args);

	fprintf(stderr, "%s", text);
}

/* Blue Burst encryption routines */

static void pso_crypt_init_key_bb(unsigned char* data)
{
	unsigned x;
	for (x = 0; x < 48; x += 3)
	{
		data[x] ^= 0x19;
		data[x + 1] ^= 0x16;
		data[x + 2] ^= 0x18;
	}
}


void pso_crypt_decrypt_bb(PSO_CRYPT* pcry, unsigned char* data, unsigned
	length)
{
	unsigned eax, ecx, edx, ebx, ebp, esi, edi;

	edx = 0;
	ecx = 0;
	eax = 0;
	while (edx < length)
	{
		ebx = *(unsigned long*)&data[edx];
		ebx = ebx ^ pcry->tbl[5];
		ebp = ((pcry->tbl[(ebx >> 0x18) + 0x12] + pcry->tbl[((ebx >> 0x10) & 0xff) + 0x112])
			^ pcry->tbl[((ebx >> 0x8) & 0xff) + 0x212]) + pcry->tbl[(ebx & 0xff) + 0x312];
		ebp = ebp ^ pcry->tbl[4];
		ebp ^= *(unsigned long*)&data[edx + 4];
		edi = ((pcry->tbl[(ebp >> 0x18) + 0x12] + pcry->tbl[((ebp >> 0x10) & 0xff) + 0x112])
			^ pcry->tbl[((ebp >> 0x8) & 0xff) + 0x212]) + pcry->tbl[(ebp & 0xff) + 0x312];
		edi = edi ^ pcry->tbl[3];
		ebx = ebx ^ edi;
		esi = ((pcry->tbl[(ebx >> 0x18) + 0x12] + pcry->tbl[((ebx >> 0x10) & 0xff) + 0x112])
			^ pcry->tbl[((ebx >> 0x8) & 0xff) + 0x212]) + pcry->tbl[(ebx & 0xff) + 0x312];
		ebp = ebp ^ esi ^ pcry->tbl[2];
		edi = ((pcry->tbl[(ebp >> 0x18) + 0x12] + pcry->tbl[((ebp >> 0x10) & 0xff) + 0x112])
			^ pcry->tbl[((ebp >> 0x8) & 0xff) + 0x212]) + pcry->tbl[(ebp & 0xff) + 0x312];
		edi = edi ^ pcry->tbl[1];
		ebp = ebp ^ pcry->tbl[0];
		ebx = ebx ^ edi;
		*(unsigned long*)&data[edx] = ebp;
		*(unsigned long*)&data[edx + 4] = ebx;
		edx = edx + 8;
	}
}


void pso_crypt_encrypt_bb(PSO_CRYPT* pcry, unsigned char* data, unsigned
	length)
{
	unsigned eax, ecx, edx, ebx, ebp, esi, edi;

	edx = 0;
	ecx = 0;
	eax = 0;
	while (edx < length)
	{
		ebx = *(unsigned long*)&data[edx];
		ebx = ebx ^ pcry->tbl[0];
		ebp = ((pcry->tbl[(ebx >> 0x18) + 0x12] + pcry->tbl[((ebx >> 0x10) & 0xff) + 0x112])
			^ pcry->tbl[((ebx >> 0x8) & 0xff) + 0x212]) + pcry->tbl[(ebx & 0xff) + 0x312];
		ebp = ebp ^ pcry->tbl[1];
		ebp ^= *(unsigned long*)&data[edx + 4];
		edi = ((pcry->tbl[(ebp >> 0x18) + 0x12] + pcry->tbl[((ebp >> 0x10) & 0xff) + 0x112])
			^ pcry->tbl[((ebp >> 0x8) & 0xff) + 0x212]) + pcry->tbl[(ebp & 0xff) + 0x312];
		edi = edi ^ pcry->tbl[2];
		ebx = ebx ^ edi;
		esi = ((pcry->tbl[(ebx >> 0x18) + 0x12] + pcry->tbl[((ebx >> 0x10) & 0xff) + 0x112])
			^ pcry->tbl[((ebx >> 0x8) & 0xff) + 0x212]) + pcry->tbl[(ebx & 0xff) + 0x312];
		ebp = ebp ^ esi ^ pcry->tbl[3];
		edi = ((pcry->tbl[(ebp >> 0x18) + 0x12] + pcry->tbl[((ebp >> 0x10) & 0xff) + 0x112])
			^ pcry->tbl[((ebp >> 0x8) & 0xff) + 0x212]) + pcry->tbl[(ebp & 0xff) + 0x312];
		edi = edi ^ pcry->tbl[4];
		ebp = ebp ^ pcry->tbl[5];
		ebx = ebx ^ edi;
		*(unsigned long*)&data[edx] = ebp;
		*(unsigned long*)&data[edx + 4] = ebx;
		edx = edx + 8;
	}
}

void encryptcopy(BANANA* client, const unsigned char* src, unsigned size)
{
	unsigned char* dest;

	if (TCP_BUFFER_SIZE - client->snddata < ((int)size + 7))
		client->todc = 1;
	else
	{
		dest = &client->sndbuf[client->snddata];
		memcpy(dest, src, size);
		while (size % 8)
			dest[size++] = 0x00;
		client->snddata += (int)size;
		pso_crypt_encrypt_bb(cipher_ptr, dest, size);
	}
}


void decryptcopy(unsigned char* dest, const unsigned char* src, unsigned size)
{
	memcpy(dest, src, size);
	pso_crypt_decrypt_bb(cipher_ptr, dest, size);
}


void pso_crypt_table_init_bb(PSO_CRYPT* pcry, const unsigned char* salt)
{
	unsigned long eax, ecx, edx, ebx, ebp, esi, edi, ou, x;
	unsigned char s[48];
	unsigned short* pcryp;
	unsigned short* bbtbl;
	unsigned short dx;

	pcry->cur = 0;
	pcry->mangle = NULL;
	pcry->size = 1024 + 18;

	memcpy(s, salt, sizeof(s));
	pso_crypt_init_key_bb(s);

	bbtbl = (unsigned short*)&bbtable[0];
	pcryp = (unsigned short*)&pcry->tbl[0];

	eax = 0;
	ebx = 0;

	for (ecx = 0;ecx < 0x12;ecx++)
	{
		dx = bbtbl[eax++];
		dx = ((dx & 0xFF) << 8) + (dx >> 8);
		pcryp[ebx] = dx;
		dx = bbtbl[eax++];
		dx ^= pcryp[ebx++];
		pcryp[ebx++] = dx;
	}

	/*

	pcry->tbl[0] = 0x243F6A88;
	pcry->tbl[1] = 0x85A308D3;
	pcry->tbl[2] = 0x13198A2E;
	pcry->tbl[3] = 0x03707344;
	pcry->tbl[4] = 0xA4093822;
	pcry->tbl[5] = 0x299F31D0;
	pcry->tbl[6] = 0x082EFA98;
	pcry->tbl[7] = 0xEC4E6C89;
	pcry->tbl[8] = 0x452821E6;
	pcry->tbl[9] = 0x38D01377;
	pcry->tbl[10] = 0xBE5466CF;
	pcry->tbl[11] = 0x34E90C6C;
	pcry->tbl[12] = 0xC0AC29B7;
	pcry->tbl[13] = 0xC97C50DD;
	pcry->tbl[14] = 0x3F84D5B5;
	pcry->tbl[15] = 0xB5470917;
	pcry->tbl[16] = 0x9216D5D9;
	pcry->tbl[17] = 0x8979FB1B;

	*/

	memcpy(&pcry->tbl[18], &bbtable[18], 4096);

	ecx = 0;
	//total key[0] length is min 0x412
	ebx = 0;

	while (ebx < 0x12)
	{
		//in a loop
		ebp = ((unsigned long)(s[ecx])) << 0x18;
		eax = ecx + 1;
		edx = eax - ((eax / 48) * 48);
		eax = (((unsigned long)(s[edx])) << 0x10) & 0xFF0000;
		ebp = (ebp | eax) & 0xffff00ff;
		eax = ecx + 2;
		edx = eax - ((eax / 48) * 48);
		eax = (((unsigned long)(s[edx])) << 0x8) & 0xFF00;
		ebp = (ebp | eax) & 0xffffff00;
		eax = ecx + 3;
		ecx = ecx + 4;
		edx = eax - ((eax / 48) * 48);
		eax = (unsigned long)(s[edx]);
		ebp = ebp | eax;
		eax = ecx;
		edx = eax - ((eax / 48) * 48);
		pcry->tbl[ebx] = pcry->tbl[ebx] ^ ebp;
		ecx = edx;
		ebx++;
	}

	ebp = 0;
	esi = 0;
	ecx = 0;
	edi = 0;
	ebx = 0;
	edx = 0x48;

	while (edi < edx)
	{
		esi = esi ^ pcry->tbl[0];
		eax = esi >> 0x18;
		ebx = (esi >> 0x10) & 0xff;
		eax = pcry->tbl[eax + 0x12] + pcry->tbl[ebx + 0x112];
		ebx = (esi >> 8) & 0xFF;
		eax = eax ^ pcry->tbl[ebx + 0x212];
		ebx = esi & 0xff;
		eax = eax + pcry->tbl[ebx + 0x312];

		eax = eax ^ pcry->tbl[1];
		ecx = ecx ^ eax;
		ebx = ecx >> 0x18;
		eax = (ecx >> 0x10) & 0xFF;
		ebx = pcry->tbl[ebx + 0x12] + pcry->tbl[eax + 0x112];
		eax = (ecx >> 8) & 0xff;
		ebx = ebx ^ pcry->tbl[eax + 0x212];
		eax = ecx & 0xff;
		ebx = ebx + pcry->tbl[eax + 0x312];

		for (x = 0; x <= 5; x++)
		{
			ebx = ebx ^ pcry->tbl[(x * 2) + 2];
			esi = esi ^ ebx;
			ebx = esi >> 0x18;
			eax = (esi >> 0x10) & 0xFF;
			ebx = pcry->tbl[ebx + 0x12] + pcry->tbl[eax + 0x112];
			eax = (esi >> 8) & 0xff;
			ebx = ebx ^ pcry->tbl[eax + 0x212];
			eax = esi & 0xff;
			ebx = ebx + pcry->tbl[eax + 0x312];

			ebx = ebx ^ pcry->tbl[(x * 2) + 3];
			ecx = ecx ^ ebx;
			ebx = ecx >> 0x18;
			eax = (ecx >> 0x10) & 0xFF;
			ebx = pcry->tbl[ebx + 0x12] + pcry->tbl[eax + 0x112];
			eax = (ecx >> 8) & 0xff;
			ebx = ebx ^ pcry->tbl[eax + 0x212];
			eax = ecx & 0xff;
			ebx = ebx + pcry->tbl[eax + 0x312];
		}

		ebx = ebx ^ pcry->tbl[14];
		esi = esi ^ ebx;
		eax = esi >> 0x18;
		ebx = (esi >> 0x10) & 0xFF;
		eax = pcry->tbl[eax + 0x12] + pcry->tbl[ebx + 0x112];
		ebx = (esi >> 8) & 0xff;
		eax = eax ^ pcry->tbl[ebx + 0x212];
		ebx = esi & 0xff;
		eax = eax + pcry->tbl[ebx + 0x312];

		eax = eax ^ pcry->tbl[15];
		eax = ecx ^ eax;
		ecx = eax >> 0x18;
		ebx = (eax >> 0x10) & 0xFF;
		ecx = pcry->tbl[ecx + 0x12] + pcry->tbl[ebx + 0x112];
		ebx = (eax >> 8) & 0xff;
		ecx = ecx ^ pcry->tbl[ebx + 0x212];
		ebx = eax & 0xff;
		ecx = ecx + pcry->tbl[ebx + 0x312];

		ecx = ecx ^ pcry->tbl[16];
		ecx = ecx ^ esi;
		esi = pcry->tbl[17];
		esi = esi ^ eax;
		pcry->tbl[(edi / 4)] = esi;
		pcry->tbl[(edi / 4) + 1] = ecx;
		edi = edi + 8;
	}


	eax = 0;
	edx = 0;
	ou = 0;
	while (ou < 0x1000)
	{
		edi = 0x48;
		edx = 0x448;

		while (edi < edx)
		{
			esi = esi ^ pcry->tbl[0];
			eax = esi >> 0x18;
			ebx = (esi >> 0x10) & 0xff;
			eax = pcry->tbl[eax + 0x12] + pcry->tbl[ebx + 0x112];
			ebx = (esi >> 8) & 0xFF;
			eax = eax ^ pcry->tbl[ebx + 0x212];
			ebx = esi & 0xff;
			eax = eax + pcry->tbl[ebx + 0x312];

			eax = eax ^ pcry->tbl[1];
			ecx = ecx ^ eax;
			ebx = ecx >> 0x18;
			eax = (ecx >> 0x10) & 0xFF;
			ebx = pcry->tbl[ebx + 0x12] + pcry->tbl[eax + 0x112];
			eax = (ecx >> 8) & 0xff;
			ebx = ebx ^ pcry->tbl[eax + 0x212];
			eax = ecx & 0xff;
			ebx = ebx + pcry->tbl[eax + 0x312];

			for (x = 0; x <= 5; x++)
			{
				ebx = ebx ^ pcry->tbl[(x * 2) + 2];
				esi = esi ^ ebx;
				ebx = esi >> 0x18;
				eax = (esi >> 0x10) & 0xFF;
				ebx = pcry->tbl[ebx + 0x12] + pcry->tbl[eax + 0x112];
				eax = (esi >> 8) & 0xff;
				ebx = ebx ^ pcry->tbl[eax + 0x212];
				eax = esi & 0xff;
				ebx = ebx + pcry->tbl[eax + 0x312];

				ebx = ebx ^ pcry->tbl[(x * 2) + 3];
				ecx = ecx ^ ebx;
				ebx = ecx >> 0x18;
				eax = (ecx >> 0x10) & 0xFF;
				ebx = pcry->tbl[ebx + 0x12] + pcry->tbl[eax + 0x112];
				eax = (ecx >> 8) & 0xff;
				ebx = ebx ^ pcry->tbl[eax + 0x212];
				eax = ecx & 0xff;
				ebx = ebx + pcry->tbl[eax + 0x312];
			}

			ebx = ebx ^ pcry->tbl[14];
			esi = esi ^ ebx;
			eax = esi >> 0x18;
			ebx = (esi >> 0x10) & 0xFF;
			eax = pcry->tbl[eax + 0x12] + pcry->tbl[ebx + 0x112];
			ebx = (esi >> 8) & 0xff;
			eax = eax ^ pcry->tbl[ebx + 0x212];
			ebx = esi & 0xff;
			eax = eax + pcry->tbl[ebx + 0x312];

			eax = eax ^ pcry->tbl[15];
			eax = ecx ^ eax;
			ecx = eax >> 0x18;
			ebx = (eax >> 0x10) & 0xFF;
			ecx = pcry->tbl[ecx + 0x12] + pcry->tbl[ebx + 0x112];
			ebx = (eax >> 8) & 0xff;
			ecx = ecx ^ pcry->tbl[ebx + 0x212];
			ebx = eax & 0xff;
			ecx = ecx + pcry->tbl[ebx + 0x312];

			ecx = ecx ^ pcry->tbl[16];
			ecx = ecx ^ esi;
			esi = pcry->tbl[17];
			esi = esi ^ eax;
			pcry->tbl[(ou / 4) + (edi / 4)] = esi;
			pcry->tbl[(ou / 4) + (edi / 4) + 1] = ecx;
			edi = edi + 8;
		}
		ou = ou + 0x400;
	}
}

unsigned RleEncode(unsigned char* src, unsigned char* dest, unsigned src_size)
{
	unsigned char currChar, prevChar;             /* current and previous characters */
	unsigned short count;                /* number of characters in a run */
	unsigned src_end, dest_start;

	dest_start = (unsigned)dest;
	src_end = (unsigned)src + src_size;

	prevChar = 0xFF - *src;

	while ((unsigned)src < src_end)
	{
		currChar = *(dest++) = *(src++);

		if (currChar == prevChar)
		{
			if ((unsigned)src == src_end)
			{
				*(dest++) = 0;
				*(dest++) = 0;
			}
			else
			{
				count = 0;
				while (((unsigned)src < src_end) && (count < 0xFFF0))
				{
					if (*src == prevChar)
					{
						count++;
						src++;
						if ((unsigned)src == src_end)
						{
							*(unsigned short*)dest = count;
							dest += 2;
						}
					}
					else
					{
						*(unsigned short*)dest = count;
						dest += 2;
						prevChar = 0xFF - *src;
						break;
					}
				}
			}
		}
		else
			prevChar = currChar;
	}
	return (unsigned)dest - dest_start;
}

void RleDecode(unsigned char* src, unsigned char* dest, unsigned src_size)
{
	unsigned char currChar, prevChar;             /* current and previous characters */
	unsigned short count;                /* number of characters in a run */
	unsigned src_end;

	src_end = (unsigned)src + src_size;

	/* decode */

	prevChar = 0xFF - *src;     /* force next char to be different */

								/* read input until there's nothing left */

	while ((unsigned)src < src_end)
	{
		currChar = *(src++);

		*(dest++) = currChar;

		/* check for run */
		if (currChar == prevChar)
		{
			/* we have a run.  write it out. */
			count = *(unsigned short*)src;
			src += 2;
			while (count > 0)
			{
				*(dest++) = currChar;
				count--;
			}

			prevChar = 0xFF - *src;     /* force next char to be different */
		}
		else
		{
			/* no run */
			prevChar = currChar;
		}
	}
}


/* expand a key (makes a rc4_key) */

void prepare_key(unsigned char* keydata, unsigned len, struct rc4_key* key)
{
	unsigned index1, index2, counter;
	unsigned char* state;

	state = key->state;

	for (counter = 0; counter < 256; counter++)
		state[counter] = counter;

	key->x = key->y = index1 = index2 = 0;

	for (counter = 0; counter < 256; counter++) {
		index2 = (keydata[index1] + state[counter] + index2) & 255;

		/* swap */
		state[counter] ^= state[index2];
		state[index2] ^= state[counter];
		state[counter] ^= state[index2];

		index1 = (index1 + 1) % len;
	}
}

/* reversible encryption, will encode a buffer updating the key */

void rc4(unsigned char* buffer, unsigned len, struct rc4_key* key)
{
	unsigned x, y, xorIndex, counter;
	unsigned char* state;

	/* get local copies */
	x = key->x; y = key->y;
	state = key->state;

	for (counter = 0; counter < len; counter++) {
		x = (x + 1) & 255;
		y = (state[x] + y) & 255;

		/* swap */
		state[x] ^= state[y];
		state[y] ^= state[x];
		state[x] ^= state[y];

		xorIndex = (state[y] + state[x]) & 255;

		buffer[counter] ^= state[xorIndex];
	}

	key->x = x; key->y = y;
}

void compressShipPacket(ORANGE* ship, unsigned char* src, unsigned long src_size)
{
	unsigned char* dest;
	unsigned long result;

	if (ship->shipSockfd >= 0)
	{
		if (PACKET_BUFFER_SIZE - ship->snddata < (int)(src_size + 100))
			initialize_ship(ship);
		else
		{
			if (ship->crypt_on)
			{
				dest = &ship->sndbuf[ship->snddata];
				// Store the original packet size before RLE compression at offset 0x04 of the new packet.
				dest += 4;
				*(unsigned*)dest = src_size;
				// Compress packet using RLE, storing at offset 0x08 of new packet.
				//
				// result = size of RLE compressed data + a DWORD for the original packet size.
				result = RleEncode(src, dest + 4, src_size) + 4;
				// Encrypt with RC4
				rc4(dest, result, &ship->sc_key);
				// Increase result by the size of a DWORD for the final ship packet size.
				result += 4;
				// Copy it to the front of the packet.
				*(unsigned*)&ship->sndbuf[ship->snddata] = result;
				ship->snddata += (int)result;
			}
			else
			{
				memcpy(&ship->sndbuf[ship->snddata + 4], src, src_size);
				src_size += 4;
				*(unsigned*)&ship->sndbuf[ship->snddata] = src_size;
				ship->snddata += src_size;
			}
		}
	}
}

void decompressShipPacket(ORANGE* ship, unsigned char* dest, unsigned char* src)
{
	unsigned src_size, dest_size;
	unsigned char* srccpy;

	if (ship->crypt_on)
	{
		src_size = *(unsigned*)src;
		src_size -= 8;
		src += 4;
		srccpy = src;
		// Decrypt RC4
		rc4(src, src_size + 4, &ship->cs_key);
		// The first four bytes of the src should now contain the expected uncompressed data size.
		dest_size = *(unsigned*)srccpy;
		// Increase expected size by 4 before inserting into the destination buffer.  (To take account for the packet
		// size DWORD...)
		dest_size += 4;
		*(unsigned*)dest = dest_size;
		// Decompress the data...
		RleDecode(srccpy + 4, dest + 4, src_size);
	}
	else
	{
		src_size = *(unsigned*)src;
		memcpy(dest + 4, src + 4, src_size);
		src_size += 4;
		*(unsigned*)dest = src_size;
	}
}
