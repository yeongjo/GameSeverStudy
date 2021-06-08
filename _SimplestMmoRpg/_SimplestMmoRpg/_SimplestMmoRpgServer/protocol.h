#pragma once

constexpr int MAX_STR_LEN = 50;
constexpr int MAX_ID_LEN = 20;

#define QUICKTEST
#ifndef QUICKTEST
constexpr int MAX_USER = 210000;                // 서버내의 최대 객체 개수,  객체 ID의 최대 값
constexpr int NPC_ID_START = 10000;		// NPC의 ID가 시작하는 지점, 따라서 플레이어는 0부터 NPC_ID_START까지의 ID를 가짐
					// NPC의 개수는 MAX_USER - NPC_ID_START = 20000,  20만 마리의 NPC가 존재
#define WORLD_WIDTH	2000
#define WORLD_HEIGHT	2000
#else
constexpr int MAX_USER = 7000;
constexpr int NPC_ID_START = 5000;
#define WORLD_WIDTH		400
#define WORLD_HEIGHT	400
#endif


	
#define VIEW_RADIUS		7

#define SERVER_PORT		3500

#define CS_LOGIN		1		// 클라이언트가 서버에 접속 요청
#define CS_MOVE		2		// 클라이언트가 아바타기 이동을 서버에 요청
#define CS_ATTACK	3		// 아바타가 주위의 몬스터를 공격
#define CS_CHAT		4		// 아바타가 채팅
#define CS_LOGOUT	5		// 클라이언트 종료
#define CS_TELEPORT	6		// 랜덤 텔레포트 요청, 동접테스트시 아바타를 맵에 골고루 배치시키기 위함, 이것이 없으면 시작 위치가 HOTSPOT이 됨

#define SC_LOGIN_OK		1	// CS_LOGIN의 응답 패킷, 서버에서 클라이언트의 접속을 수락
#define SC_LOGIN_FAIL		2	// CS_LOGIN의 응답 패킷, 서버에서 클라이언트의 접속을 거절
#define SC_POSITION		3	// OBJECT의 위치 변경을 클라이언트에 통보
#define SC_CHAT			4	// OBJECT의 채팅을 통보
#define SC_STAT_CHANGE		5	// OBJECT의 정보가 변경되었음을 통보
#define SC_REMOVE_OBJECT		6	// OBJECT가 시야에서 사라 졌음
#define SC_ADD_OBJECT		7	// 새로운 OBJECT가 시야에 들어 왔음

#pragma pack(push ,1)

struct sc_packet_login_ok {
	unsigned char size;
	char type;
	int id;
	short	x, y;
	int	HP, LEVEL, EXP;
};

struct sc_packet_login_fail {
	unsigned char size;
	char type;
};

struct sc_packet_position {
	unsigned char size;
	char type;
	int id;
	short x, y;
	int move_time;			// Stress Test 프로그램에서 delay를 측정할 때 사용, 
					// 서버는 해당 id가 접속한 클라이언트에서 보내온 최신 값을 return 해야 한다.
};

struct sc_packet_chat {
	unsigned char size;
	char	type;
	int	id;
	char	message[MAX_STR_LEN];
};

struct sc_packet_stat_change {
	unsigned char size;
	char	type;
	int	id;
	int	HP, LEVEL, EXP;
};


struct sc_packet_remove_object {
	unsigned char size;
	char type;
	int id;
};

struct sc_packet_add_object {
	unsigned char	size;
	char	type;
	int	id;
	int	obj_class;		// 1: PLAYER,    2:ORC,  3:Dragon, …..  비주얼을 결정하는 값, 정의하기 나름
	short	x, y;
	int	HP, LEVEL, EXP;
	char	name[MAX_ID_LEN];

};


struct cs_packet_login {
	unsigned char	size;
	char	type;
	char       player_id[MAX_ID_LEN ];
};

struct cs_packet_move {
	unsigned char	size;
	char	type;
	char	direction;		// 0:Up, 1:Down, 2:Left, 3:Right
	int move_time;
};

struct cs_packet_attack {
	unsigned char	size;
	char	type;
};

struct cs_packet_chat {
	unsigned char	size;
	char	type;
	char 	message[MAX_STR_LEN];
};

struct cs_packet_logout {
	unsigned char	size;
	char	type;
};

struct cs_packet_teleport {
	unsigned char	size;
	char	type;
};
#pragma pack (pop)

#define S2C_LOGIN_OK SC_LOGIN_OK
#define S2C_MOVE_PLAYER SC_POSITION
#define S2C_ADD_PLAYER SC_ADD_OBJECT
#define S2C_REMOVE_PLAYER SC_REMOVE_OBJECT
#define S2C_CHAT SC_CHAT
#define C2S_MOVE CS_MOVE
#define C2S_LOGIN CS_LOGIN
typedef  sc_packet_login_ok s2c_login_ok;
typedef  sc_packet_add_object s2c_add_player;
typedef  sc_packet_position s2c_move_player;
typedef  sc_packet_remove_object s2c_remove_player;
typedef  sc_packet_chat s2c_chat;
typedef  cs_packet_move c2s_move;
typedef  cs_packet_login c2s_login;


#define MAX_BUFFER 256
#define MAX_PLAYER (NPC_ID_START)
#define MAX_NPC (MAX_USER-NPC_ID_START)
#define MESSAGE_MAX_BUFFER MAX_STR_LEN
#define MAX_NAME MAX_ID_LEN
enum DIRECTION { D_N, D_S, D_W, D_E, D_NO };