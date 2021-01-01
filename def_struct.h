#define NO_ALIGN __declspec(align(4))

/* Mag data structure */

typedef struct NO_ALIGN st_mag
//typedef struct st_mag
{
	unsigned char two; // "02" =P
	unsigned char mtype;
	unsigned char level;
	unsigned char blasts;
	short defense;
	short power;
	short dex;
	short mind;
	unsigned itemid;
	char synchro;
	unsigned char IQ;
	unsigned char PBflags;
	unsigned char color;
} MAG;


/* Character Data Structure ��ɫ���ݽṹ*/

typedef struct NO_ALIGN st_minichar {
	//typedef struct st_minichar {
	unsigned short packetSize; // 0x00 - 0x01
	unsigned short command; // 0x02 - 0x03
	unsigned char flags[4]; // 0x04 - 0x07
	unsigned char unknown[8]; // 0x08 - 0x0F �����λ Ѫ�� ħ��ֵ 
	unsigned short level; // 0x10 - 0x11
	unsigned short reserved; // 0x12 - 0x13
	char gcString[10]; // 0x14 - 0x1D
	unsigned char unknown2[14]; // 0x1E - 0x2B
	unsigned char nameColorBlue; // 0x2C
	unsigned char nameColorGreen; // 0x2D
	unsigned char nameColorRed; // 0x2E
	unsigned char nameColorTransparency; // 0x2F
	unsigned short skinID; // 0x30 - 0x31
	unsigned char unknown3[18]; // 0x32 - 0x43
	unsigned char sectionID; // 0x44
	unsigned char _class; // 0x45
	unsigned char skinFlag; // 0x46
	unsigned char unknown4[5]; // 0x47 - 0x4B (same as unknown5 in E7)
	unsigned short costume; // 0x4C - 0x4D
	unsigned short skin; // 0x4E - 0x4F
	unsigned short face; // 0x50 - 0x51
	unsigned short head; // 0x52 - 0x53
	unsigned short hair; // 0x54 - 0x55
	unsigned short hairColorRed; // 0x56 - 0x57
	unsigned short hairColorBlue; // 0x58 - 0x59
	unsigned short hairColorGreen; // 0x5A - 0x5B
	unsigned proportionX; // 0x5C - 0x5F
	unsigned proportionY; // 0x60 - 0x63
	unsigned char name[24]; // 0x64 - 0x7B
	unsigned char unknown5[8]; // 0x7C - 0x83
	unsigned playTime;
} MINICHAR;

//packetSize = 0x399C;
//command = 0x00E7;

typedef struct NO_ALIGN st_bank_item {
	//typedef struct st_bank_item {
	unsigned char data[12]; // the standard $setitem1 - $setitem3 fare
	unsigned itemid; // player item id
	unsigned char data2[4]; // $setitem4 (mag use only)
	unsigned bank_count; // Why?
} BANK_ITEM;

typedef struct NO_ALIGN st_bank {
	//typedef struct st_bank {
	unsigned bankUse;
	unsigned bankMeseta;
	BANK_ITEM bankInventory[200];
} BANK;

typedef struct NO_ALIGN st_challenge_data {
	//typedef struct st_bank {
	unsigned char challengeData[320];
} CHALLENGEDATA;

typedef struct NO_ALIGN st_battle_data {
	unsigned char battleData[88];
} BATTLEDATA;

typedef struct NO_ALIGN st_item {
	//typedef struct st_item {
	unsigned char data[12]; // the standard $setitem1 - $setitem3 fare
	unsigned itemid; // player item id
	unsigned char data2[4]; // $setitem4 (mag use only)
} ITEM;

typedef struct NO_ALIGN st_inventory {
	//typedef struct st_inventory_item {
	unsigned in_use; // 0x01 = item slot in use, 0xFF00 = unused
	unsigned flags; // 8 = equipped
	ITEM item;
} INVENTORY_ITEM;

/* Main Character Structure ��Ҫ��ɫ�ṹ*/
typedef struct NO_ALIGN st_chardata {
	unsigned short packetSize; // 0x00-0x01 0 - 1 2 ����  Always set to 0x399C ����14748�ֽ� �޸�Ϊ0x39A0 ���� 14752 ���ֽڳ���
	unsigned short command; // 0x02-0x03 2 - 3 Always set to 0x00E7 ָ��ռ��231�ֽ� 2 ����
							//��ɫ�Ļ�������
	unsigned char flags[4]; // 0x04-0x07 4 - 7 ����ְҵ�İɣ� 4 ����
	unsigned char inventoryUse; // 0x08 8 �����λ 1 ����
	unsigned char HPmat; // 0x09 Ѫ�� 9 1 ����
	unsigned char TPmat; // 0x0A ħ��ֵ 10 1 ����
	unsigned char lang; // 0x0B ���� 11 1 ����
	INVENTORY_ITEM inventory[30]; // 0x0C-0x353 //30�񱳰� 12 - 851 840 ����
								  //��ŵ�����
	unsigned short ATP; // 0x354-0x355 852 - 853 2 ����
	unsigned short MST; // 0x356-0x357 854 - 855 2 ����
	unsigned short EVP; // 0x358-0x359 856 - 857 2 ����
	unsigned short HP; // 0x35A-0x35B 858 - 859 2 ����
	unsigned short DFP; // 0x35C-0x35D 860 - 861 2 ����
	unsigned short ATA; // 0x35E-0x35F 862 - 863 2 ����
	unsigned short LCK; // 0x360-0x361 864 - 865 2 ����
						//unsigned char unknown[10]; // 0x362-0x36B 866 - 875 10 ����
	unsigned char option_flags[10]; // 0x362-0x36B 866 - 875 10 ����
	unsigned short level; // 0x36C-0x36D; 876 - 877 2 ����
	unsigned short unknown2; // 0x36E-0x36F; 878 - 879 2 ����
	unsigned XP; // 0x370-0x373 880 - 883 4 ����
	unsigned meseta; // 0x374-0x377; 884 - 887 4 ����
	char gcString[10]; // 0x378-0x381; 888 - 897 10 ����
	unsigned char unknown3[14]; // 0x382-0x38F; 898 - 911 4 ����  // Same as E5 unknown2 ��E5ָ��� δ֪���� 2 һ��
	unsigned char nameColorBlue; // 0x390; 912  1 ����
	unsigned char nameColorGreen; // 0x391; 913 1 ����
	unsigned char nameColorRed; // 0x392; 914 1 ����
	unsigned char nameColorTransparency; // 0x393; 915 1 ����
	unsigned short skinID; // 0x394-0x395; 916 - 917 2 ����
	unsigned char unknown4[18]; // 0x396-0x3A7 918 - 935 18����
	unsigned char sectionID; // 0x3A8; 936 1 ����
	unsigned char _class; // 0x3A9; 937 1 ����
	unsigned char skinFlag; // 0x3AA; 938 1 ����
	unsigned char unknown5[5]; // 0x3AB-0x3AF; 939 - 943 5 ���� // Same as E5 unknown4. ��E5ָ��� δ֪���� 4 һ��
	unsigned short costume; // 0x3B0 - 0x3B1; 944 - 945 2 ���� //��ɫ��װ
	unsigned short skin; // 0x3B2 - 0x3B3; 946 - 947 2 ����
	unsigned short face; // 0x3B4 - 0x3B5; 948 - 949 2 ����
	unsigned short head; // 0x3B6 - 0x3B7; 950 - 951 2 ����
	unsigned short hair; // 0x3B8 - 0x3B9; 952 - 953 2 ����
	unsigned short hairColorRed; // 0x3BA-0x3BB; 954 - 959 6 ����
	unsigned short hairColorBlue; // 0x3BC-0x3BD; 960 - 961 2 ����
	unsigned short hairColorGreen; // 0x3BE-0x3BF; 958 - 959 2 ����
	unsigned proportionX; // 0x3C0-0x3C3; 960 - 963 4 ����
	unsigned proportionY; // 0x3C4-0x3C7; 964 - 967 4 ����
	unsigned char name[24]; // 0x3C8-0x3DF; 968 - 991 4 ���� ����1
	unsigned playTime; // 0x3E0 - 0x3E3 992 - 995 4 ����
					   //��½��������������clientchar->unknown5
	unsigned char unknown6[4]; // 0x3E4 - 0x3E7; 996 - 999 4 ���� //������С�����ɫ���Ͻṹ
	unsigned char keyConfig[232]; // 0x3E8 - 0x4CF; 1000 - 1231 232 ����
								  // Stored from ED 07 packet.  ��ED 07���д洢�� 
	unsigned char techniques[20]; // 0x4D0 - 0x4E3; 1232 - 1251 20����
	unsigned char name3[16]; // 0x4E4 - 0x4F3; 1252 - 1267 16���� ����3
	unsigned char options[4]; // 0x4F4-0x4F7; 1268 - 1271 4 ���� //����ѡ�
							  // Stored from ED 01 packet.
	unsigned char quest_data1[520]; // 0x4F8 - 0x6FF; 1272 - 1791 512 ���� ���� 1 ����
									// ���вֿ����
	unsigned bankUse; // 0x700 - 0x703 1792 - 1795 4 ����
	unsigned bankMeseta; // 0x704 - 0x707; 1796 - 1799 4 ����
	BANK_ITEM bankInventory[200]; // 0x708 - 0x19C7 1800 - 6599 200 ����
	unsigned guildCard; // 0x19C8-0x19CB; 6600 - 6603 4 ����
								  //��Ƭ��ص�
								  // Stored from E8 06 packet.
	unsigned char friendName[24]; // 0x19CC - 0x19E3; 6604 - 6627 24 ����
	unsigned char unknown9[56]; // 0x19E4-0x1A1B; 6628 - 6683 56 ����
	unsigned char friendText[176]; // 0x1A1C - 0x1ACB 6684 - 6859 176 ����
	unsigned char reserved1;  // 0x1ACC; 6860 1 ���� // ��Schthack����ֵ0x01
	unsigned char reserved2; // 0x1ACD; 6861 1 ���� // ��Schthack����ֵ0x01
	unsigned char sectionID2; // 0x1ACE; 6862 1 ����
	unsigned char _class2; // 0x1ACF; 6863 1 ����
	unsigned char unknown10[4]; // 0x1AD0-0x1AD3; 6864 - 6867 4 ����
	unsigned char symbol_chats[1248]; // 0x1AD4 - 0x1FB3 6868 - 8115 1248 ����
									  // Stored from ED 02 packet.
	unsigned char shortcuts[2624]; // 0x1FB4 - 0x29F3 8116 - 10739 2624 ����
								   // Stored from ED 03 packet.
	unsigned char autoReply[344]; // 0x29F4 - 0x2B4B; 10740 - 11083 344 ����
	unsigned char GCBoard[172]; // 0x2B4C - 0x2BF7; 11084 - 11255 172 ����
	unsigned char unknown12[200]; // 0x2BF8 - 0x2CBF; 11256 - 11455 200 ����
	unsigned char challengeData[320]; // 0x2CC0 - 0x2DFF 11456 - 11775 320 ����
									  //unsigned char unknown13[172]; 
									  // 0x2E00 - 0x2EAB; 11776 - 11947 �ֽ�Ϊ��������
	unsigned char techConfig[40]; // 0x2E00 - 0x2E27 11776 - 11815 40 ���� ħ������
	//unsigned char unknown13[40]; // 0x2E28-0x2E4F 11816 - 11855 40 ���� δ֪
	//unsigned char battleData[92]; // 0x2E50 - 0x2EAB (Quest data 2 ��������2) 11856 - 11947
	unsigned char unknown13[44]; // 0x2E28-0x2E53 11816 - 11859
	unsigned char battleData[88];// 0x2E54 - 0x2EAB (Quest data 2 ��������2) 11860 - 11947
								  // I don't know what this is, but split from unknown13 because this chunk is
								  // actually copied into the 0xE2 packet during login @ 0x08 
								  //�Ҳ�֪������ʲô�����Ǵ�unknown13��ʼ��֣���Ϊ�����ʵ�������ڵ�¼�ڼ临�Ƶ�0xE2���е� 
	unsigned char unknown14[276]; // 0x2EAC - 0x2FBF; 11948 - 12223 ��ʱδ֪ ������newserv�й���Ϊ����Ľṹ
	unsigned char keyConfigGlobal[364]; // 0x2FC0 - 0x312B  12224 - 12587 ��Ϸ��λ���� key_data ���ݿ�
										// Copied into 0xE2 login packet @ 0x11C ���Ƶ�0xE2��¼��@0x11C 
										// Stored from ED 04 packet.
	unsigned char joyConfigGlobal[56]; // 0x312C - 0x3163 12588 - 12643 �ֱ����� key_data ���ݿ�
									   // Copied into 0xE2 login packet @ 0x288 ���ƽ�0xE2��½������
									   // Stored from ED 05 packet.��ED 05���ݰ��ռ�
	unsigned serial_number; // 0x3164 - 0x3167 12644 - 12647 4 ���� ͨ������һ�����к� Ψһ
							//(From here on copied into 0xE2 login packet @ 0x2C0...)
	unsigned teamID; // 0x3168 - 0x316B 12648 - 12651 5 ���� int
	unsigned char teamInformation[8]; // 0x316C - 0x3173 12652 - 12659 8 ���� ������Ϣ (usually blank...)ͨ���ǿհ׵�״̬
	unsigned short privilegeLevel; // 0x3174 - 0x3175 12660 - 12661 2 ���� ������Ȩ�ȼ� �����ڲ�����
	unsigned short reserved3; // 0x3176 - 0x3177 12662 - 12663 2 ���� �����Ķ���
	unsigned char teamName[28]; // 0x3178 - 0x3193 12664 - 12691 28 ���� �������� tinyblob
	unsigned teamRank; // 0x3194 - 0x3197 12692 - 12695 4 ���� δ֪ �ᱻд��ܴ�һ����ֵ
	unsigned char teamFlag[2048]; // 0x3198 - 0x3997 12696 - 14743 2048 ���� ���ǹ���ı�־
	unsigned char teamRewards[8]; // 0x3998 - 0x39A0 14744 - 14752 8 ���� ���ǹ�����ص佱���ɣ�
} CHARDATA;

/* Player Structure */

typedef struct st_banana {
	int plySockfd;
	int login;
	unsigned char peekbuf[8];
	unsigned char rcvbuf[TCP_BUFFER_SIZE];
	unsigned short rcvread;
	unsigned short expect;
	unsigned char decryptbuf[TCP_BUFFER_SIZE];
	unsigned char sndbuf[TCP_BUFFER_SIZE];
	unsigned char encryptbuf[TCP_BUFFER_SIZE];
	int snddata,
		sndwritten;
	unsigned char packet[TCP_BUFFER_SIZE];
	unsigned short packetdata;
	unsigned short packetread;
	int crypt_on;
	PSO_CRYPT server_cipher, client_cipher;
	unsigned guildcard;
	char guildcard_string[12];
	unsigned char guildcard_data[20000];
	int sendingchars;
	short slotnum;
	unsigned lastTick;  // The last second
	unsigned toBytesSec; // How many bytes per second the server sends to the client
	unsigned fromBytesSec; // How many bytes per second the server receives from the client
	unsigned packetsSec; // How many packets per second the server receives from the client
	unsigned connected;
	unsigned char sendCheck[MAX_SENDCHECK + 2];
	int todc;
	unsigned char IP_Address[16];
	char hwinfo[18];
	int isgm;
	int dress_flag;
	unsigned connection_index;
} BANANA;

typedef struct st_dressflag {
	unsigned guildcard;
	unsigned flagtime;
} DRESSFLAG;

/* a RC4 expanded key session */
struct rc4_key {
	unsigned char state[256];
	unsigned x, y;
};

/* Ship Structure */

typedef struct st_orange {
	int shipSockfd;
	unsigned char name[13];
	unsigned playerCount;
	unsigned char shipAddr[5];
	unsigned char listenedAddr[4];
	unsigned short shipPort;
	unsigned char rcvbuf[TCP_BUFFER_SIZE];
	unsigned long rcvread;
	unsigned long expect;
	unsigned char decryptbuf[TCP_BUFFER_SIZE];
	unsigned char sndbuf[PACKET_BUFFER_SIZE];
	unsigned char encryptbuf[TCP_BUFFER_SIZE];
	unsigned char packet[PACKET_BUFFER_SIZE];
	unsigned long packetread;
	unsigned long packetdata;
	int snddata,
		sndwritten;
	unsigned shipID;
	int authed;
	int todc;
	int crypt_on;
	unsigned char user_key[128];
	int key_change[128];
	unsigned key_index;
	struct rc4_key cs_key; // Encryption keys
	struct rc4_key sc_key; // Encryption keys
	unsigned connection_index;
	unsigned connected;
	unsigned last_ping;
	int sent_ping;
} ORANGE;