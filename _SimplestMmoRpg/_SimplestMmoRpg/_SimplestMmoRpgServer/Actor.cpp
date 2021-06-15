#include "Actor.h"

#include "Math.h"
#include "Player.h"
#include "Sector.h"
#include "TimerQueueManager.h"
#include "WorldManager.h"

std::array <Actor*, MAX_USER + 1> Actor::actors;

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

void Actor::Update() {
}

void Actor::Move(DIRECTION dir) {
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
		SetPos(x, y);
	}
}


void Actor::RemoveFromViewSet(int otherId) {
	viewSetLock.lock();
	viewSet.erase(otherId);
	viewSetLock.unlock();
}

void Actor::Die() {
	RemoveFromAll();
}

void Actor::RemoveFromAll() {
	viewSetLock.lock();
	for (auto viewId : viewSet) {
		auto actor = Get(viewId);
		if (!actor->IsNpc()) {
			Player::Get(viewId)->SendRemoveActor(id);
		}
		actor->RemoveFromViewSet(id);
	}
	viewSet.clear();
	viewSetLock.unlock();
	RemoveFromSector();
	TimerQueueManager::RemoveAll(id);
}

void Actor::SendStatChange() {
	{
		std::lock_guard<std::mutex> lock(viewSetLock);
		for (auto viewId : viewSet) {
			_ASSERT(viewId != id);
			const auto actor = Get(viewId);
			if (actor->IsNpc()) {
				continue;
			}
			Player::Get(viewId)->SendChangedStat(id, GetHp(), GetLevel(), GetExp());
		}
	}
	if (GetHp() == 0) {
		TimerQueueManager::Add(id, 1, nullptr, [this](int) {
			Die();
			});
	}
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
	return MONSTER_ID_START < id;
}

bool Actor::IsNpc() const {
	return NPC_ID_START < id;
}

void Actor::SetExp(int exp) {
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

MiniOver* Actor::GetOver() { return nullptr; }

std::atomic_bool& Actor::IsActive() { return isActive; }

Actor* Actor::Get(int id) {
	return actors[id];
}

std::vector<int>& Actor::CopyViewSetToOldViewList() {
	std::lock_guard<std::mutex> lock2(viewSetLock);
	oldViewList.resize(viewSet.size());
	std::copy(viewSet.begin(), viewSet.end(), oldViewList.begin());
	return oldViewList;
}
