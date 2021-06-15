#pragma once
#include "Actor.h"
#include "lua.h"
#include "TimerQueueManager.h"

class NonPlayer : public Actor {
	MiniOver miniOver; // Npc ID�� PostQueuedCompletionStatus ȣ���Ҷ� ���
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

	void OnNearActorWithPlayerMove(int actorId) override;

	bool TakeDamage(int attackerId) override;

	void AddToViewSet(int otherId) override;

	void Die() override;

	void WakeUpNpc();

	void SleepNpc();

	void LuaLock() override;

	void LuaUnLock() override;

	void SetPos(int x, int y) override;

	MiniOver* GetOver() override;

	int GetHpWithoutLock() const;

	int GetLevelWithoutLock() const;

	int GetExpWithoutLock() const;

	int GetHp() override;

	int GetLevel() override;

	int GetExp() override;

	int GetDamage() override;

	void SetExp(int exp) override;

	void SetLevel(int level) override;

	void SetHp(int hp) override;

private:
	void SendStatChange() override;

	void OnNearActorWithSelfMove(int actorId) override;

	void AddNpcLoopEvent();
};
