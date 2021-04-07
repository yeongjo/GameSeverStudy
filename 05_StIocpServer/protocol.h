#pragma once
constexpr unsigned char C2S_LOGIN = 1;
constexpr unsigned char C2S_MOVE = 2;
constexpr unsigned char S2C_LOGIN_OK = 3;
constexpr unsigned char S2C_ADD_PLAYER = 4;
constexpr unsigned char S2C_MOVE_PLAYER = 5;
constexpr unsigned char S2C_REMOVE_PLAYER = 6;
constexpr int MAX_NAME = 100;
constexpr int MAX_BUFFER = 1024;
constexpr short SERVER_PORT = 3500;
constexpr int WORLD_X_SIZE = 8;
constexpr int WORLD_Y_SIZE = 8;
constexpr auto MAX_USER = 10;

#pragma pack(push, 1)

struct c2s_login
{
public:
	unsigned char size;
	unsigned char type;
	char name[MAX_NAME];
};

enum DIRECTION {
	D_NO, D_N, D_S, D_W, D_E,
};

struct c2s_move
{
public:
	unsigned char size;
	unsigned char type;
	DIRECTION dr; // 0: North, 1: South, 2:
};

struct s2c_login_ok
{
	unsigned char size;
	unsigned char type;
	int	id;
	short	x, y;
	int hp, level;
	int race;
};

struct s2c_add_player {
	unsigned char size;
	unsigned char type;
	int	id;
	short	x, y;
	int race;
};

struct s2c_move_player
{
	unsigned char size;
	unsigned char type;
	int	id;
	short	x, y;
};

struct s2c_remove_player {
	unsigned char size;
	unsigned char type;
	int	id;
};

#pragma pack(pop)