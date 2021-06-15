#include "Sector.h"

#include "Actor.h"

std::array <std::array <Sector, WORLD_SECTOR_X_COUNT>, WORLD_SECTOR_Y_COUNT> Sector::world_sector;

void Sector::Add(int actorId) {
	std::lock_guard<std::mutex> lock(sectorLock);
	sector.insert(actorId);
}

void Sector::Remove(int actorId) {
	std::lock_guard<std::mutex> lock(sectorLock);
	sector.erase(actorId);
}

void Sector::Move(int actorId, int prevX, int prevY, int x, int y) {
	if (y != prevY || x != prevX){
		// ���� ���Ͷ� �ٸ��� �ٸ� ���ͷ� �̵��� ����
		GetSector(prevX, prevY)->Remove(actorId);
		GetSector(x, y)->Add(actorId);
	}
}

void Sector::AddSectorPlayersToMainSector(int id, int y, int x, std::vector<int>& returnSector) {
	auto actor = Actor::Get(id);
	auto isNpc = actor->IsNpc();
	std::lock_guard<std::mutex> lock(world_sector[y][x].sectorLock);
	auto& otherSet = world_sector[y][x].sector;
	for (auto otherId : otherSet){
		auto otherActor = Actor::Get(otherId);
		auto otherIsNpc = otherActor->IsNpc();
		if ((isNpc && otherIsNpc) || // p_id�� NPC�� NPC�� �ʿ����. ����� �ν��Ѵ�
			otherId == id || // �� id�� ���� �ȴ�
			!actor->CanSee(otherActor)){
			// �Ⱥ��̴°� �ʿ����.
			continue;
		}
		returnSector.push_back(otherId);
	}
}

std::vector<int>& Sector::GetIdFromOverlappedSector(int playerId) {
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

Sector* Sector::GetSector(int x, int y) {
	return &world_sector[y/WORLD_SECTOR_SIZE][x / WORLD_SECTOR_SIZE];
}
