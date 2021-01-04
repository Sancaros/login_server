#pragma once
/* Ship Packets */

unsigned char ShipPacket00[] = {
	0x00, 0x00, 0x3F, 0x01, 0x03, 0x04, 0x19, 0x55, 0x54, 0x65, 0x74, 0x68, 0x65, 0x61, 0x6C, 0x6C,
	0x61, 0x20, 0x4C, 0x6F, 0x67, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* Start Encryption Packet */

unsigned char Packet03[] = {
	0xC8, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x68, 0x61, 0x6E, 0x74, 0x61, 0x73, 0x79,
	0x20, 0x53, 0x74, 0x61, 0x72, 0x20, 0x4F, 0x6E, 0x6C, 0x69, 0x6E, 0x65, 0x20, 0x42, 0x6C, 0x75,
	0x65, 0x20, 0x42, 0x75, 0x72, 0x73, 0x74, 0x20, 0x47, 0x61, 0x6D, 0x65, 0x20, 0x53, 0x65, 0x72,
	0x76, 0x65, 0x72, 0x2E, 0x20, 0x43, 0x6F, 0x70, 0x79, 0x72, 0x69, 0x67, 0x68, 0x74, 0x20, 0x31,
	0x39, 0x39, 0x39, 0x2D, 0x32, 0x30, 0x30, 0x34, 0x20, 0x53, 0x4F, 0x4E, 0x49, 0x43, 0x54, 0x45,
	0x41, 0x4D, 0x2E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30
};

/* Server Redirect */

const unsigned char Packet19[] = {
	0x10, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


/* Login message 登陆信息 */

const unsigned char Packet1A[] = {
	0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x45, 0x00
};

/* Ping pong 发送心跳包*/

const unsigned char Packet1D[] = {
	0x08, 0x00, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Current time 当前时间*/

const unsigned char PacketB1[] = {
	0x24, 0x00, 0xB1, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Guild Card Data */

unsigned char PacketDC01[0x14] = { 0 };
unsigned char PacketDC02[0x10] = { 0 };
unsigned char PacketDC_Check[54672] = { 0 };

/* No Character Header */

unsigned char PacketE4[0x10] = { 0 };


/* Character Header */

unsigned char PacketE5[0x88] = { 0 };

/* Security Packet */

const unsigned char PacketE6[] = {
	0x44, 0x00, 0xE6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x3F, 0x71, 0x8D, 0x34, 0x37, 0x7A, 0xBD,
	0x67, 0x39, 0x65, 0x6B, 0x2C, 0xB1, 0xA5, 0x7C, 0x17, 0x93, 0x93, 0x29, 0x4A, 0x90, 0xE9, 0x11,
	0xB8, 0xB5, 0x0E, 0x77, 0x41, 0x30, 0x9B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x01, 0x00, 0x00
};

/* Acknowledging client's expected checksum */

const unsigned char PacketE8[] = {
	0x0C, 0x00, 0xE8, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00
};

/* The "Top Broadcast" Packet */

const unsigned char PacketEE[] = {
	0x00, 0x00, 0xEE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*挑战模式的数据包*/
const unsigned char PacketD1[] = {
	//(前面的所有元素都是角色基础数据用于获取角色的对应信息)
	//shipEvent = 0x0E
	//client->block = 0x0C
	//0x06 teamid / slotnum
	//0x03 选项config 用于检索数据查看数据
	//0x01 episode 应该是代表的章节
	//0x07 是否是GMisgm
	//0x04 对应的ship
	//0x02 96
	//0x08 ip 1 93 C-Rank and Battle Config 挑战设置 slotnum 
	//0x05
	//0x09 ip 2 9
	//0x12
	//0x0F
	//0x10 gcn l->clientx[client->clientID];
	//0x11
	//0x0D
	//0x0A slotnum
	//0x0B ip 4 l->quest_loaded 251 isgm
	//0x0C l->floor[client->clientID
	//0x0E (任务进度)
	//后面的都是预留的NULL存储单元
	0x00, 0x00, 0x06, 0x00, 0x03, 0x00, 0x01, 0x00, 0x07, 0x00, 0x04, 0x00, 0x02, 0x00, 0x08, 0x00,
	0x05, 0x00, 0x09, 0x00, 0x12, 0x00, 0x0F, 0x00, 0x10, 0x00, 0x11, 0x00, 0x0D, 0x00, 0x0A, 0x00,
	0x0B, 0x00, 0x0C, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char E2_Base[2808] = { 0 };
unsigned char PacketE2Data[2808] = { 0 };