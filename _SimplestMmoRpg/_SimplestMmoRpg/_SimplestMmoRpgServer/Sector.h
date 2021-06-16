#pragma once
#include <array>
#include <mutex>
#include <unordered_set>

#include "ConstexprMath.h"
#include "protocol.h"
constexpr int WORLD_SECTOR_SIZE = (VIEW_RADIUS + 2);
constexpr int WORLD_SECTOR_X_COUNT = ceil_const(WORLD_WIDTH / (float)WORLD_SECTOR_SIZE);
constexpr int WORLD_SECTOR_Y_COUNT = ceil_const(WORLD_HEIGHT / (float)WORLD_SECTOR_SIZE);

// TODO ���� �Ŵ��� ���� �ϰų�, static ����ؼ� ��ü���������� ����� ������
class Sector {
	/// <summary>
	/// ���� �ȿ� �ִ� �÷��̾� ID
	/// sectorLock���� ����ְ� ���
	/// </summary>
	std::unordered_set<int> sector;
	std::mutex sectorLock;
	static std::array <std::array <Sector, WORLD_SECTOR_X_COUNT>, WORLD_SECTOR_Y_COUNT> world_sector;
public:
	void Add(int actorId);

	void Remove(int actorId);

	static void Move(int actorId, int prevX, int prevY, int x, int y);

	/// <summary>
	/// ���Ϳ� �ִ� ���� ���͸� �����մϴ�. p_id�� �������� �ʽ��ϴ�.
	/// npc id�� ȣ���ϸ� npc�� �ɷ��� �����մϴ�.
	/// </summary>
	/// <param name="playerId"></param>
	/// <returns></returns>
	static void GetViewListFromSector(int playerId, std::vector<int>& newViewList);

	static Sector* GetSector(int x, int y);
private:
	/// <summary>
	/// p_id�� ���� �÷��̾ �����ϰ� �ش� ��ǥ ���Ϳ� �ִ� �÷��̾ main_sector�� �߰��մϴ�.
	/// </summary>
	/// <param name="y"></param>
	/// <param name="id"></param>
	/// <param name="x"></param>
	/// <param name="returnSector"></param>
	static void AddSectorPlayersToMainSector(int id, int y, int x, std::vector<int>& returnSector);
};

