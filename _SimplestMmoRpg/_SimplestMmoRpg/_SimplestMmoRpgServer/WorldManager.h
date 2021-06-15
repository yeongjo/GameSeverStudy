#pragma once
#include <iosfwd>
#include <string>
#include <vector>

#include "protocol.h"

class Monster;

class WorldManager {
public:
	enum class EFindPlayerAct { Peace, Agro, COUNT };
	enum class ESoloMove { Fixing, Roaming, COUNT };
	struct FileMonster {
		int x, y, hp, level, exp, damage;
		std::string name;
		std::string script;
		EFindPlayerAct findPlayerAct;
		ESoloMove soloMove;
	};
private:
	std::vector<FileMonster> monsters;
	std::vector<ETile> world;
	int width, height;
	static WorldManager* self;

	WorldManager(){}
public:
	void Load();

	void Generate();

	Monster* GetMonster(int id);
	bool GetCollidable(int x, int y) {
		if (x < 0 || width <= x || y < 0 || height <= y) {
			return true;
		}
		auto tile = world[y * width + x];
		return tile == Wall;
	}
	bool GetCollidable(int index) {
		auto tile = world[index];
		return tile == Wall;
	}
	int GetWorldIndex(int x, int y) const {
		return y * width + x;
	}
	void GetPosFormWorldIndex(int index, int& x, int& y) const {
		y = index / width;
		x = index - (y * width);
	}

	std::string GetPosString(int index);
	int GetWidth() const { return width; }
	int GetHeight() const { return height; }
	static WorldManager* Get() {
		if(self == nullptr){
			self = new WorldManager;
		}
		return self;
	}
};
