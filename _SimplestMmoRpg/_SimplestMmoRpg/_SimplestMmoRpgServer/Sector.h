#pragma once
#include <array>
#include <mutex>
#include <unordered_set>

#include "ConstexprMath.h"
#include "protocol.h"
constexpr int WORLD_SECTOR_SIZE = (VIEW_RADIUS + 2);
constexpr int WORLD_SECTOR_X_COUNT = ceil_const(WORLD_WIDTH / (float)WORLD_SECTOR_SIZE);
constexpr int WORLD_SECTOR_Y_COUNT = ceil_const(WORLD_HEIGHT / (float)WORLD_SECTOR_SIZE);

// TODO 섹터 매니저 만들어서 하거나, static 사용해서 객체지향적으로 만들면 좋을듯
class Sector {
	/// <summary>
	/// 섹터 안에 있는 플레이어 ID
	/// sectorLock으로 잠궈주고 사용
	/// </summary>
	std::unordered_set<int> sector;
	std::mutex sectorLock;
	static std::array <std::array <Sector, WORLD_SECTOR_X_COUNT>, WORLD_SECTOR_Y_COUNT> world_sector;
public:
	void Add(int actorId);

	void Remove(int actorId);

	static void Move(int actorId, int prevX, int prevY, int x, int y);

	/// <summary>
	/// 섹터에 있는 세션 벡터를 리턴합니다. p_id는 포함하지 않습니다.
	/// npc id로 호출하면 npc는 걸러서 리턴합니다.
	/// </summary>
	/// <param name="playerId"></param>
	/// <returns></returns>
	static void GetViewListFromSector(int playerId, std::vector<int>& newViewList);

	static Sector* GetSector(int x, int y);
private:
	/// <summary>
	/// p_id를 가진 플레이어를 제외하고 해당 좌표 섹터에 있는 플레이어를 main_sector에 추가합니다.
	/// </summary>
	/// <param name="y"></param>
	/// <param name="id"></param>
	/// <param name="x"></param>
	/// <param name="returnSector"></param>
	static void AddSectorPlayersToMainSector(int id, int y, int x, std::vector<int>& returnSector);
};

