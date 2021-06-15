#include "NonPlayer.h"

#include "LuaUtil.h"
#include "Monster.h"
#include "Player.h"
#include "Sector.h"
//#pragma comment(lib, "lua54.lib")

int LuaAddEventSendMess(lua_State* L) {
	int recverId = lua_tointeger(L, -5);
	int senderId = lua_tointeger(L, -4);
	const char* mess = lua_tostring(L, -3);
	int delay = lua_tointeger(L, -2);
	int threadIdx = lua_tointeger(L, -1);
	lua_pop(L, 5);
	TimerQueueManager::Add(recverId, delay, threadIdx, nullptr, [=](int size, int threadIdx) {
		Player::Get(recverId)->SendChat(senderId, mess, threadIdx);
		});
	return 1;
}

int LuaTakeDamage(lua_State* L) {
	int obj_id = lua_tointeger(L, -3);
	int attackerId = lua_tointeger(L, -2);
	int threadIdx = lua_tointeger(L, -1);
	lua_pop(L, 3);
	Actor::Get(obj_id)->TakeDamage(attackerId, threadIdx);
	return 1;
}

int LuaGetX(lua_State* L) {
	int id = lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = Actor::Get(id)->GetX();
	lua_pushinteger(L, x);
	return 1;
}
int LuaGetY(lua_State* L) {
	int id = lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = Actor::Get(id)->GetY();
	lua_pushinteger(L, y);
	return 1;
}

int LuaSetPos(lua_State* L) {
	int id = lua_tointeger(L, -4);
	int x = lua_tointeger(L, -3);
	int y = lua_tointeger(L, -2);
	int threadIdx = lua_tointeger(L, -1);
	lua_pop(L, 5);
	auto actor = Actor::Get(id);
	actor->LuaUnLock();
	actor->SetPos(x, y, threadIdx);
	actor->LuaLock();
	return 1;
}

int LuaSendMess(lua_State* L) {
	int recverId = lua_tointeger(L, -4);
	int senderId = lua_tointeger(L, -3);
	const char* mess = lua_tostring(L, -2);
	int threadIdx = lua_tointeger(L, -1);
	lua_pop(L, 4);
	Player::Get(recverId)->SendChat(senderId, mess, threadIdx);
	return 1;
}
int LauGetHp(lua_State* L) {
	int targetId = lua_tointeger(L, -1);
	lua_pop(L, 2);
	lua_pushinteger(L, Actor::Get(targetId)->GetHp());
	return 1;
}
int LuaGetLevel(lua_State* L) {
	int targetId = lua_tointeger(L, -1);
	lua_pop(L, 2);
	lua_pushinteger(L, Actor::Get(targetId)->GetLevel());
	return 1;
}
int LuaGetExp(lua_State* L) {
	int targetId = lua_tointeger(L, -1);
	lua_pop(L, 2);
	lua_pushinteger(L, Actor::Get(targetId)->GetExp());
	return 1;
}

int LuaSendStatChange(lua_State* L) {
	int targetId = lua_tointeger(L, -6);
	int orderId = lua_tointeger(L, -5);
	int hp = lua_tointeger(L, -4);
	int level = lua_tointeger(L, -3);
	int exp = lua_tointeger(L, -2);
	int threadIdx = lua_tointeger(L, -1);
	lua_pop(L, 7);
	auto actor = Actor::Get(targetId);
	if (orderId != targetId) { // 자기 자신건 변경 할 필요없다. 그리고 변경하려하면 락이 문제가 됨
		actor->SetHp(hp);
		actor->SetLevel(level);
		actor->SetExp(exp, threadIdx);
	}
	actor->SendStatChange(threadIdx);
	return 1;
}

int LuaPrint(lua_State* L) {
	const char* mess = lua_tostring(L, -1);
	lua_pop(L, 2);
	std::cout << mess;
	return 1;
}

//int LuaAddEventNpcRandomMove(lua_State* L) {
//	int p_id = lua_tointeger(L, -2);
//	int delay = lua_tointeger(L, -1);
//	lua_pop(L, 3);
//	TimerQueueManager::Add(p_id, delay, nullptr, [=](int size) {
//		Actor::Get(p_id)->Move(static_cast<DIRECTION>(rand() % 4));
//		});
//	return 1;
//}
int LuaIsMovable(lua_State* L) {
	int x = lua_tointeger(L, -2);
	int y = lua_tointeger(L, -1);
	lua_pop(L, 3);
	lua_pushboolean(L, !WorldManager::Get()->GetCollidable(x, y));
	return 1;
}

void NonPlayer::Init() {
	Actor::Init();
	moveTime = 0;
	memset(&miniOver.over, 0, sizeof(miniOver.over));
	InitSector();
}

void NonPlayer::OnNearActorWithPlayerMove(int actorId, int threadIdx) {
	std::lock_guard<std::mutex> lock(luaLock);
	SetLuaPosWithoutLock();
	lua_getglobal(L, "OnNearActorWithPlayerMove");
	lua_pushnumber(L, actorId);
	lua_pushnumber(L, threadIdx);
	CallLuaFunction(L, 2, 0);
}

bool NonPlayer::TakeDamage(int attackerId, int threadIdx) {
	std::lock_guard<std::mutex> lock(luaLock);
	lua_getglobal(L, "TakeDamage");
	lua_pushinteger(L, attackerId);
	lua_pushinteger(L, Actor::Get(attackerId)->GetDamage());
	lua_pushinteger(L, threadIdx);
	CallLuaFunction(L, 3, 1);
	auto result = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return result;
}

int NonPlayer::GetExp() {
	std::lock_guard<std::mutex> lock(luaLock);
	return GetExpWithoutLock();
}

int NonPlayer::GetDamage() {
	std::lock_guard<std::mutex> lock(luaLock);
	lua_getglobal(L, "mDamage");
	int value = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return value;
}

int NonPlayer::GetNearestPlayer() {
	std::lock_guard<std::mutex> lock(viewSetLock);
	int nearestId = -1;
	int minDistance = VIEW_RADIUS + 1000;
	for(auto viewId : viewSet){
		auto actor = Actor::Get(viewId);
		auto actorDistance = Distance(x, y, actor->GetX(), actor->GetY());
		if(actorDistance < minDistance){
			minDistance = actorDistance;
			nearestId = viewId;
		}
	}
	return nearestId;
}

void NonPlayer::SetExp(int exp, int threadIdx) {
	std::lock_guard<std::mutex> lock(luaLock);
	lua_pushnumber(L, exp);
	lua_setglobal(L, "mExp");
}

void NonPlayer::SetLevel(int level) {
	std::lock_guard<std::mutex> lock(luaLock);
	lua_pushnumber(L, level);
	lua_setglobal(L, "mLevel");
}

void NonPlayer::SetHp(int hp) {
	std::lock_guard<std::mutex> lock(luaLock);
	lua_pushnumber(L, hp);
	lua_setglobal(L, "mHp");
}

void NonPlayer::SendStatChange(int threadIdx) {
	{
		std::lock_guard<std::mutex> lock(viewSetLock);
		for (auto viewId : viewSet){
			_ASSERT(viewId != id);
			if (Actor::Get(viewId)->IsNpc()){
				continue;
			}
			Player::Get(viewId)->SendChangedStat(id, GetHpWithoutLock(), GetLevelWithoutLock(), GetExpWithoutLock(), threadIdx);
		}
	}
	if (GetHpWithoutLock() == 0){
		TimerQueueManager::Post(id, threadIdx, [this](int,int threadIdx2) {
			Die(threadIdx2);
		});
	}
}

void NonPlayer::AddNpcLoopEvent(int threadIdx) {
	TimerQueueManager::Add(id, 1000, threadIdx, nullptr, [&](int, int threadIdx2) {
		AddNpcLoopEvent(threadIdx2);
		Actor::Get(id)->Update(threadIdx2);
	});
}

NonPlayer* NonPlayer::Get(int idx) {
	_ASSERT(NPC_ID_START<= idx && idx <= MONSTER_ID_END);
	return static_cast<NonPlayer*>(Actor::Get(idx));
}

void NonPlayer::InitLua(const char* path) {
	L = luaL_newstate();
	luaL_openlibs(L);
	int luaError = luaL_loadfile(L, path);

	CallLuaFunction(L, 0, 0);

	// API함수들은 락걸린 LUA함수가 호출하기에 락이 필요없다.
	lua_register(L, "LuaSetPos", LuaSetPos);
	lua_register(L, "LuaGetX", LuaGetX);
	lua_register(L, "LuaGetY", LuaGetY);
	lua_register(L, "LuaSendMess", LuaSendMess);
	lua_register(L, "LuaSendStatChange", LuaSendStatChange);
	lua_register(L, "LauGetHp", LauGetHp);
	lua_register(L, "LuaGetLevel", LuaGetLevel);
	lua_register(L, "LuaGetExp", LuaGetExp);
	lua_register(L, "LuaTakeDamage", LuaTakeDamage);
	lua_register(L, "LuaPrint", LuaPrint);
	//lua_register(L, "LuaAddEventNpcRandomMove", LuaAddEventNpcRandomMove);
	lua_register(L, "LuaAddEventSendMess", LuaAddEventSendMess);
	lua_register(L, "LuaIsMovable", LuaIsMovable);
	lua_register(L, "LuaSetPathStartAndTarget", [](lua_State* L) {
		auto monsterId = lua_tointeger(L, -3);
		auto startX = Get(monsterId)->GetX();
		auto startY = Get(monsterId)->GetY();
		auto targetX = lua_tointeger(L, -2);
		auto targetY = lua_tointeger(L, -1);
		lua_pop(L, 4);
		// TODO Actor도 길찾기 할 수 있으니 수정해야함
		Monster::Get(monsterId)->SetPathStartAndTarget(startX, startY, targetX, targetY);
		return 1;
		});
	lua_register(L, "LuaMoveToConsiderWall", [](lua_State* L) {
		auto id = lua_tointeger(L, -4);
		auto targetX = lua_tointeger(L, -3);
		auto targetY = lua_tointeger(L, -2);
		auto threadIdx = lua_tointeger(L, -1);
		lua_pop(L, 4);
		auto actor = Actor::Get(id);
		actor->LuaUnLock();
		actor->MoveToConsiderWall(targetX, targetY, threadIdx);
		actor->LuaLock();
		return 1;
	});
	lua_register(L, "LuaRandomMove", [](lua_State* L) {
		auto id = lua_tointeger(L, -2);
		auto threadIdx = lua_tointeger(L, -1);
		lua_pop(L, 2);
		auto actor = Actor::Get(id);
		actor->LuaUnLock();
		actor->RandomMove(threadIdx);
		actor->LuaLock();
		return 1;
	});
	lua_register(L, "LuaGetNearPlayer", [](lua_State* L) {
		auto id = lua_tointeger(L, -1);
		lua_pop(L, 2);
		auto actor = NonPlayer::Get(id);
		lua_pushinteger(L, actor->GetNearestPlayer());
		return 1;
	});
	lua_register(L, "LuaGetPos", [](lua_State* L) {
		int id = lua_tointeger(L, -1);
		lua_pop(L, 2);
		int x = Actor::Get(id)->GetX();
		int y = Actor::Get(id)->GetY();
		lua_pushinteger(L, y);
		lua_pushinteger(L, x);
		return 1;
	});


	lua_getglobal(L, "SetId");
	lua_pushinteger(L, id);
	CallLuaFunction(L, 1, 0);

}

void NonPlayer::AddToViewSet(int otherId, int threadIdx) {
	Actor::AddToViewSet(otherId, threadIdx);
	WakeUpNpc(threadIdx);
}

void NonPlayer::Die(int threadIdx) {
	isDead = true;
	Actor::Die(threadIdx);
	SleepNpc();
}

void NonPlayer::WakeUpNpc(int threadIdx) {
	if(isDead){
		return;
	}
	if (isActive == false){
		bool old_state = false;
		if (true == std::atomic_compare_exchange_strong(&isActive, &old_state, true)){
			//std::cout << "wake up id: " << id << " is active: " << isActive << std::endl;
			AddNpcLoopEvent(threadIdx);
		}
	}
}

void NonPlayer::SleepNpc() {
	//std::cout << "SleepNpc id: " << id << " is active: " << isActive << std::endl;
	isActive = false;
	TimerQueueManager::RemoveAll(id);
}

void NonPlayer::LuaLock() {
	luaLock.lock();
}

void NonPlayer::LuaUnLock() {
	luaLock.unlock();
}

void NonPlayer::SetLuaPos() {
	std::lock_guard<std::mutex> lockLua(luaLock);
	SetLuaPosWithoutLock();
}

void NonPlayer::SetLuaPosWithoutLock() const {
	lua_pushinteger(L, this->x);
	lua_setglobal(L, "mX");
	lua_pushinteger(L, this->y);
	lua_setglobal(L, "mY");
}

void NonPlayer::SetPos(int x, int y, int threadIdx) {
	if (this->x == x && this->y == y) {
		return;
	}
	auto prevX = this->x;
	auto prevY = this->y;
	this->x = x;
	this->y = y;
	SetLuaPos();

	Sector::Move(id, prevX, prevY, x, y);

	std::lock_guard<std::mutex> lock(oldNewViewListLock);
	newViewList = Sector::GetIdFromOverlappedSector(id);
#ifdef NPCLOG
	lock_guard<mutex> coutLock{ coutMutex };
	cout << "npc[" << id << "] (" << x << "," << y << ") 이동 " << oldViewList.size() << "명[";
	for (auto tViewId : oldViewList) {
		cout << tViewId << ",";
	}
	cout << "] -> " << new_vl.size() << "명[";
	for (auto tViewId : new_vl) {
		cout << tViewId << ",";
	}
	cout << "]한테 보임";
#endif // NPCLOG

	CopyViewSetToOldViewList();
	if (newViewList.empty()) {
		SleepNpc(); // 아무도 보이지 않으므로 취침
#ifdef NPCLOG
		cout << " & 아무에게도 안보여서 취침" << endl;
#endif
		for (auto otherId : oldViewList) {
			Actor::Get(otherId)->RemoveFromViewSet(id, threadIdx);
		}
		return;
	}
	for (auto otherId : newViewList) {
		if (oldViewList.end() == std::find(oldViewList.begin(), oldViewList.end(), otherId)) {
			// 플레이어의 시야에 등장
			AddToViewSet(otherId, threadIdx);
			Player::Get(otherId)->AddToViewSet(id, threadIdx);
#ifdef NPCLOG
			cout << " &[" << otherId << "]에게 등장";
#endif
		} else {
			// 플레이어가 계속 보고있음.
#ifdef NPCLOG
			cout << " &[" << otherId << "]에게 위치 갱신";
#endif
			Player::Get(otherId)->SendMove(id, threadIdx);
		}
	}
	for (auto otherId : oldViewList) {
		if (newViewList.end() == std::find(newViewList.begin(), newViewList.end(), otherId)) {
			RemoveFromViewSet(otherId, threadIdx);
			Actor::Get(otherId)->RemoveFromViewSet(id, threadIdx);
#ifdef NPCLOG
			cout << " &[" << otherId << "]에게서 사라짐";
#endif
		}
	}
#ifdef NPCLOG
	cout << endl;
#endif
}

MiniOver* NonPlayer::GetOver(int threadIdx) {
	memset(&miniOver.over, 0, sizeof(miniOver.over));
	return &miniOver;
}

int NonPlayer::GetHpWithoutLock() const {
	lua_getglobal(L, "mHp");
	int value = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return value;
}

int NonPlayer::GetLevelWithoutLock() const {
	lua_getglobal(L, "mLevel");
	int value = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return value;
}

int NonPlayer::GetExpWithoutLock() const {
	lua_getglobal(L, "mExp");
	int value = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return value;
}

int NonPlayer::GetHp() {
	std::lock_guard<std::mutex> lock(luaLock);
	return GetHpWithoutLock();
}

int NonPlayer::GetLevel() {
	std::lock_guard<std::mutex> lock(luaLock);
	return GetLevelWithoutLock();
}
