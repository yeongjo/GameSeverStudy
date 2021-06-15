#include "Actor.h"

#include "Math.h"
#include "Player.h"
#include "Sector.h"
#include "TimerQueueManager.h"
#include "WorldManager.h"

std::array <Actor*, MAX_USER> Actor::actors;

Actor::Actor(int id) {
	this->id = id;
	actors[id] = this;
}

void Actor::Init() {
	initX = x;
	initY = y;
	isActive = false;
}

void Actor::InitSector() const {
	Sector::GetSector(x, y)->Add(id);
}

void Actor::RemoveFromSector() const {
	Sector::GetSector(x, y)->Remove(id);
}

void Actor::Update(int threadIdx) {
}

void Actor::Move(DIRECTION dir, int threadIdx) {
	auto x = this->x;
	auto y = this->y;
	switch (dir){
	case D_E: if (x < WORLD_WIDTH - 1) ++x;
		break;
	case D_W: if (x > 0) --x;
		break;
	case D_S: if (y < WORLD_HEIGHT - 1) ++y;
		break;
	case D_N: if (y > 0) --y;
		break;
	}
	if (IsMovableTile(x, y)){
		SetPos(x, y, threadIdx);
	}
}


void Actor::RemoveFromViewSet(int otherId, int threadIdx) {
	viewSetLock.lock();
	viewSet.erase(otherId);
	viewSetLock.unlock();
}

void Actor::Die(int threadIdx) {
	RemoveFromAll(threadIdx);
}

void Actor::RemoveFromAll(int threadIdx) {
	viewSetLock.lock();
	for (auto viewId : viewSet) {
		auto actor = Get(viewId);
		if (!actor->IsNpc()) {
			Player::Get(viewId)->SendRemoveActor(id, threadIdx);
		}
		actor->RemoveFromViewSet(id, threadIdx);
	}
	viewSet.clear();
	viewSetLock.unlock();
	RemoveFromSector();
	TimerQueueManager::RemoveAll(id);
}

void Actor::SendStatChange(int threadIdx) {
	{
		std::lock_guard<std::mutex> lock(viewSetLock);
		for (auto viewId : viewSet) {
			_ASSERT(viewId != id);
			const auto actor = Get(viewId);
			if (actor->IsNpc()) {
				continue;
			}
			Player::Get(viewId)->SendChangedStat(id, GetHp(), GetLevel(), GetExp(), threadIdx);
		}
	}
	if (GetHp() == 0) {
		TimerQueueManager::Post(id, threadIdx, [this](int, int threadIdx) {
			Die(threadIdx);
			});
	}
}

void Actor::MoveTo(int targetX, int targetY, int threadIdx) {
	int tx = x, ty = y;
	auto offX = targetX - tx;
	auto offY = targetY - ty;
	if (abs(offX) > abs(offY)) {
		offX > 0 ? ++tx : offX < 0 ? --tx : tx;
	} else {
		offY > 0 ? ++ty : offY < 0 ? --ty : ty;
	}
	SetPos(tx, ty, threadIdx);
}

void Actor::MoveToConsiderWall(int targetX, int targetY, int threadIdx) {
	int tx = x, ty = y;
	auto offX = targetX - tx;
	auto offY = targetY - ty;
	if (abs(offX) > abs(offY)) {
		offX > 0 ? ++tx : offX < 0 ? --tx : tx;
	} else {
		offY > 0 ? ++ty : offY < 0 ? --ty : ty;
	}
	if(IsMovableTile(tx, ty)){
		SetPos(tx, ty, threadIdx);
	}
}

void Actor::RandomMove(int threadIdx) {
	Move(static_cast<DIRECTION>(rand() % 4), threadIdx);
}

bool Actor::IsMovableTile(int x, int y) {
	return !WorldManager::Get()->GetCollidable(x, y);
}

bool Actor::CanSee(int otherId) const {
	return CanSee(Actor::Get(otherId));
}

bool Actor::CanSee(Actor* other) const {
	int ax = GetX();
	int ay = GetY();
	int bx = other->GetX();
	int by = other->GetY();
	return HALF_VIEW_RADIUS >=
		Distance(ax, ay, bx, by);
}

void Actor::LuaLock() {
}

void Actor::LuaUnLock() {
}

bool Actor::IsMonster() const {
	return MONSTER_ID_START <= id;
}

bool Actor::IsNpc() const {
	return NPC_ID_START <= id;
}

void Actor::SetExp(int exp, int threadIdx) {
}

void Actor::SetLevel(int level) {
}

void Actor::SetHp(int hp) {
}

int Actor::GetX() const { return x; }

int Actor::GetY() const { return y; }

int Actor::GetMoveTime() const { return moveTime; }

std::string Actor::GetName() const { return name; }

int Actor::GetHp() { return -1; }

int Actor::GetLevel() { return -1; }

int Actor::GetExp() { return -1; }

int Actor::GetDamage() { return -1; }

std::vector<int>& Actor::GetSelectedSector() {
	auto& result = selectedSector;
	std::lock_guard<std::mutex> lock(selectedSectorLock);
	result.clear();
	return result;
}

MiniOver* Actor::GetOver(int threadIdx) { return nullptr; }

Actor* Actor::Get(int id) {
	return actors[id];
}

std::vector<int>& Actor::CopyViewSetToOldViewList() {
	std::lock_guard<std::mutex> lock2(viewSetLock);
	oldViewList.resize(viewSet.size());
	std::copy(viewSet.begin(), viewSet.end(), oldViewList.begin());
	return oldViewList;
}
