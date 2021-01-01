
const wchar_t Message03[22] = { L"Tethealla 登录服务器 v.053" };
// const char *PSO_CLIENT_VER_STRING = "TethVer12510";
const char* PSO_CLIENT_VER_STRING = "TethVer12513";
#define PSO_CLIENT_VER 0x41

#define MAX_SIMULTANEOUS_CONNECTIONS 6
#define LOGIN_COMPILED_MAX_CONNECTIONS 300
#define SHIP_COMPILED_MAX_CONNECTIONS 50
#define MAX_EB02 800000
#define SERVER_VERSION "0.053"
#define MAX_ACCOUNTS 2000
#define MAX_DRESS_FLAGS 500
#define DRESS_FLAG_EXPIRY 7200
#define PSO_CLIENT_VER 0x41


//#define USEADDR_ANY
#define SHIP_LISTEN_PORT 3455
#define DEBUG_OUTPUT
#define TCP_BUFFER_SIZE 64000
#define PACKET_BUFFER_SIZE ( TCP_BUFFER_SIZE * 16 )

//#define USEADDR_ANY

#define SEND_PACKET_03 0x00 // Done
#define SEND_PACKET_E6 0x01 // Done
#define SEND_PACKET_E2 0x02 // Done
#define SEND_PACKET_E5 0x03 // Done
#define SEND_PACKET_E8 0x04 // Done接收公会卡
#define SEND_PACKET_DC 0x05 // Done移除公会卡
#define SEND_PACKET_EB 0x06 // Done设置公会卡信息
#define SEND_PACKET_E4 0x07 // Done新增屏蔽玩家
#define SEND_PACKET_B1 0x08 // 移除被屏蔽的玩家
#define SEND_PACKET_A0 0x09 //为玩家写评论
#define RECEIVE_PACKET_93 0x0A //分类工会卡
#define MAX_SENDCHECK 0x0B

#define CLASS_HUMAR 0x00
#define CLASS_HUNEWEARL 0x01
#define CLASS_HUCAST 0x02
#define CLASS_RAMAR 0x03
#define CLASS_RACAST 0x04
#define CLASS_RACASEAL 0x05
#define CLASS_FOMARL 0x06
#define CLASS_FONEWM 0x07
#define CLASS_FONEWEARL 0x08
#define CLASS_HUCASEAL 0x09
#define CLASS_FOMAR 0x0A
#define CLASS_RAMARL 0x0B
#define CLASS_MAX 0x0C