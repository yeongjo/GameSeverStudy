#pragma once
#include "NonPlayer.h"

class Npc : public NonPlayer {
protected:
	Npc(int id) : NonPlayer(id) {
	}
public:
	static Npc* Create(int id) {
		return new Npc(id);
	}

	static Npc* Get(int id) {
		_ASSERT(NPC_ID_START <= id && id <= NPC_ID_END);
		return static_cast<Npc*>(actors[id]);
	}

	void Init() override;

	void Update() override;
};

