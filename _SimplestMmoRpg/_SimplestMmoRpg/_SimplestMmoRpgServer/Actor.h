#pragma once
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <vector>
#include <array>

#include "protocol.h"
struct MiniOver;

class Actor {
protected:
	int		id;
	char name[MAX_NAME];
	short	x, y, initX, initY;
	int		moveTime;
	
	/// <summary>
	/// selectedSectorLock���� ����ְ� ���
	/// </summary>
	std::vector<int> selectedSector;
	std::mutex   selectedSectorLock;

	/// <summary>
	/// oldNewViewListLock���� ����ְ� ���
	/// </summary>
	std::vector<int> oldViewList;
	/// <summary>
	/// oldNewViewListLock���� ����ְ� ���
	/// </summary>
	std::vector<int> newViewList;
	std::mutex oldNewViewListLock;

	/// <summary>
	/// viewSetLock���� ����ְ� ���
	/// </summary>
	std::unordered_set<int> viewSet;
	std::mutex   viewSetLock;
	
	static std::array <Actor*, MAX_USER> actors;

	Actor(int id);

public:
	std::atomic_bool isActive;
	
	virtual void Init();

	void InitSector() const;

	void RemoveFromSector() const;

	virtual void Update(int threadIdx);

	virtual void Move(DIRECTION dir, int threadIdx);

	virtual void Interact(Actor* interactor) {}

	virtual void OnNearActorWithPlayerMove(int actorId, int threadIdx) {}

	virtual void AddToViewSet(int otherId, int threadIdx);

	virtual void RemoveFromViewSet(int otherId, int threadIdx); // TODO ���߿� RemoveAll ������ lock���� ���� �������°� �پ��

	virtual void Die(int threadIdx);

	virtual void RemoveFromAll(int threadIdx);

	virtual void SendStatChange(int threadIdx);

	/// <summary>
	/// �ش� �������� �� ĭ �̵��մϴ�.
	/// </summary>
	void MoveTo(int targetX, int targetY, int threadIdx);
	
	/// <summary>
	/// ���� �΋H���� �����鼭 �ش� �������� �� ĭ �̵��մϴ�.
	/// </summary>
	void MoveToConsiderWall(int targetX, int targetY, int threadIdx);

	void RandomMove(int threadIdx);

	virtual void SetPos(int x, int y, int threadIdx) {
		this->x = x;
		this->y = y;
	}

	virtual bool IsMovableTile(int x, int y);

	/// <summary>
	/// ������ false ����
	/// </summary>
	/// <param name="attackerId"></param>
	/// <returns></returns>
	virtual bool TakeDamage(int attackerId, int threadIdx);

	bool CanSee(int otherId) const;

	bool CanSee(Actor* other) const;

	virtual void LuaLock();
	virtual void LuaUnLock();

	bool IsMonster() const;

	bool IsNpc() const;

	virtual void SetExp(int exp, int threadIdx);
	virtual void SetLevel(int level);
	virtual void SetHp(int hp);
	void SetMoveTime(int moveTime);
	int GetX() const;
	int GetY() const;
	int GetMoveTime() const;
	std::string GetName() const;
	virtual int GetHp();
	virtual int GetLevel();
	virtual int GetExp();
	virtual int GetDamage();
	std::vector<int>& GetSelectedSector();
	virtual MiniOver* GetOver(int threadIdx);

	static Actor* Get(int id);
protected:
	std::vector<int>& CopyViewSetToOldViewList();
};

inline void Actor::SetMoveTime(int moveTime) { this->moveTime = moveTime; }

inline bool Actor::TakeDamage(int attackerId, int threadIdx) {
	return true;
}

inline void Actor::AddToViewSet(int otherId, int threadIdx) {
	std::lock_guard<std::mutex> lock(viewSetLock);
	viewSet.insert(otherId);
}

