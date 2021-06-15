#include "Monster.h"

#include "LuaUtil.h"
#include "PathFindHelper.h"

Monster::Monster(int id): NonPlayer(id) {
	pathFindHelper = PathFindHelper::Get(WorldManager::Get());
}

Monster* Monster::Create(int id) {
	return new Monster(id);
}

Monster* Monster::Get(int id) {
	_ASSERT(MONSTER_ID_START <= id && id < MONSTER_ID_START + MAX_MONSTER);
	return reinterpret_cast<Monster*>(Get(id));
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

void Monster::Init(WorldManager::FileMonster& monster) {
	strcpy_s(this->name, monster.name.c_str());
	this->x = monster.x;
	this->y = monster.y;
	NonPlayer::Init();
	InitLua(monster.script.c_str());
	SetHp(monster.hp);
	SetLevel(monster.level);
	SetExp(monster.exp);
	SetDamage(monster.damage);

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

inline void Monster::MoveTo(int targetX, int targetY) {
	int tx = x, ty = y;
	auto offX = targetX - tx;
	auto offY = targetY - ty;
	if (abs(offX) > abs(offY)) {
		offX > 0 ? ++tx : offX < 0 ? --tx : tx;
	} else {
		offY > 0 ? ++ty : offY < 0 ? --ty : ty;
	}
	SetPos(tx, ty);
}

void Monster::Update() {
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
			asTable(L, viewSet.begin(), viewSet.end());
		}
		CallLuaFunction(L, 1, 0);
	}

	PathFindHelper::FindStatus findWay;
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
		MoveTo(tx, ty);
	}
}