#include "Npc.h"

void Npc::Init() {
	sprintf_s(name, "N%d", id);
	x = rand() % WORLD_WIDTH;
	y = rand() % WORLD_HEIGHT;
	NonPlayer::Init();
	InitLua("npc.lua");
}

void Npc::Update() {
	Move(static_cast<DIRECTION>(rand() % 4));
}