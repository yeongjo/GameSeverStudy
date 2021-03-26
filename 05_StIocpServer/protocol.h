#pragma once
constexpr unsigned char C2S_LOGIN = 1;
constexpr unsigned char C2S_MOVE = 2;
constexpr unsigned char S2C_LOGIN_OK = 3;
constexpr unsigned char S2C_ADD_PLAYER = 4;
constexpr unsigned char S2C_MOVE_PLAYER = 5;
constexpr unsigned char S2C_REMOVE_PLAYER = 6;
constexpr int MAX_NAME = 100;

#pragma pack(push, 1)

struct c2s_login
{
public:
	unsigned char size;
	unsigned char type;
	char name[MAX_NAME];
};

enum DIRECTION {
	D_N, D_S, D_W, D_E,
};

struct c2s_move
{
public:
	unsigned char size;
	unsigned char type;
	DIRECTION direction; // 0: North, 1: South, 2:
};

#pragma pack(pop)
