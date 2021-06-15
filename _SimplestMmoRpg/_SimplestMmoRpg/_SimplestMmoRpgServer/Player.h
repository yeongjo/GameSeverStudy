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
	Session session;	// �÷��̾�� ���� ���� ���� ������ ���

	Player(int id);

public:
	static Player* Create(int id);

	static Player* Get(int id);

	static int GetNewId(SOCKET socket);

	void Init() override;

	void SetPos(int x, int y) override;
	
	void SendStatChange() override;

	virtual void Disconnect();

	virtual void Attack();

	bool TakeDamage(int attackerId) override;

	void Die() override;

	void AddToViewSet(int otherId) override;

	void RemoveFromViewSet(int otherId) override;

	void SendLoginOk();

	void SendChat(int senderId, const char* mess);

	void SendMove(int p_id);

	void SendAddActor(int addedId);

	void SendRemoveActor(int removeTargetId);

	void SendChangedStat(int statChangedId, int hp, int level, int exp);

	void CallRecv();

	int GetId() const { return id; }
	MiniOver* GetOver() override;
	Session* GetSession() { return &session; }
	int GetHp() override { return hp; }
	int GetLevel() override { return level; }
	int GetExp() override { return exp; }
	int GetDamage() override { return damage; }
	void SetExp(int exp) override;
	void SetLevel(int level) override;
	void SetHp(int hp) override;

private:
	/// <summary>
	/// �� �����忡���� ȣ��ȵǱ⶧���� lock �Ȱɾ��
	/// </summary>
	/// <param name="id"></param>
	void ProcessPacket(unsigned char* buf);

	void AddHealTimer();
};

