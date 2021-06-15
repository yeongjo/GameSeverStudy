#pragma once
#include "Actor.h"
#include "Session.h"

class Player : public Actor {
protected:
	std::wstring wname;
	int hp, maxHp = 5;
	int level;
	int exp;
	int damage;
	std::vector<int> attackViewList;
	Session session;	// 플레이어와 같은 세션 접속 유저만 사용

	Player(int id);

public:
	static Player* Create(int id);

	static Player* Get(int id);

	static int GetNewId(SOCKET socket);

	void Init() override;

	void SetPos(int x, int y, int threadIdx) override;
	
	void SendStatChange(int threadIdx) override;

	virtual void Disconnect(int threadIdx);

	virtual void Attack(int threadIdx);

	bool TakeDamage(int attackerId, int threadIdx) override;

	void Die(int threadIdx) override;

	void AddToViewSet(int otherId, int threadIdx) override;

	void RemoveFromViewSet(int otherId, int threadIdx) override;
	
	void RemoveFromViewSetWithoutLock(int otherId, int threadIdx);

	void SendLoginOk(int threadIdx);

	void SendChat(int senderId, const char* mess, int threadIdx);

	void SendMove(int p_id, int threadIdx);

	void SendAddActor(int addedId, int threadIdx);

	void SendRemoveActor(int removeTargetId, int threadIdx);

	void SendChangedStat(int statChangedId, int hp, int level, int exp, int threadIdx);

	void CallRecv();

	int GetId() const { return id; }
	Session* GetSession() { return &session; }
	int GetHp() override { return hp; }
	int GetLevel() override { return level; }
	int GetExp() override { return exp; }
	int GetDamage() override { return damage; }
	void SetExp(int exp, int threadIdx) override;
	void SetLevel(int level) override;
	void SetHp(int hp) override;
	MiniOver* GetOver(int threadIdx) { return session.bufOverManager.Get(threadIdx); }

private:
	/// <summary>
	/// 한 스레드에서만 호출안되기때문에 lock 안걸어도됨
	/// </summary>
	/// <param name="id"></param>
	void ProcessPacket(unsigned char* buf, int threadIdx);

	void AddHealTimer(int threadIdx);
};

