#include "Monster.h"

#include "LuaUtil.h"
#include "PathFindHelper.h"
#pragma comment(lib, "lua54.lib")
Monster::Monster(int id): NonPlayer(id) {
	pathFindHelper = PathFindHelper::Get(WorldManager::Get());
}

Monster* Monster::Create(int id) {
	return new Monster(id);
}

Monster* Monster::Get(int id) {
	_ASSERT(MONSTER_ID_START <= id && id <= MONSTER_ID_END);
	return reinterpret_cast<Monster*>(Actor::Get(id));
}

void Monster::Init() {
	sprintf_s(name, "M%d", id);
	x = rand() % WORLD_WIDTH;
	y = rand() % WORLD_HEIGHT;
	WorldManager::FileMonster monster;
	monster.x = x;
	monster.y = y;
	monster.hp = GetHpWithoutLock();
	monster.level = GetLevelWithoutLock();
	monster.exp = GetExpWithoutLock();
	monster.damage = GetDamage();
	monster.name = name;
	monster.script = "Monster.lua";
	monster.findPlayerAct = WorldManager::EFindPlayerAct::Peace;
	monster.soloMove = WorldManager::ESoloMove::Fixing;
	Init(monster);
}

void Monster::SetCanFindWay(bool canFindWay) {
	std::lock_guard<std::mutex> lock(luaLock);
	lua_pushboolean(L, canFindWay);
	lua_setglobal(L, "mCanFindWay");
}

void Monster::Init(WorldManager::FileMonster& monster) {
	strcpy_s(this->name, monster.name.c_str());
	this->x = monster.x;
	this->y = monster.y;
	NonPlayer::Init();
	InitLua(monster.script.c_str());
	SetHp(monster.hp);
	SetLevel(monster.level);
	SetExp(monster.exp, 0); // 플레이어 관련된거만 스레드 인덱스 사용
	SetDamage(monster.damage);
	SetCanFindWay(monster.canFindWay);

	lua_pushnumber(L, static_cast<int>(monster.findPlayerAct));
	lua_setglobal(L, "mFindPlayerAct");
	lua_pushnumber(L, static_cast<int>(monster.soloMove));
	lua_setglobal(L, "mSoloMove");
	lua_getglobal(L, "InitStat");
	CallLuaFunction(L, 0, 0);
}

void Monster::SetPathStartAndTarget(int startX, int startY, int targetX, int targetY) const {
	pathFindHelper->SetStartAndTarget(startX, startY, targetX, targetY);
}

inline void Monster::SetDamage(int damage) {
	std::lock_guard<std::mutex> lock(luaLock);
	lua_pushnumber(L, damage);
	lua_setglobal(L, "mDamage");
}

void Monster::Update(int threadIdx) {
	std::lock_guard<std::mutex> lock(monsterLock);
	{
		std::lock_guard<std::mutex> lockLua(luaLock);
		lua_getglobal(L, "Tick");
		{
			std::lock_guard<std::mutex> lockViewSet(viewSetLock);
			if (viewSet.empty()) {
				SleepNpc();
				lua_pop(L, 1);
				return;
			}
		}
		CallLuaFunction(L, 0, 0);
	}

	PathFindHelper::FindStatus findWay = PathFindHelper::FindStatus::CantFindWay;
	for (size_t i = 0; i < VIEW_RADIUS; i++) {
		//for (size_t i = 0; i < 100; i++) {
		findWay = pathFindHelper->FindWayOnce();
		if (PathFindHelper::FindStatus::Finding < findWay) { // 찾거나 찾을수 없었음
			break;
		}
	}
	if (findWay == PathFindHelper::FindStatus::FoundWay ||
		findWay == PathFindHelper::FindStatus::Finding) {
		int tx = x;
		int ty = y;
		pathFindHelper->GetNextPos(tx, ty);
		MoveTo(tx, ty, threadIdx);
	}
}