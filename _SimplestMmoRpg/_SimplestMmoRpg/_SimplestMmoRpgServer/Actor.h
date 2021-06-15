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

	std::atomic_bool isActive;

	/// <summary>
	/// selectedSectorLock으로 잠궈주고 사용
	/// </summary>
	std::vector<int> selectedSector;
	std::mutex   selectedSectorLock;

	/// <summary>
	/// oldNewViewListLock으로 잠궈주고 사용
	/// </summary>
	std::vector<int> oldViewList;
	/// <summary>
	/// oldNewViewListLock으로 잠궈주고 사용
	/// </summary>
	std::vector<int> newViewList;
	std::mutex oldNewViewListLock;

	/// <summary>
	/// viewSetLock으로 잠궈주고 사용
	/// </summary>
	std::unordered_set<int> viewSet;
	std::mutex   viewSetLock;
	
	static std::array <Actor*, MAX_USER + 1> actors;

	Actor(int id);

public:
	virtual void Init();

	void InitSector() const;

	void RemoveFromSector() const;

	virtual void Update();

	virtual void Move(DIRECTION dir);

	virtual void Interact(Actor* interactor) {}

	virtual void OnNearActorWithPlayerMove(int actorId) {}

	virtual void OnNearActorWithSelfMove(int actorId) {}

	virtual void AddToViewSet(int otherId);

	virtual void RemoveFromViewSet(int otherId); // TODO 나중에 RemoveAll 만들어야 lock으로 생긴 느려지는거 줄어듬

	virtual void Die();

	virtual void RemoveFromAll();

	virtual void SendStatChange();

	virtual void SetPos(int x, int y) {
		this->x = x;
		this->y = y;
	}

	virtual bool IsMovableTile(int x, int y);

	/// <summary>
	/// 죽으면 false 리턴
	/// </summary>
	/// <param name="attackerId"></param>
	/// <returns></returns>
	virtual bool TakeDamage(int attackerId);

	bool CanSee(int otherId) const;

	bool CanSee(Actor* other) const;

	virtual void LuaLock();
	virtual void LuaUnLock();

	bool IsMonster() const;

	bool IsNpc() const;

	virtual void SetExp(int exp);
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
	virtual MiniOver* GetOver();
	std::atomic_bool& IsActive();

	static Actor* Get(int id);
protected:
	std::vector<int>& CopyViewSetToOldViewList();
};

inline void Actor::SetMoveTime(int moveTime) { this->moveTime = moveTime; }

inline bool Actor::TakeDamage(int attackerId) {
	return true;
}

inline void Actor::AddToViewSet(int otherId) {
	std::lock_guard<std::mutex> lock(viewSetLock);
	viewSet.insert(otherId);
}

