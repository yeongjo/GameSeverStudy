#include "Sector.h"

#include "Actor.h"

std::array <std::array <SECTOR, WORLD_SECTOR_X_COUNT>, WORLD_SECTOR_Y_COUNT> SECTOR::world_sector;

void SECTOR::Add(int actorId) {
	std::lock_guard<std::mutex> lock(sectorLock);
	sector.insert(actorId);
}

void SECTOR::Remove(int actorId) {
	std::lock_guard<std::mutex> lock(sectorLock);
	sector.erase(actorId);
}

void SECTOR::Move(int actorId, int prevX, int prevY, int x, int y) {
	if (y != prevY || x != prevX){
		// 원래 섹터랑 다르면 다른 섹터로 이동한 것임
		GetSector(prevX, prevY)->Remove(actorId);
		GetSector(x, y)->Add(actorId);
	}
}

void SECTOR::AddSectorPlayersToMainSector(int id, int y, int x, std::vector<int>& returnSector) {
	auto actor = Actor::Get(id);
	auto isNpc = actor->IsNpc();
	std::lock_guard<std::mutex> lock(world_sector[y][x].sectorLock);
	auto& otherSet = world_sector[y][x].sector;
	for (auto otherId : otherSet){
		auto otherActor = Actor::Get(otherId);
		auto otherIsNpc = otherActor->IsNpc();
		if ((isNpc && otherIsNpc) || // p_id가 NPC면 NPC는 필요없다. 사람만 인식한다
			otherId == id || // 내 id는 몰라도 된다
			!actor->CanSee(otherActor)){
			// 안보이는건 필요없다.
			continue;
		}
		returnSector.push_back(otherId);
	}
}

std::vector<int>& SECTOR::GetIdFromOverlappedSector(int playerId) {
	auto actor = Actor::Get(playerId);
	auto amINpc = actor->IsNpc();
	const int y = actor->GetY();
	const int x = actor->GetX();
	const auto sectorY = y / WORLD_SECTOR_SIZE;
	const auto sectorX = x / WORLD_SECTOR_SIZE;
	auto& mainSector = world_sector[sectorY][sectorX];

	auto& returnSector = actor->GetSelectedSector();
	mainSector.sectorLock.lock();
	for (auto otherId : mainSector.sector){
		const auto otherActor = Actor::Get(otherId);
		if ((amINpc && otherActor->IsNpc()) ||
			otherId == playerId ||
			!actor->CanSee(otherActor)){
			continue;
		}
		returnSector.push_back(otherId);
	}
	mainSector.sectorLock.unlock();

	const auto sectorViewFrustumTop = (y - HALF_VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	const auto sectorViewFrustumBottom = (y + HALF_VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	const auto sectorViewFrustumLeft = (x - HALF_VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	const auto sectorViewFrustumRight = (x + HALF_VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	const auto isLeft = sectorViewFrustumLeft != sectorX && 0 < sectorX;
	const auto isRight = sectorViewFrustumRight != sectorX && sectorX < static_cast<int>(world_sector[sectorViewFrustumTop].
		size() - 1);
	if (sectorViewFrustumTop != sectorY && 0 < sectorY){
		AddSectorPlayersToMainSector(playerId, sectorViewFrustumTop, sectorX, returnSector);
		if (isLeft){
			AddSectorPlayersToMainSector(playerId, sectorViewFrustumTop, sectorViewFrustumLeft, returnSector);
		}
		else if (isRight){
			AddSectorPlayersToMainSector(playerId, sectorViewFrustumTop, sectorViewFrustumRight,
			                             returnSector);
		}
	}
	else if (sectorViewFrustumBottom != sectorY && sectorY < static_cast<int>(world_sector.size() - 1)){
		AddSectorPlayersToMainSector(playerId, sectorViewFrustumBottom, sectorX, returnSector);
		if (isLeft){
			AddSectorPlayersToMainSector(playerId, sectorViewFrustumBottom, sectorViewFrustumLeft,
			                             returnSector);
		}
		else if (isRight){
			AddSectorPlayersToMainSector(playerId, sectorViewFrustumBottom, sectorViewFrustumRight,
			                             returnSector);
		}
	}
	if (isLeft){
		AddSectorPlayersToMainSector(playerId, sectorY, sectorViewFrustumLeft, returnSector);
	}
	else if (isRight){
		AddSectorPlayersToMainSector(playerId, sectorY, sectorViewFrustumRight, returnSector);
	}
	return returnSector;
}

SECTOR* SECTOR::GetSector(int x, int y) {
	return &world_sector[y/WORLD_SECTOR_SIZE][x / WORLD_SECTOR_SIZE];
}
