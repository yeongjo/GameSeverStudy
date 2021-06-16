#include "PathFindHelper.h"

#include <iostream>
#include <sstream>

#include "WorldManager.h"

std::string PathFindHelper::PathPoint::ToString() {
	std::stringstream ss;
	ss << "gcost: " << gcost << "parent: " << parent;
	return ss.str();
}

PathFindHelper::PathPoint* PathFindHelper::PathQueue::Find(int index) {
	auto it = std::find_if(this->c.begin(), this->c.end(), [=](PathPoint& path) {
		return path.index == index;
	});
	if (it != this->c.end()){
		return &*it;
	}
	return nullptr;
}

bool PathFindHelper::PathQueue::Replace(const PathPoint& value) {
	auto it = std::find(this->c.begin(), this->c.end(), value);
	if (it != this->c.end()){
		*it = value;
		Sort();
		return true;
	}
	return false;
}

void PathFindHelper::PathQueue::Sort() {
	std::sort(this->c.begin(), this->c.end());
}

void PathFindHelper::PathQueue::Clear() {
	this->c.clear();
}

void PathFindHelper::PathQueue::Print(WorldManager* worldManager) {
	for (auto t : this->c){
		std::cout << worldManager->GetPosString(t.index) << ",cost:" << t.fcost << "/";
	}
}

PathFindHelper* PathFindHelper::Get(WorldManager* worldManager) {
	auto result = new PathFindHelper;
	result->worldManager = worldManager;
	return result;
}

void PathFindHelper::SetStartAndTarget(int startX, int startY, int targetX, int targetY) {
	prevPosIdx = startIdx = worldManager->GetWorldIndex(startX, startY);
	closePoints.clear();
	goalPosIdx = -1;
	this->targetX = targetX;
	this->targetY = targetY;
	targetIdx = worldManager->GetWorldIndex(targetX, targetY);
	openPoints.Clear();
	openPoints.push(PathPoint(startIdx, 0, 0, -1));
	findStatus = FindStatus::Finding;
	//cout << "SetStartAndTarget() startX" << startX << ", startY" << startY << ", targetX" << targetX << ", targetY" << targetY << ": openPointsSize:" << openPoints.size() << endl;
}

void PathFindHelper::GetNextPos(int& x, int& y) {
	if (goalPosIdx == -1){
		return;
	}

	std::vector<int> findingPath;
	FindPrevPath(goalPosIdx, findingPath);
	/*cout << "x:" << x << "y:" << y;
	cout << " 찾아진 길: ";
	for (int i = findingPath.size() - 1;  0 <= i; --i) {
		cout << worldManager->GetPosString(findingPath[i]);
	}
	cout << ": ";*/
	int prevPathX, prevPathY;
	for (int i = 0; i < findingPath.size() - 1; ++i){
		// 목적지부터 나 있는 곳까지 이동해서 직진으로 도착할 수 있는 곳이 있으면 거기로는 직선으로 이동한다
		worldManager->GetPosFormWorldIndex(findingPath[i], prevPathX, prevPathY);
		if (x == prevPathX && y == prevPathY){
			continue;
		}
		if (CanMoveStraight(x, y, prevPathX, prevPathY)){
			worldManager->GetPosFormWorldIndex(findingPath[i], x, y);
			break;
		}
	}
	//cout << "다음 위치->" << x << "," << y << endl;
}

void PathFindHelper::FindPrevPath(int posIdx, std::vector<int>& path) {
	auto findNextPosIter = closePoints.find(posIdx);
	if (findNextPosIter == closePoints.end()){
		return;
	}
	path.push_back(posIdx);
	FindPrevPath(findNextPosIter->second.parent, path);
}

PathFindHelper::FindStatus PathFindHelper::FindWayOnce() {
	if (findStatus == FindStatus::FoundWay || openPoints.empty()){
		findStatus = findStatus == FindStatus::Finding ? FindStatus::CantFindWay : FindStatus::FoundWay;
		return findStatus;
	}
	//cout << "FindWayOnce: ";
	//cout << "openPoints: ";
	//openPoints.Print(worldManager);
	//cout << "closePoints: ";
	//for (auto closePoint : closePoints) {
	//	cout << worldManager->GetPosString(closePoint.second.index) << "/";
	//}
	//cout << endl;

	auto curPoint = openPoints.top();
	openPoints.pop();
	goalPosIdx = curPoint.index;
	closePoints[curPoint.index] = curPoint;
	if (CanMoveStraight(curPoint.index)){
		if (targetIdx != curPoint.index){
			closePoints[targetIdx] = PathPoint(targetIdx, 0, 0, curPoint.index);
		}
		goalPosIdx = targetIdx;
		findStatus = FindStatus::FoundWay;
		return findStatus;
	}

	auto worldWidth = worldManager->GetWidth();
	auto worldHeight = worldManager->GetHeight();
	int curX, curY;
	worldManager->GetPosFormWorldIndex(curPoint.index, curX, curY);
	int offset[] = {-1, 1, -worldWidth, worldWidth};
	int offsetX[] = {-1, 1, 0, 0};
	int offsetY[] = {0, 0, -1, 1};
	for (int i = 0; i < 4; i++){
		auto movedIdx = curPoint.index + offset[i];
		auto movedX = curX + offsetX[i];
		auto movedY = curY + offsetY[i];
		if (movedX < 0 || worldWidth <= movedX ||
			movedY < 0 || worldHeight <= movedY ||
			worldManager->GetCollidable(movedIdx) ||
			closePoints.find(movedIdx) != closePoints.end()){
			// 갈 수 없는 길이면 넘기기
			continue;
		}
		if (targetIdx == movedIdx){
			// 길찾기 성공
			closePoints[movedIdx] = PathPoint(movedIdx, 0, 0, curPoint.index);
			goalPosIdx = movedIdx;
			findStatus = FindStatus::FoundWay;
			return findStatus;
		}
		auto hcost = GetCostToTarget(movedIdx);
		auto gcost = 1 + curPoint.gcost;
		auto fcost = gcost + hcost;
		auto movedIndexGcost = openPoints.Find(movedIdx);
		if (movedIndexGcost){
			if (movedIndexGcost->gcost < gcost){
				openPoints.Replace(PathPoint(movedIdx, fcost, gcost, curPoint.index));
			}
		}
		else{
			openPoints.emplace(PathPoint(movedIdx, fcost, gcost, curPoint.index));
		}
	}
	findStatus = FindStatus::Finding;
	return findStatus;
}

int PathFindHelper::GetCostToTarget(int index) {
	int x, y;
	worldManager->GetPosFormWorldIndex(index, x, y);
	return GetCostToTarget(x, y);
}

int PathFindHelper::GetCostToTarget(int x, int y) const {
	return abs(x - targetX) + abs(y - targetY);
}

bool PathFindHelper::CanMoveStraight(int index) {
	int x, y;
	worldManager->GetPosFormWorldIndex(index, x, y);
	return CanMoveStraight(x, y);
}

bool PathFindHelper::CanMoveStraight(int x, int y) {
	return CanMoveStraight(x, y, targetX, targetY);
}

bool PathFindHelper::CanMoveStraight(int x, int y, int targetX, int targetY) const {
	while (x != targetX || y != targetY){
		auto xOff = targetX - x;
		auto yOff = targetY - y;
		if (abs(xOff) > abs(yOff)){
			xOff > 0 ? ++x : xOff < 0 ? --x : x;
		}
		else{
			yOff > 0 ? ++y : yOff < 0 ? --y : y;
		}
		if (worldManager->GetCollidable(x, y)){
			return false;
		}
	}

	return true;
}
