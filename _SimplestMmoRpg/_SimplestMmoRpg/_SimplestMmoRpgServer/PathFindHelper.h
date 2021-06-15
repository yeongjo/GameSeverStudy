#pragma once
#include <string>
#include <unordered_map>

#include "removable_priority_queue.h"

class WorldManager;

class PathFindHelper {
public:
	enum class FindStatus {
		Finding, CantFindWay, FoundWay
	};
	//protected:
	struct PathPoint {
		int index, fcost, gcost, parent;
		PathPoint(int index, int fcost, int gcost, int parent) :index(index), fcost(fcost), gcost(gcost), parent(parent) {}
		PathPoint() {}
		constexpr bool operator<(const PathPoint& a) const {
			return fcost > a.fcost;
		}
		constexpr bool operator>(const PathPoint& a) const {
			return fcost < a.fcost;
		}
		constexpr bool operator==(const PathPoint& a) const {
			return index == a.index;
		}

		std::string ToString();
	};
public:
	class PathQueue : public removable_priority_queue<PathPoint> {
	public:
		PathPoint* Find(int index);

		bool Replace(const PathPoint& value);

		void Sort();

		void Clear();

		void Print(WorldManager* worldManager);
	};
protected:
	PathQueue openPoints;
	std::unordered_map<int, PathPoint> closePoints;
	int targetX, targetY;
	int targetIdx;
	int startIdx;
	int goalPosIdx = -1, prevPosIdx;
	WorldManager* worldManager;
	std::vector<int> straightPath;
	FindStatus findStatus;

	PathFindHelper() {}

public:
	static PathFindHelper* Get(WorldManager* worldManager);

	void SetStartAndTarget(int startX, int startY, int targetX, int targetY);

	void GetNextPos(int& x, int& y);

	void FindPrevPath(int posIdx, std::vector<int>& path);

	/// <summary>
	/// 한 사이클 길을 찾습니다.
	/// </summary>
	/// <returns>길을 찾거나 찾을 이유가 없으면 true를 반환합니다</returns>
	FindStatus FindWayOnce();
private:
	int GetCostToTarget(int index);

	int GetCostToTarget(int x, int y) const;

	/// <summary>
	/// 해당 index 위치에서 목적지까지 직진거리로 갈 수 있는지 반환합니다.
	/// </summary>
	bool CanMoveStraight(int index);

	/// <summary>
	/// x, y 위치에서 목적지까지 직진거리로 갈 수 있는지 반환합니다.
	/// </summary>
	bool CanMoveStraight(int x, int y);

	/// <summary>
	/// x, y 위치에서 목적지까지 직진거리로 갈 수 있는지 반환합니다.
	/// </summary>
	bool CanMoveStraight(int x, int y, int targetX, int targetY) const;
};
