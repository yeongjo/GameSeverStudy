#include "WorldManager.h"

#include <sstream>

#include "Image.h"
#include "Monster.h"

WorldManager* WorldManager::self = nullptr;

void WorldManager::Load() {
	Image image;
	image.ReadBmp(MAP_PATH);
	world.resize(image.height * image.width);
	width = image.width;
	height = image.height;
	for (size_t y = 0; y < image.height; y++) {
		for (size_t x = 0; x < image.width; x++) {
			world[y * image.width + x] = ETile::Empty;
			switch (image.GetPixel(x, y).r) {
			case 255: {
				world[y * image.width + x] = ETile::Wall;
				break;
			}
			}
		}
	}
}

void WorldManager::Generate() {
	monsters.resize(MAX_MONSTER);
	for (size_t i = 0; i < monsters.size(); i++) {
		monsters[i].x = rand() % WORLD_WIDTH;
		monsters[i].y = rand() % WORLD_HEIGHT;
		auto level = 1;
		monsters[i].hp = 2 * level;
		monsters[i].level = level;
		monsters[i].exp = level * level * 2;
		auto findPlayerAct = static_cast<EFindPlayerAct>(rand() % static_cast<int>(EFindPlayerAct::COUNT));
		auto soloMove = static_cast<ESoloMove>(rand() % static_cast<int>(ESoloMove::COUNT));
		monsters[i].findPlayerAct = findPlayerAct;
		monsters[i].soloMove = soloMove;
		if (findPlayerAct == EFindPlayerAct::Peace) {
			if (soloMove == ESoloMove::Fixing) {
				monsters[i].name = "Peace Orc";
			} else if (soloMove == ESoloMove::Roaming) {
				monsters[i].name = "Peace Roaming Orc";
				monsters[i].exp *= 2;
			}
		} else if (findPlayerAct == EFindPlayerAct::Agro) {
			monsters[i].exp *= 2;
			if (soloMove == ESoloMove::Fixing) {
				monsters[i].name = "Agro Orc";
			} else if (soloMove == ESoloMove::Roaming) {
				monsters[i].name = "Agro Roaming Orc";
				monsters[i].exp *= 2;
			}
		}
		monsters[i].script = "Monster.lua";
	}
}

std::string WorldManager::GetPosString(int index) {
	int x, y;
	GetPosFormWorldIndex(index, x, y);
	std::stringstream ss;
	ss << "(" << x << "," << y << ")";
	return ss.str();
}

Monster* WorldManager::GetMonster(int id) {
	auto monster = Monster::Create(id);
	auto& fileMonster = monsters[id - MONSTER_ID_START];
	monster->Init(fileMonster);
	return monster;
}