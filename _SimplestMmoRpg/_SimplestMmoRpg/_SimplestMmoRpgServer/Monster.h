#pragma once
#include "NonPlayer.h"
#include "WorldManager.h"

class PathFindHelper;

class Monster : public NonPlayer {
protected:
	PathFindHelper* pathFindHelper = nullptr; // TODO 모든 NPC가 길을 찾는게 아니기에 풀러에서 가져와서 쓰면서 메모리 절약가능
	std::mutex monsterLock;

	Monster(int id);
public:
	static Monster* Create(int id);

	static Monster* Get(int id);

	void Init() override;

	void Init(WorldManager::FileMonster& monster);

	void SetPathStartAndTarget(int startX, int startY, int targetX, int targetY) const;

	void Update(int threadIdx) override;

	void SetDamage(int damage);

	void Die(int threadIdx) override {
		NonPlayer::Die(threadIdx);
	}
private:
	void SetCanFindWay(bool canFindWay);
};


