#pragma once
#include "Actor.h"
#include "lua.h"
#include "TimerQueueManager.h"

class NonPlayer : public Actor {
	MiniOver miniOver; // Npc ID로 PostQueuedCompletionStatus 호출할때 사용
public:
	static NonPlayer* Get(int idx);
protected:
	/// <summary>
	/// luaLock으로 잠궈주고 사용
	/// </summary>
	lua_State* L; // Npc만 사용
	std::mutex luaLock; // Npc만 사용
	bool isDead = false;

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

	void SetLuaPos();
	void SetLuaPosWithoutLock() const;
	void SetPos(int x, int y) override;

	MiniOver* GetOver() override;

	int GetHpWithoutLock() const;

	int GetLevelWithoutLock() const;

	int GetExpWithoutLock() const;

	int GetHp() override;

	int GetLevel() override;

	int GetExp() override;

	int GetDamage() override;

	/// <summary>
	/// 가장 근처에 있는 플레이어를 반환합니다. 실패시 -1반환
	/// </summary>
	/// <returns></returns>
	int GetNearestPlayer();

	void SetExp(int exp) override;

	void SetLevel(int level) override;

	void SetHp(int hp) override;

private:
	void SendStatChange() override;

	void AddNpcLoopEvent();
};
