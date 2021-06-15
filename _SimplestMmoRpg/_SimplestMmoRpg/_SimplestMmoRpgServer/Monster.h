#pragma once
#include "NonPlayer.h"
#include "WorldManager.h"

class PathFindHelper;

class Monster : public NonPlayer {
protected:
	PathFindHelper* pathFindHelper = nullptr; // TODO ��� NPC�� ���� ã�°� �ƴϱ⿡ Ǯ������ �����ͼ� ���鼭 �޸� ���డ��
	std::mutex monsterLock;

	Monster(int id);
public:
	static Monster* Create(int id);

	static Monster* Get(int id);

	void Init() override;

	void Init(WorldManager::FileMonster& monster);

	void SetPathStartAndTarget(int startX, int startY, int targetX, int targetY) const;

	void Update() override;

	void SetDamage(int damage);

	void Die() override {
		NonPlayer::Die();
	}
private:
	void SetCanFindWay(bool canFindWay);
};


