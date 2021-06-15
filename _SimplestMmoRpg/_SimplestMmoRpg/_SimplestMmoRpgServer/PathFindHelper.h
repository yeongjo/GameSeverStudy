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
	/// �� ����Ŭ ���� ã���ϴ�.
	/// </summary>
	/// <returns>���� ã�ų� ã�� ������ ������ true�� ��ȯ�մϴ�</returns>
	FindStatus FindWayOnce();
private:
	int GetCostToTarget(int index);

	int GetCostToTarget(int x, int y) const;

	/// <summary>
	/// �ش� index ��ġ���� ���������� �����Ÿ��� �� �� �ִ��� ��ȯ�մϴ�.
	/// </summary>
	bool CanMoveStraight(int index);

	/// <summary>
	/// x, y ��ġ���� ���������� �����Ÿ��� �� �� �ִ��� ��ȯ�մϴ�.
	/// </summary>
	bool CanMoveStraight(int x, int y);

	/// <summary>
	/// x, y ��ġ���� ���������� �����Ÿ��� �� �� �ִ��� ��ȯ�մϴ�.
	/// </summary>
	bool CanMoveStraight(int x, int y, int targetX, int targetY) const;
};
