#pragma once
#include "Actor.h"
#include "lua.h"
#include "TimerQueueManager.h"

class NonPlayer : public Actor {
	MiniOver miniOver; // Npc ID�� PostQueuedCompletionStatus ȣ���Ҷ� ���
public:
	static NonPlayer* Get(int idx);
protected:
	/// <summary>
	/// luaLock���� ����ְ� ���
	/// </summary>
	lua_State* L; // Npc�� ���
	std::mutex luaLock; // Npc�� ���
	
	NonPlayer(int id) : Actor(id) {
	}

	void InitLua(const char* path);

	void Init() override;

	void OnNearActorWithPlayerMove(int actorId, int threadIdx) override;

	bool TakeDamage(int attackerId, int threadIdx) override;

	void AddToViewSet(int otherId, int threadIdx) override;

	void Die(int threadIdx) override;

	void WakeUpNpc(int threadIdx);

	void SleepNpc(int threadIdx);

	void LuaLock() override;

	void LuaUnLock() override;

	void SetLuaPos();
	void SetLuaPosWithoutLock() const;
	void SetPos(int x, int y, int threadIdx) override;

	MiniOver* GetOver(int threadIdx) override;

	int GetHpWithoutLock() const;

	int GetLevelWithoutLock() const;

	int GetExpWithoutLock() const;

	int GetHp() override;

	int GetLevel() override;

	int GetExp() override;

	int GetDamage() override;

	/// <summary>
	/// ���� ��ó�� �ִ� �÷��̾ ��ȯ�մϴ�. ���н� -1��ȯ
	/// </summary>
	/// <returns></returns>
	int GetNearestPlayer();

	void SetExp(int exp, int threadIdx) override;

	void SetLevel(int level) override;

	void SetHp(int hp) override;

private:
	void SendStatChange(int threadIdx) override;

	void AddNpcLoopEvent(int threadIdx);
};
