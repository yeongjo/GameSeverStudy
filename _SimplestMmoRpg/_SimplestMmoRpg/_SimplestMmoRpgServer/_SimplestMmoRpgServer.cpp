// =============== ����ȭ ===============
// BufOverManager::Get(), BufOverManager::Recycle�� �̿��� BufOver ��Ȱ��
// BufOverManager::AddSendingData()���� ��Ŷ�� �����ߴ�
// ������ �����忡�� BufOverManager::SendAddedData() ȣ��� ��Ŷ �Ѳ����� �������� WSASendȣ�� �ּ�ȭ
// CanSee�� Array�� Ȱ���� ����ó���� �þ߳� �÷��̾�Ը� ��Ŷ����
// ===========================================


// =============== ���� �̵� ===============
// �þ߿� �������� ��� ��ũ��Ʈ���� �������� ���� �����Ѵ�.
// (1ĭ ���������� �ٰ�����, �÷��̾� ��ġ�� ���޹����鼭 ������ ���� bool�� ��ũ��Ʈ���� �����Ѵ�.) ��ֹ� ȸ�ǿ��ο� ���� �Լ��� ����, Ȥ�� wakeup�����϶� �� �������� ����� �Լ� ȣ��ǰ� ���൵ ������. �ٵ� �ٸ� �÷��̾��� ������ �ٸ������� �Ѱܹ޾Ƽ� �۷ι� ������ ������ �վ���ϳ�
// 
// �÷��̾�� �ٰ����鼭 �ֺ� �÷��̾�� ��ġ���� ���踦 ��ũ��Ʈ���� �˻��ϰ� ���ش�.
// �迭�� �ѱ�� �� ����
// ===========================================


// =================== ���� ==================
// ���� �޽��� ����ؾ��� - sendChat�� �����ؾ��ϳ� �α׷� ����ؾ��ϳ�?
// �ٸ� NPC�� �ִ� ������ �̵��ϸ� ������. sc_packet_stat_change��Ŷ ����
// ===========================================


// =================== ��ų ==================
// ���⼺ ��ų, ������ų
// ===========================================


// =================== �� json �ε� ==================
// Ŭ�� ������ ���� �� �����͸� ������ �־����
// ���� ��ġ ����. ������ �����ð� ������ ����� �Ǿ����
//===========================================


// =============== ��ֹ� ȸ�� �̵� ===========
// c++���� A* ����Ͽ� �̵�
// ===========================================


#include <iostream>
#include <unordered_map>
#include <thread>
#include <vector>
#include <mutex>
#include <array>
#include <functional>
#include <map>
#include <queue>
#include <random>
#include <sstream>
#include <stack>
#include <unordered_set>
class PathFindHelper;
class Monster;
class Player;
struct Actor;
struct BufOverManager;
using namespace std;
#include <WS2tcpip.h>
#include <MSWSock.h>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#define DISPLAYLOG
//#define PLAYERLOG
//#define NPCLOG
#define PLAYER_NOT_RANDOM_SPAWN
mutex coutMutex;

#pragma comment(lib, "lua54.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")

#include "protocol.h"

#define BATCH_SEND_BYTES 1024 // TODO ExOver�� �������ִ� �����ͻ���� 256�̶� ����Ҽ�����
constexpr int MONSTER_AGRO_RANGE = 3; // ���Ͱ� ��׷� ������ ����

constexpr int32_t ceil_const(float num) {
	return (static_cast<float>(static_cast<int32_t>(num)) == num)
		? static_cast<int32_t>(num)
		: static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
}

constexpr int WORLD_SECTOR_SIZE = (VIEW_RADIUS + 2);
constexpr int WORLD_SECTOR_X_COUNT = ceil_const(WORLD_WIDTH / (float)WORLD_SECTOR_SIZE);
constexpr int WORLD_SECTOR_Y_COUNT = ceil_const(WORLD_HEIGHT / (float)WORLD_SECTOR_SIZE);

enum EPlayerState { PLST_FREE, PLST_CONNECTED, PLST_INGAME };

typedef function<void(int)> iocpCallback;

struct Color {
	unsigned char b, g, r;
};

struct Image {
	Color* pixel;
	int width, height;

	Image(){}
	Image(Color* pixel, int width, int height) : pixel(pixel), width(width), height(height){}
	~Image() {
		delete[] pixel;
	}

	void ReadBmp(const char* filename) {
		int i;
		FILE* f;
		if (0 != fopen_s(&f, filename, "rb")) {
			cout << "bmp open error: " << filename << endl;
			return;
		}
		unsigned char info[54];

		// read the 54-byte header
		fread(info, sizeof(unsigned char), 54, f);

		// extract image height and width from header
		width = *(int*)&info[18];
		height = *(int*)&info[22];

		// allocate 3 bytes per pixel
		int size = width * height;
		pixel = new Color[size];
		//unsigned char* pixel = new unsigned char[size*3];

		// read the rest of the data at once
		//fread(pixel, sizeof(unsigned char), size *3, f);
		fread(pixel, sizeof(unsigned char), size * 3, f);
		fclose(f);

		//Now data should contain the (R, G, B) values of the pixels. The color of pixel (i, j) is stored at data[3 * (i * width + j)], data[3 * (i * width + j) + 1] and data[3 * (i * width + j) + 2].
	}

	Color GetPixel(int x, int y) const {
		_ASSERT(0 <= x && x < width);
		_ASSERT(0 <= y && y < height);
		return pixel[(height-y-1) * width + x];
	}
};

template <typename T, typename U>
void asTable(lua_State* L, T begin, U end) {
	lua_newtable(L);
	for (size_t i = 0; begin != end; ++begin, ++i) {
		lua_pushinteger(L, i + 1);
		lua_pushnumber(L, *begin);
		lua_settable(L, -3);
	}
}

template <typename T, typename U>
void fromTable(lua_State* L, T begin, U end) {
	_ASSERT(lua_istable(L, -1));
	for (size_t i = 0; begin != end; ++begin, ++i) {
		lua_pushinteger(L, i + 1);
		lua_gettable(L, -2);
		*begin = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}
}

/// <summary>
/// [��Ƽ������ ����]����Ʈ�� Ǯ����, �տ��� ���� �Ͱ� �ڿ��� �ִ� ���� ���� ���� �ɾ� ���� ����� �븲
/// </summary>
/// <typeparam name="T"></typeparam>
template<class T>
class StructPool {
	struct Element {
		T* element;
		T* next;
	};
	Element* start;
	mutex startLock;
	Element* end;
	mutex endLock;
	atomic_int size;
public:	
	T* Get() {
		lock_guard<mutex> lock(startLock);
		auto returnObj = start;
		start = start->next; // start�� null �����ʰ� ���ο� ������Ʈ�� ��� ������־���Ѵ�.
		--size;
		if(size <= 1){ // end �ϳ��� ���ܵΰ� ������Ŵ
			CreatePoolObjects();
		}
		return returnObj;
	}

	void Recycle(T* obj) {
		lock_guard<mutex> lock(endLock);
		end->next = obj;
		end = obj;
		end->next = nullptr;
		++size;
	}
protected:
	StructPool() : start(nullptr){
		auto newObj = new Element;
		newObj->element = new T;
		end = newObj;
		end->next = nullptr;
		newObj = new Element;
		newObj->element = new T;
		start = newObj;
		start->next = end;
	}
private:
	void CreatePoolObjects() { // startLock�ɰ� ȣ��Ǿ���ϸ�, ���Ұ� ������ �Ѱ� �̻� �־�� ��
		for (size_t i = 0; i < 2; i++) {
			auto newObj = new Element;
			newObj->element = new T;
			newObj->next = start;
			start = newObj;
		}
	}
};

struct MiniOver {
	WSAOVERLAPPED	over; // Ŭ���� �����ڿ��� �ʱ�ȭ�ϴ� ���� ������� ���ƿ´�??
	iocpCallback callback;
	virtual void Recycle(){}
};
struct AcceptOver : public MiniOver {
	SOCKET			cSocket;					// OP_ACCEPT������ ���
	WSABUF			wsabuf[1];
};
struct BufOver : public MiniOver {
	WSABUF			wsabuf[1];
	unsigned char	packetBuf[MAX_BUFFER];
private:
	BufOverManager* manager;
public:
	BufOver() {}
	BufOver(BufOverManager* manager) : manager(manager) {}
	void InitOver() {
		memset(&over, 0, sizeof(over));
	}

	void Recycle() override;
	BufOverManager* GetManager() {
		return manager;
	}
};
struct RecvOver : public BufOver {
	void Recycle() override { }
};

/// <summary>
/// Get()���� BufOver ����
/// BufOver->Recycle() ȣ���ϸ� ��
/// </summary>
struct BufOverManager {
#define INITAL_MANAGED_EXOVER_COUNT 2
private:
	vector<BufOver*> managedExOvers;
	mutex managedExOversLock;
	vector<unsigned char> sendingData;
	mutex sendingDataLock;
	vector<vector<unsigned char>*> sendingDataQueue;
	mutex sendingDataQueueLock;
	static size_t EX_OVER_SIZE_INCREMENT;

public:
	BufOverManager() : managedExOvers(INITAL_MANAGED_EXOVER_COUNT) {
		auto size = INITAL_MANAGED_EXOVER_COUNT;
		for (size_t i = 0; i < size; ++i) {
			managedExOvers[i] = new BufOver(this);
			managedExOvers[i]->InitOver();
		}
	}

	virtual ~BufOverManager() {
		for (auto queue : sendingDataQueue) {
			delete queue;
		}
		sendingDataQueue.clear();
	}

	/// <summary>
	/// ���� �����Ͱ� ���������� true ��ȯ
	/// ���ο��� lock �����
	/// </summary>
	/// <returns></returns>
	bool HasSendData() {
		lock_guard<mutex> lock(sendingDataLock);
		return !sendingData.empty();
	}

	/// <summary>
	/// ���� �����͸� ť�� �׾ƵӴϴ�.
	/// </summary>
	/// <param name="p"></param>
	void AddSendingData(void* p) {
		const auto packetSize = static_cast<size_t>(static_cast<unsigned char*>(p)[0]);
		lock_guard<mutex> lock(sendingDataLock);
		const auto prevSize = sendingData.size();
		const auto totalSendPacketSize = prevSize + packetSize;
		sendingData.resize(totalSendPacketSize);
		memcpy(&sendingData[prevSize], p, packetSize);
	}

	/// <summary>
	/// �����ص� �����͸� ��� �ʱ�ȭ�մϴ�.
	/// </summary>
	void ClearSendingData() {
		lock_guard<mutex> lock(sendingDataLock);
		sendingData.clear();
	}

	/// <summary>
	/// �����ص� �����͸� send�� ȣ���Ͽ� id�� �ش��ϴ� �÷��̾ �����ϴ�.
	/// NPC���� ������ �ʰ� ����ó������ �ʽ��ϴ�.
	/// </summary>
	/// <param name="playerId"></param>
	void SendAddedData(int playerId);

	BufOver* Get() {
		BufOver* result;
		managedExOversLock.lock();
		size_t size = managedExOvers.size();
		if (size == 0) {
			managedExOvers.resize(size + EX_OVER_SIZE_INCREMENT);
			for (size_t i = size; i < size + EX_OVER_SIZE_INCREMENT; ++i) {
				managedExOvers[i] = new BufOver(this);
				managedExOvers[i]->InitOver();
			}
			managedExOversLock.unlock();
			auto newOver = new BufOver(this);
			newOver->InitOver();
			return newOver;
		}
		result = managedExOvers[size - 1];
		managedExOvers.pop_back(); // �˾��ϴ� ������� ����ȭ ����
		managedExOversLock.unlock();
		return result;
	}

	/// <summary>
	/// ���� ȣ���ϴ� �Լ� �ƴ� BufOver::Recycle����Ұ�
	/// </summary>
	/// <param name="usableGroup"></param>
	void Recycle(BufOver* usableGroup) {
		usableGroup->InitOver();
		managedExOversLock.lock();
		managedExOvers.push_back(usableGroup);
		managedExOversLock.unlock();
	}

private:
	vector<unsigned char>* GetSendingDataQueue() {
		lock_guard<mutex> lock(sendingDataQueueLock);
		auto size = sendingDataQueue.size();
		if (0 < size) {
			const auto result = sendingDataQueue[size - 1];
			sendingDataQueue.pop_back();
			return result;
		}
		return new vector<unsigned char>();
	}

	void RecycleSendingDataQueue(vector<unsigned char>* recycleQueue) {
		lock_guard<mutex> lock(sendingDataQueueLock);
		sendingDataQueue.push_back(recycleQueue);
	}
};

typedef function<bool ()> TimerEventCheckCondition;

size_t BufOverManager::EX_OVER_SIZE_INCREMENT = 2;
struct TimerEvent {
	int object;
	/// <summary>
	/// false�� Ÿ�̸ӿ��� ������.
	/// </summary>
	TimerEventCheckCondition checkCondition;
	iocpCallback callback;
	chrono::system_clock::time_point startTime;
	int targetId;
	char buffer[MESSAGE_MAX_BUFFER];
	bool hasBuffer;

	constexpr bool operator< (const TimerEvent& L) const {
		return (startTime > L.startTime);
	}
	constexpr bool operator== (const TimerEvent& L) const {
		return (object == L.object);
	}
};
template<typename T>
class removable_priority_queue : public std::priority_queue<T, std::vector<T>> {
public:
	bool remove(const T& value) {
		auto it = std::find(this->c.begin(), this->c.end(), value);
		if (it != this->c.end()) {
			this->c.erase(it);
			std::make_heap(this->c.begin(), this->c.end(), this->comp);
			return true;
		}
		return false;
	}
	void remove_all(const T& value) {
		for (auto iter = this->c.begin(); iter != this->c.end();) {
			if (*iter == value) {
				this->c.erase(iter);
				continue;
			}
			++iter;
		}
	}
};
class TimerQueue : public removable_priority_queue<TimerEvent> {
public:
	// TODO Add�Ҷ� id ���Ϲް� �װ� �������� �������ҵ� �ٵ� ������ ���Ͼ���
	//bool remove(const int playerId, const EEventType eventType) {
	//	auto size = this->c.size();
	//	for (size_t i = 0; i < size;) {
	//		if (this->c[i].object == playerId && this->c[i].eventType == eventType) {
	//			auto begin = this->c.begin();
	//			this->c.erase(begin + i);
	//			--size;
	//			return true;
	//		}
	//		++i;
	//	}
	//	return false;
	//}
	void remove_all(const int playerId) {
		auto size = this->c.size();
		auto object = playerId;
		for (size_t i = 0; i < size;) {
			if (this->c[i].object == object) {
				auto begin = this->c.begin();
				this->c.erase(begin + i);
				--size;
				continue;
			}
			++i;
		}
	}
};


constexpr int SERVER_ID = 0;
HANDLE hIocp;

class TimerQueueManager {
	/// <summary>
	/// timerLock���� ����ְ� ���
	/// </summary>
	static TimerQueue timerQueue;
	static mutex timerLock;

public:
	static void Add(TimerEvent event) {
		lock_guard<mutex> lock(timerLock);
		timerQueue.push(event);
	}

	static void RemoveAll(int playerId) {
		lock_guard<mutex> lock(timerLock);
		timerQueue.remove_all(playerId);
	}

	static void Add(int obj, int delayMs, TimerEventCheckCondition checkCondition, iocpCallback callback, const char* buffer = 0, int targetId = 0) {
		using namespace chrono;
		TimerEvent ev;
		ev.checkCondition = checkCondition;
		ev.callback = callback;
		ev.object = obj;
		ev.startTime = system_clock::now() + milliseconds(delayMs);
		ev.targetId = targetId;
		if (nullptr != buffer) {
			memcpy(ev.buffer, buffer, sizeof(char) * (strlen(buffer) + 1));
			ev.hasBuffer = true;
		} else {
			ev.hasBuffer = false;
		}
		Add(ev);
	}

	static void Do();
};
TimerQueue TimerQueueManager::timerQueue;
mutex TimerQueueManager::timerLock;

// TODO ���� �Ŵ��� ���� �ϰų�, static ����ؼ� ��ü���������� ����� ������
struct SECTOR {
	/// <summary>
	/// ���� �ȿ� �ִ� �÷��̾� ID
	/// sectorLock���� ����ְ� ���
	/// </summary>
	unordered_set<int> sector;
	mutex sectorLock;

	void Add(int actorId) {
		lock_guard<mutex> lock(sectorLock);
		sector.insert(actorId);
	}
	
	void Remove(int actorId) {
		lock_guard<mutex> lock(sectorLock);
		sector.erase(actorId);
	}
};

array <array <SECTOR, WORLD_SECTOR_X_COUNT>, WORLD_SECTOR_Y_COUNT> world_sector;

struct Session {
	atomic<EPlayerState> state;
	RecvOver recvOver;
	BufOverManager bufOverManager;

	/// <summary>
	/// socketLock���� ����ְ� ���
	/// </summary>
	SOCKET socket;
	mutex  socketLock;

	/// <summary>
	/// ��Ŷ �Ѱ����� ���� ����� ��
	/// recvedBufLock���� ����ְ� ���
	/// </summary>
	vector<unsigned char> recvedBuf; // ��Ŷ �Ѱ����� ���� ����� ��
	mutex recvedBufLock;
	atomic<unsigned char> recvedBufSize;
};

array <Actor*, MAX_USER + 1> actors;

class WorldManager {
public:
	enum class EFindPlayerAct { Peace, Agro, COUNT };
	enum class ESoloMove { Fixing, Roaming, COUNT };
	struct FileMonster {
		int x, y, hp, level, exp, damage;
		string name;
		string script;
		EFindPlayerAct findPlayerAct;
		ESoloMove soloMove;
	};
private:
	vector<FileMonster> monsters;
	vector<ETile> world;
	int width, height;
public:
	void Load() {
		Image image;
		image.ReadBmp(MAP_PATH);
		world.resize(image.height* image.width);
		width = image.width;
		height = image.height;
		for (size_t y = 0; y < image.height; y++) {
			for (size_t x = 0; x < image.width; x++) {
				world[y*image.width+x] = ETile::Empty;
				switch (image.GetPixel(x, y).r) {
				case 255: {
					world[y * image.width +x] = ETile::Wall;
					break;
				}
				}
			}
		}
	}

	void Generate() {
		monsters.resize(MAX_MONSTER);
		for (size_t i = 0; i < monsters.size(); i++) {
			monsters[i].x = rand() % WORLD_WIDTH;
			monsters[i].y = rand() % WORLD_HEIGHT;
			auto level = 1;
			monsters[i].hp = 2* level;
			monsters[i].level = level;
			monsters[i].exp = level* level*2;
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

	Monster* GetMonster(int id);
	bool GetCollidable(int x, int y) {
		if(x < 0 || width <= x || y < 0 || height <= y){
			return true;
		}
		auto tile = world[y*width+x];
		return tile == Wall;
	}
	bool GetCollidable(int index) {
		auto tile = world[index];
		return tile == Wall;
	}
	int GetWorldIndex(int x, int y) {
		return y* width + x;
	}
	void GetPosFormWorldIndex(int index, int& x, int& y) {
		y = index / width;
		x = index - (y * width);
	}
	string GetPosString(int index) {
		int x, y;
		GetPosFormWorldIndex(index, x, y);
		stringstream ss;
		ss << "("<< x << "," << y << ")";
		return ss.str();
	}
	int GetWidth() { return width; }
	int GetHeight() { return height; }
};

WorldManager worldManager;

Actor* GetActor(int id) {
	return actors[id];
}

Player* GetPlayer(int id) {
	_ASSERT(0 < id && id < NPC_ID_START);
	return reinterpret_cast<Player*>(actors[id]);
}

Monster* GetMonster(int id) {
	_ASSERT(MONSTER_ID_START <= id && id < MONSTER_ID_START+MAX_MONSTER);
	return reinterpret_cast<Monster*>(actors[id]);
}

int Distance(int x, int y, int x2, int y2) {
	return abs(x - x2) + abs(y - y2);
}
struct Actor {
	int		id;
	char name[MAX_NAME];
	short	x, y, initX, initY;
	int		moveTime;

	atomic_bool isActive;

	/// <summary>
	/// selectedSectorLock���� ����ְ� ���
	/// </summary>
	vector<int> selectedSector;
	mutex   selectedSectorLock;

	/// <summary>
	/// oldNewViewListLock���� ����ְ� ���
	/// </summary>
	vector<int> oldViewList;
	/// <summary>
	/// oldNewViewListLock���� ����ְ� ���
	/// </summary>
	vector<int> newViewList;
	mutex oldNewViewListLock;

	/// <summary>
	/// viewSetLock���� ����ְ� ���
	/// </summary>
	unordered_set<int> viewSet;
	mutex   viewSetLock;

protected:
	Actor(int id) {
		this->id = id;
		actors[id] = this;
	}

public:

	virtual void Init() {
		initX = x;
		initY = y;
		isActive = false;
	}

	void InitSector() const {
		auto sectorViewFrustumX = x / WORLD_SECTOR_SIZE;
		auto sectorViewFrustumY = y / WORLD_SECTOR_SIZE;
		auto& sector = world_sector[sectorViewFrustumY][sectorViewFrustumX];
		lock_guard<mutex> lock(sector.sectorLock);
		sector.sector.insert(id);
	}

	void RemoveFromSector() const {
		auto sectorViewFrustumX = x / WORLD_SECTOR_SIZE;
		auto sectorViewFrustumY = y / WORLD_SECTOR_SIZE;
		auto& sector = world_sector[sectorViewFrustumY][sectorViewFrustumX];
		lock_guard<mutex> lock(sector.sectorLock);
		sector.sector.erase(id);
	}

	virtual void Update() {}

	virtual void Move(DIRECTION dir) {
		auto x = this->x;
		auto y = this->y;
		switch (dir) {
		case D_E: if (x < WORLD_WIDTH - 1) ++x;
			break;
		case D_W: if (x > 0) --x;
			break;
		case D_S: if (y < WORLD_HEIGHT - 1) ++y;
			break;
		case D_N: if (y > 0) --y;
			break;
		}
		if (IsMovableTile(x, y)) {
			SetPos(x, y);
		}
	}

	virtual void Interact(Actor* interactor) {}

	virtual void OnNearActorWithPlayerMove(int actorId) {}

	virtual void OnNearActorWithSelfMove(int actorId) {}

	virtual void AddToViewSet(int otherId) {
		lock_guard<mutex> lock(viewSetLock);
		viewSet.insert(otherId);
	}

	virtual void RemoveFromViewSet(int otherId); // TODO ���߿� RemoveAll ������ lock���� ���� �������°� �پ��

	virtual void Die();

	virtual void RemoveFromAll();

	virtual void SendStatChange();

	virtual void SetPos(int x, int y) {
		this->x = x;
		this->y = y;
	}

	virtual bool IsMovableTile(int x, int y) {
		return !worldManager.GetCollidable(x, y);
	}

	/// <summary>
	/// ������ false ����
	/// </summary>
	/// <param name="attackerId"></param>
	/// <returns></returns>
	virtual bool TakeDamage(int attackerId) {
		return true;
	}

	virtual void LuaLock(){}
	virtual void LuaUnLock(){}

	virtual int GetHp() { return -1; }
	virtual int GetLevel() { return -1; }
	virtual int GetExp() { return -1; }
	virtual int GetDamage() { return -1; }
	virtual MiniOver* GetOver() { return nullptr; }
	virtual void SetExp(int exp) {}
	virtual void SetLevel(int level) {}
	virtual void SetHp(int hp) {}
protected:
	vector<int>& CopyViewSetToOldViewList() {
		lock_guard<mutex> lock2(viewSetLock);
		oldViewList.resize(viewSet.size());
		std::copy(viewSet.begin(), viewSet.end(), oldViewList.begin());
		return oldViewList;
	}
};

class PathFindHelper {
public:
	enum class FindStatus {
		Finding, CantFindWay, FoundWay
	};
//protected:
	struct PathPoint {
		int index, fcost, gcost, parent;
		PathPoint(int index, int fcost, int gcost, int parent):index(index), fcost(fcost), gcost(gcost), parent(parent){}
		PathPoint(){}
		constexpr bool operator<(const PathPoint& a) const {
			return fcost > a.fcost;
		}
		constexpr bool operator>(const PathPoint& a) const {
			return fcost < a.fcost;
		}
		constexpr bool operator==(const PathPoint& a) const {
			return index == a.index;
		}
		string ToString() {
			stringstream ss;
			ss << "gcost: " << gcost << "parent: " << parent;
			return ss.str();
		}
	};
public:
	class PathQueue : public removable_priority_queue<PathPoint> {
	public:
		PathPoint* Find(int index) {
			auto it = std::find_if(this->c.begin(), this->c.end(), [=](PathPoint& path) {
				return path.index == index;
			});
			if (it != this->c.end()) {
				return &*it;
			}
			return nullptr;
		}
		bool Replace(const PathPoint& value) {
			auto it = std::find(this->c.begin(), this->c.end(), value);
			if (it != this->c.end()) {
				*it = value;
				Sort();
				return true;
			}
			return false;
		}

		void Sort() {
			std::sort(this->c.begin(), this->c.end());
		}

		void Clear() {
			this->c.clear();
		}
		void Print(WorldManager* worldManager) {
			for(auto t : this->c){
				cout << worldManager->GetPosString(t.index) << ",cost:" << t.fcost <<  "/";
			}
		}
	};
protected:
	PathQueue openPoints;
	unordered_map<int, PathPoint> closePoints;
	int targetX, targetY;
	int targetIdx;
	int startIdx;
	int goalPosIdx = -1, prevPosIdx;
	WorldManager* worldManager;
	vector<int> straightPath;
	FindStatus findStatus;
	
	PathFindHelper(){}

public:
	static PathFindHelper* Get(WorldManager* worldManager) {
		auto result = new PathFindHelper;
		result->worldManager = worldManager;
		return result;
	}

	void SetStartAndTarget(int startX, int startY, int targetX, int targetY) {
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

	void GetNextPos(int& x, int& y) {
		if(goalPosIdx == -1){
			return;
		}

		vector<int> findingPath;
		FindPrevPath(goalPosIdx, findingPath);
		/*cout << "x:" << x << "y:" << y;
		cout << " ã���� ��: ";
		for (int i = findingPath.size() - 1;  0 <= i; --i) {
			cout << worldManager->GetPosString(findingPath[i]);
		}
		cout << ": ";*/
		int prevPathX, prevPathY;
		for (int i = 0; i < findingPath.size() - 1; ++i) {
			// ���������� �� �ִ� ������ �̵��ؼ� �������� ������ �� �ִ� ���� ������ �ű�δ� �������� �̵��Ѵ�
			worldManager->GetPosFormWorldIndex(findingPath[i], prevPathX, prevPathY);
			if(x == prevPathX && y == prevPathY){
				continue;
			}
			if(CanMoveStraight( x, y, prevPathX, prevPathY)){
				worldManager->GetPosFormWorldIndex(findingPath[i], x, y);
				break;
			}
		}
		//cout << "���� ��ġ->" << x << "," << y << endl;
	}

	void FindPrevPath(int posIdx, vector<int>& path) {
		auto findNextPosIter = closePoints.find(posIdx);
		if (findNextPosIter == closePoints.end()) {
			return;
		}
		path.push_back(posIdx);
		FindPrevPath(findNextPosIter->second.parent, path);
	}

	/// <summary>
	/// �� ����Ŭ ���� ã���ϴ�.
	/// </summary>
	/// <returns>���� ã�ų� ã�� ������ ������ true�� ��ȯ�մϴ�</returns>
	FindStatus FindWayOnce() {
		if(findStatus == FindStatus::FoundWay || openPoints.empty()){
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
		if(CanMoveStraight(curPoint.index)){
			if(targetIdx != curPoint.index){
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
		int offset[] = { -1,1,-worldWidth,worldWidth };
		int offsetX[] = { -1,1,0,0 };
		int offsetY[] = { 0,0,-1,1 };
		for (int i = 0; i < 4; i++) {
			auto movedIdx = curPoint.index + offset[i];
			auto movedX = curX + offsetX[i];
			auto movedY = curY + offsetY[i];
			if (movedX < 0 || worldWidth <= movedX ||
				movedY < 0 || worldHeight <= movedY ||
				worldManager->GetCollidable(movedIdx) || 
				closePoints.find(movedIdx) != closePoints.end()) {
				// �� �� ���� ���̸� �ѱ��
				continue;
			}
			if (targetIdx == movedIdx) {
				// ��ã�� ����
				closePoints[movedIdx] = PathPoint(movedIdx, 0, 0, curPoint.index);
				goalPosIdx = movedIdx;
				findStatus = FindStatus::FoundWay;
				return findStatus;
			}
			auto hcost = GetCostToTarget(movedIdx);
			auto gcost = 1 + curPoint.gcost;
			auto fcost = gcost + hcost;
			auto movedIndexGcost = openPoints.Find(movedIdx);
			if (movedIndexGcost) {
				if (movedIndexGcost->gcost < gcost) {
					openPoints.Replace(PathPoint(movedIdx, fcost, gcost, curPoint.index));
				}
			} else {
				openPoints.emplace(PathPoint(movedIdx, fcost, gcost, curPoint.index));
			}
		}
		findStatus = FindStatus::Finding;
		return findStatus;
	}
private:
	int GetCostToTarget(int index) {
		int x, y;
		worldManager->GetPosFormWorldIndex(index, x, y);
		return GetCostToTarget(x, y);
	}

	int GetCostToTarget(int x, int y) {
		return abs(x - targetX) + abs(y - targetY);
	}
	
	/// <summary>
	/// �ش� index ��ġ���� ���������� �����Ÿ��� �� �� �ִ��� ��ȯ�մϴ�.
	/// </summary>
	bool CanMoveStraight(int index) {
		int x, y;
		worldManager->GetPosFormWorldIndex(index, x, y);
		return CanMoveStraight(x, y);
	}

	/// <summary>
	/// x, y ��ġ���� ���������� �����Ÿ��� �� �� �ִ��� ��ȯ�մϴ�.
	/// </summary>
	bool CanMoveStraight(int x, int y) {
		return CanMoveStraight(x, y, targetX, targetY);
	}

	/// <summary>
	/// x, y ��ġ���� ���������� �����Ÿ��� �� �� �ִ��� ��ȯ�մϴ�.
	/// </summary>
	bool CanMoveStraight(int x, int y, int targetX, int targetY) {
		while (x != targetX || y != targetY) {
			auto xOff = targetX - x;
			auto yOff = targetY - y;
			if (abs(xOff) > abs(yOff)) {
				xOff > 0 ? ++x : xOff < 0 ? --x : x;
			} else {
				yOff > 0 ? ++y : yOff < 0 ? --y : y;
			}
			if (worldManager->GetCollidable(x, y)) {
				return false;
			}
		}
		
		return true;
	}
};

bool IsMonster(int id) {
	return MONSTER_ID_START < id;
}

bool IsNpc(int id) {
	return NPC_ID_START < id;
}

void AddSendingData(int targetId, void* buf);

void UpdateSector(int actorId, int prevX, int prevY, int x, int y);

bool DebugLua(lua_State* L, int err) {
	if (err) {
		auto msg = lua_tostring(L, -1);
		cout << "LUA error in exec: " << msg << endl;
		lua_pop(L, 1);
		
		switch (err) {
		case LUA_ERRFILE:
			printf("couldn't open the given file\n");
			exit(-1);
		case LUA_ERRSYNTAX:
			printf("syntax error during pre-compilation\n");
			luaL_traceback(L, L, msg, 1);
			printf("%s\n", lua_tostring(L, -1));
			exit(-1);
		case LUA_ERRMEM:
			printf("memory allocation error\n");
			exit(-1);
		case LUA_ERRRUN:
		{
			const char* msg = lua_tostring(L, -1);
			luaL_traceback(L, L, msg, 1);
			printf("LUA_ERRRUN %s\n", lua_tostring(L, -1));
			exit(-1);
		}
		case LUA_ERRERR:
			printf("error while running the error handler function\n");
			exit(-1);
		default:
			printf("unknown error %i\n", err);
			exit(-1);
		}
		return false;
	}
	return true;
}

bool CallLuaFunction(lua_State* L, int argCount, int resultCount) {
	//lua_getglobal(L, "debug");
	//lua_getfield(L, -1, "traceback");
	//lua_remove(L, -2);
	//int errindex = -argCount - 2;
	//lua_insert(L, errindex);
	int errindex = 0;
	const int res = lua_pcall(L, argCount, resultCount, errindex);
	return DebugLua(L, res);
}

class Player : public Actor {
protected:
	int hp, maxHp = 5;
	int level;
	int exp;
	int damage;
	vector<int> attackViewList;
	Session session;	// �÷��̾�� ���� ���� ���� ������ ���
	
	Player(int id);

public:
	static Player* Get(int id) {
		return new Player(id);
	}
	
	static int GetNewId(SOCKET socket) {
		for (int i = 1; i <= MAX_PLAYER; ++i) {
			auto actor = GetPlayer(i);
			auto session = actor->GetSession();
			if (PLST_FREE == session->state) {
				session->state = PLST_CONNECTED;
				lock_guard<mutex> lg{ session->socketLock };
				session->socket = socket;
				actor->name[0] = 0;
				return i;
			}
		}
		return -1;
	}

	void Init() override {
		Actor::Init();
		hp = maxHp;
		level = 1;
		exp = 0;
		damage = 1;
	}
	
	void SetPos(int x, int y) override;
	void SendStatChange() override;

	virtual void Disconnect() {
		auto actor = GetPlayer(id);
		if (session.state == PLST_FREE) {
			return;
		}
		session.state = PLST_FREE;
		closesocket(session.socket);

		{
			lock_guard<mutex> lock(session.recvedBufLock);
			session.recvedBuf.clear();
		}
		session.bufOverManager.ClearSendingData();
		actor->RemoveFromAll();
	}

	virtual void Attack() {
		viewSetLock.lock();
		int size = viewSet.size();
		attackViewList.resize(size);
		std::copy(viewSet.begin(), viewSet.end(), attackViewList.begin());
		viewSetLock.unlock();
		for (auto i = 0; i < size;) {
			if (IsMonster(attackViewList[i])) {
				auto actor = GetActor(attackViewList[i]);
				if (abs(x - actor->x) + abs(y - actor->y) <= 1) {
					actor->TakeDamage(id);
				}
			}
			++i;
		}
	}

	bool TakeDamage(int attackerId) override;

	void Die() override {
		//Actor::Die();
		//SendRemoveActor(id, id); // ������ ���ϰ� ��ġ �ű�� ����ġ �� HP ȸ���ؼ� ������ġ��
		SetPos(initX, initY);
		hp = maxHp;
		exp = exp >> 1;
		SendStatChange();
	}

	void AddToViewSet(int otherId) override {
		viewSetLock.lock();
		if (0 == viewSet.count(id)) {
			viewSet.insert(otherId);
			viewSetLock.unlock();
			SendAddActor(otherId);
			return;
		}
		viewSetLock.unlock();
	}

	void RemoveFromViewSet(int otherId) override {
		viewSetLock.lock();
		if (0 != viewSet.count(otherId)) {
			viewSet.erase(otherId);
			viewSetLock.unlock();
			SendRemoveActor(otherId);
			return;
		}
		viewSetLock.unlock();
	}

	void SendChat(int senderId, const char* mess) {
		sc_packet_chat p;
		p.id = senderId;
		p.size = sizeof(p);
		p.type = SC_CHAT;
		strcpy_s(p.message, mess);
		AddSendingData(id, &p);
	}

	void SendMove(int p_id) {
		sc_packet_position p;
		p.id = p_id;
		p.size = sizeof(p);
		p.type = SC_POSITION;
		auto actor = GetActor(p_id);
		p.x = actor->x;
		p.y = actor->y;
		p.move_time = actor->moveTime;
		AddSendingData(id, &p);
	}

	void SendAddActor(int addedId) {
		sc_packet_add_object p;
		p.id = addedId;
		p.size = sizeof(p);
		p.type = SC_ADD_OBJECT;
		auto actor = GetActor(addedId);
		p.x = actor->x;
		p.y = actor->y;
		p.obj_class = 1;
		p.HP = 1;
		p.LEVEL = 1;
		p.EXP = 1;
		strcpy_s(p.name, actor->name);
		AddSendingData(id, &p);
	}

	void SendRemoveActor(int removeTargetId) {
		sc_packet_remove_object p;
		p.id = removeTargetId;
		p.size = sizeof(p);
		p.type = SC_REMOVE_OBJECT;
		AddSendingData(id, &p);
	}

	void SendChangedStat(int statChangedId, int hp, int level, int exp) {
		sc_packet_stat_change p;
		p.id = statChangedId;
		p.size = sizeof(p);
		p.type = SC_STAT_CHANGE;
		p.HP = hp;
		p.LEVEL = level;
		p.EXP = exp;
		AddSendingData(id, &p);
	}
	
	void CallRecv() const;

	MiniOver* GetOver() override {
		return session.bufOverManager.Get();
	}
	Session* GetSession() { return &session; }
	int GetHp() override { return hp; }
	int GetLevel() override { return level; }
	int GetExp() override { return exp; }
	int GetDamage() override { return damage; }
	void SetExp(int exp) override {
		auto level = GetLevel();
		auto levelMinusOne = GetLevel() - 1;
		auto requireExp = levelMinusOne * 100 + 100;
		if(requireExp < exp){
			SetLevel(GetLevel() + 1);
			exp -= requireExp;
		}
		this->exp = exp;
		SendStatChange();
	}
	void SetLevel(int level) override { this->level = level; }
	void SetHp(int hp) override { this->hp = hp; }

private:
	/// <summary>
	/// �� �����忡���� ȣ��ȵǱ⶧���� lock �Ȱɾ��
	/// </summary>
	/// <param name="id"></param>
	void ProcessPacket(unsigned char* buf);
};

void send_login_ok_packet(int targetId) {
	sc_packet_login_ok p;
	p.size = sizeof(p);
	p.type = SC_LOGIN_OK;
	p.id = targetId;
	auto actor = GetPlayer(targetId);
	p.x = actor->x;
	p.y = actor->y;
	p.HP = actor->GetHp();
	p.LEVEL = actor->GetLevel();
	p.EXP = actor->GetExp();
	AddSendingData(targetId, &p);
}

class NonPlayer : public Actor {
	MiniOver miniOver;	// Npc ID�� PostQueuedCompletionStatus ȣ���Ҷ� ���
protected:
	/// <summary>
	/// luaLock���� ����ְ� ���
	/// </summary>
	lua_State* L;		// Npc�� ���
	mutex luaLock;		// Npc�� ���
	
	NonPlayer(int id) : Actor(id) {}

	void InitLua(const char* path);

	void Init() override {
		Actor::Init();
		moveTime = 0;
		memset(&miniOver.over, 0, sizeof(miniOver.over));
		InitSector();
	}

	void OnNearActorWithPlayerMove(int actorId) override {
		lock_guard<mutex> lock(luaLock);
		lua_getglobal(L, "OnNearActorWithPlayerMove");
		lua_pushnumber(L, actorId);
		CallLuaFunction(L, 1, 0);
	}

	bool TakeDamage(int attackerId) override {
		lock_guard<mutex> lock(luaLock);
		lua_getglobal(L, "TakeDamage");
		lua_pushinteger(L, attackerId);
		lua_pushinteger(L, GetActor(attackerId)->GetDamage());
		CallLuaFunction(L, 2, 1);
		auto result = lua_toboolean(L, -1);
		lua_pop(L, 1);
		return result;
	}

	void AddToViewSet(int otherId) override;

	void Die() override;

	void LuaLock() override {
		luaLock.lock();
	}
	void LuaUnLock() override {
		luaLock.unlock();
	}
	void SetPos(int x, int y) override;
	MiniOver* GetOver() override {
		memset(&miniOver.over, 0, sizeof(miniOver.over));
		return &miniOver;
	}
	int GetHpWithoutLock() const {
		lua_getglobal(L, "mHp");
		int value = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return value;
	}
	int GetLevelWithoutLock() const {
		lua_getglobal(L, "mLevel");
		int value = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return value;
	}
	int GetExpWithoutLock() const {
		lua_getglobal(L, "mExp");
		int value = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return value;
	}
	int GetHp() override {
		lock_guard<mutex> lock(luaLock);
		return GetHpWithoutLock();
	}
	int GetLevel() override {
		lock_guard<mutex> lock(luaLock);
		return GetLevelWithoutLock();
	}
	int GetExp() override {
		lock_guard<mutex> lock(luaLock);
		return GetExpWithoutLock();
	}
	int GetDamage() override {
		lock_guard<mutex> lock(luaLock);
		lua_getglobal(L, "mDamage");
		int value = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return value;
	}
	void SetExp(int exp) override {
		lock_guard<mutex> lock(luaLock);
		lua_pushnumber(L, exp);
		lua_setglobal(L, "mExp");
	}
	void SetLevel(int level) override {
		lock_guard<mutex> lock(luaLock);
		lua_pushnumber(L, level);
		lua_setglobal(L, "mLevel");
	}
	void SetHp(int hp) override {
		lock_guard<mutex> lock(luaLock);
		lua_pushnumber(L, hp);
		lua_setglobal(L, "mHp");
	}
private:
	void SendStatChange() override {
		{
			lock_guard<mutex> lock(viewSetLock);
			for (auto viewId : viewSet) {
				_ASSERT(viewId != id);
				if (IsNpc(viewId)) {
					continue;
				}
				GetPlayer(viewId)->SendChangedStat(id, GetHpWithoutLock(), GetLevelWithoutLock(), GetExpWithoutLock());
			}
		}
		if (GetHpWithoutLock() == 0) {
			TimerQueueManager::Add(id, 1, nullptr, [this](int) {
				Die();
				});
		}
	}
	
	void OnNearActorWithSelfMove(int actorId) override {
		lock_guard<mutex> lock(luaLock);
		lua_getglobal(L, "OnNearActorWithSelfMove");
		lua_pushnumber(L, actorId);
		CallLuaFunction(L, 1, 0);
	}
};

int LuaAddEventSendMess(lua_State* L) {
	int recverId = lua_tonumber(L, -4);
	int senderId = lua_tonumber(L, -3);
	const char* mess = lua_tostring(L, -2);
	int delay = lua_tonumber(L, -1);
	lua_pop(L, 5);
	TimerQueueManager::Add(recverId, delay, nullptr, [=](int size) {
		GetPlayer(recverId)->SendChat(senderId, mess);
		});
	return 1;
}

int LuaTakeDamage(lua_State* L) {
	int obj_id = lua_tointeger(L, -2);
	int attackerId = lua_tointeger(L, -1);
	lua_pop(L, 3);
	GetActor(obj_id)->TakeDamage(attackerId);
	return 1;
}

int LuaGetX(lua_State* L) {
	int obj_id = lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = GetActor(obj_id)->x;
	lua_pushinteger(L, x);
	return 1;
}
int LuaGetY(lua_State* L) {
	int obj_id = lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = GetActor(obj_id)->y;
	lua_pushinteger(L, y);
	return 1;
}

int LuaSetPos(lua_State* L) {
	int obj_id = lua_tointeger(L, -3);
	int x = lua_tointeger(L, -2);
	int y = lua_tointeger(L, -1);
	lua_pop(L, 3);
	GetActor(obj_id)->LuaUnLock();
	GetActor(obj_id)->SetPos(x, y);
	GetActor(obj_id)->LuaLock();
	return 1;
}

int LuaSendMess(lua_State* L) {
	int recverId = lua_tointeger(L, -3);
	int senderId = lua_tointeger(L, -2);
	const char* mess = lua_tostring(L, -1);
	lua_pop(L, 4);
	GetPlayer(recverId)->SendChat(senderId, mess);
	return 1;
}
int LauGetHp(lua_State* L) {
	int targetId = lua_tointeger(L, -1);
	lua_pop(L, 2);
	lua_pushinteger(L, GetActor(targetId)->GetHp());
	return 1;
}
int LuaGetLevel(lua_State* L) {
	int targetId = lua_tointeger(L, -1);
	lua_pop(L, 2);
	lua_pushinteger(L, GetActor(targetId)->GetLevel());
	return 1;
}
int LuaGetExp(lua_State* L) {
	int targetId = lua_tointeger(L, -1);
	lua_pop(L, 2);
	lua_pushinteger(L, GetActor(targetId)->GetExp());
	return 1;
}
int LuaSendStatChange(lua_State* L);
int LuaPrint(lua_State* L) {
	const char* mess = lua_tostring(L, -1);
	lua_pop(L, 2);
	cout << mess;
	return 1;
}

int LuaAddEventNpcRandomMove(lua_State* L) {
	int p_id = lua_tointeger(L, -2);
	int delay = lua_tointeger(L, -1);
	lua_pop(L, 3);
	TimerQueueManager::Add(p_id, delay, nullptr, [=](int size) {
		GetActor(p_id)->Move(static_cast<DIRECTION>(rand() % 4));
		});
	return 1;
}
int LuaIsMovable(lua_State* L) {
	int x = lua_tointeger(L, -2);
	int y = lua_tointeger(L, -1);
	lua_pop(L, 3);
	lua_pushboolean(L, !worldManager.GetCollidable(x, y));
	return 1;
}

class Npc : public NonPlayer {
protected:
	Npc(int id) : NonPlayer(id) {
	}
public:
	static Npc* Get(int id) {
		return new Npc(id);
	}

	void Init() override {
		sprintf_s(name, "N%d", id);
		x = rand() % WORLD_WIDTH;
		y = rand() % WORLD_HEIGHT;
		NonPlayer::Init();
		InitLua("npc.lua");
	}

	void Update() override;
};

class Monster : public NonPlayer {
protected:
	PathFindHelper* pathFindHelper = nullptr; // TODO ��� NPC�� ���� ã�°� �ƴϱ⿡ Ǯ������ �����ͼ� ���鼭 �޸� ���డ��
	mutex monsterLock;
	
	Monster(int id) : NonPlayer(id) {
		pathFindHelper = PathFindHelper::Get(&worldManager);
	}
public:
	static Monster* Get(int id) {
		return new Monster(id);
	}

	void Init() override {
		sprintf_s(name, "M%d", id);
		x = rand() % WORLD_WIDTH;
		y = rand() % WORLD_HEIGHT;
		WorldManager::FileMonster monster;
		monster.x = x;
		monster.y = y;
		monster.hp = GetHpWithoutLock();
		monster.level = GetLevelWithoutLock();
		monster.exp = GetExpWithoutLock();
		monster.damage = GetDamage();
		monster.name = name;
		monster.script = "Monster.lua";
		monster.findPlayerAct = WorldManager::EFindPlayerAct::Peace;
		monster.soloMove = WorldManager::ESoloMove::Fixing;
		Init(monster);
	}

	void Init(WorldManager::FileMonster& monster) {
		strcpy_s(this->name, monster.name.c_str());
		this->x = monster.x;
		this->y = monster.y;
		NonPlayer::Init();
		InitLua(monster.script.c_str());
		SetHp(monster.hp);
		SetLevel(monster.level);
		SetExp(monster.exp);
		SetDamage(monster.damage);

		lua_pushnumber(L, static_cast<int>(monster.findPlayerAct));
		lua_setglobal(L, "mFindPlayerAct");
		lua_pushnumber(L, static_cast<int>(monster.soloMove));
		lua_setglobal(L, "mSoloMove");
		lua_getglobal(L, "InitStat");
		CallLuaFunction(L, 0, 0);
	}

	void SetPathStartAndTarget(int startX, int startY, int targetX, int targetY) const {
		pathFindHelper->SetStartAndTarget(startX, startY, targetX, targetY);
	}

	void Update() override;

	/// <summary>
	/// �ش� �������� �� ĭ �̵��մϴ�.
	/// </summary>
	/// <param name="direcX"></param>
	/// <param name="direcY"></param>
	void MoveTo(int targetX, int targetY) {
		int tx = x, ty = y;
		auto offX = targetX - tx;
		auto offY = targetY - ty;
		if(abs(offX) > abs(offY)){
			offX > 0 ? ++tx : offX < 0 ? --tx : tx;
		}else{
			offY > 0 ? ++ty : offY < 0 ? --ty : ty;
		}
		SetPos(tx, ty);
	}

	void SetDamage(int damage) {
		lock_guard<mutex> lock(luaLock);
		lua_pushnumber(L, damage);
		lua_setglobal(L, "mDamage");
	}

	void Die() override {
		NonPlayer::Die();
	}
};

std::string random_string(std::size_t length) {
	const std::string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	std::random_device random_device;
	std::mt19937 generator(random_device());
	std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

	std::string random_string;

	for (std::size_t i = 0; i < length; ++i) {
		random_string += CHARACTERS[distribution(generator)];
	}

	return random_string;
}

void display_error(const char* msg, int errNum) {
#ifdef DISPLAYLOG
	WCHAR* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, errNum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	cout << msg;
	wcout << lpMsgBuf << endl;
	LocalFree(lpMsgBuf);
#endif
}

void AddSendingData(int targetId, void* buf) {
	_ASSERT(SERVER_ID < targetId && !IsNpc(targetId));
	auto& manager = GetPlayer(targetId)->GetSession()->bufOverManager;
	TimerQueueManager::Add(targetId, 12, [&]() {
		return manager.HasSendData();
		}, [&, targetId](int size) {
			manager.SendAddedData(targetId);
		});
	manager.AddSendingData(buf);
}

int LuaSendStatChange(lua_State* L) {
	int targetId = lua_tonumber(L, -5);
	int orderId = lua_tonumber(L, -4);
	int hp = lua_tonumber(L, -3);
	int level = lua_tonumber(L, -2);
	int exp = lua_tonumber(L, -1);
	lua_pop(L, 5);
	auto actor = GetActor(targetId);
	if(orderId != targetId){ // �ڱ� �ڽŰ� ���� �� �ʿ����. �׸��� �����Ϸ��ϸ� ���� ������ ��
		actor->SetHp(hp);
		actor->SetLevel(level);
		actor->SetExp(exp);
	}
	actor->SendStatChange();
	return 1;
}

void BufOver::Recycle() {
	_ASSERT(manager != nullptr&&"manager�� null�Դϴ�");
	manager->Recycle(this);
}

void BufOverManager::SendAddedData(int playerId) {
	sendingDataLock.lock();
	auto totalSendDataSize = sendingData.size();
	if (totalSendDataSize == 0) {
		sendingDataLock.unlock();
		return;
	}

	auto& copiedSendingData = *GetSendingDataQueue();
	copiedSendingData.resize(totalSendDataSize);
	memcpy(&copiedSendingData[0], &sendingData[0], totalSendDataSize);
	sendingData.clear();
	sendingDataLock.unlock();

	auto session = GetPlayer(playerId)->GetSession();
	auto sendDataBegin = &copiedSendingData[0];

	while (0 < totalSendDataSize) {
		auto sendDataSize = min(MAX_BUFFER, (int)totalSendDataSize);
		auto exOver = Get();
		exOver->callback = [](int size) { };
		memcpy(exOver->packetBuf, sendDataBegin, sendDataSize);
		exOver->wsabuf[0].buf = reinterpret_cast<CHAR*>(exOver->packetBuf);
		exOver->wsabuf[0].len = sendDataSize;
		int ret;
		{
		lock_guard <mutex> lock{ session->socketLock };
		ret = WSASend(session->socket, exOver->wsabuf, 1, NULL, 0, &exOver->over, NULL);// TODO GetQueuedCompletionStatus���� �� ���´��� Ȯ���ؼ� �Ȱ����� �� ������
		}
		if (0 != ret) {
			auto err = WSAGetLastError();
			if (WSA_IO_PENDING != err) {
				display_error("WSASend : ", WSAGetLastError());
				GetPlayer(playerId)->Disconnect();
				return;
			}
		}
		totalSendDataSize -= sendDataSize;
		sendDataBegin += sendDataSize;
	}
	RecycleSendingDataQueue(&copiedSendingData);
}

void TimerQueueManager::Do() {
	using namespace chrono;
	for (;;){
		timerLock.lock();
		if (false == timerQueue.empty() && timerQueue.top().startTime < system_clock::now()){
			TimerEvent ev = timerQueue.top();
			timerQueue.pop();
			timerLock.unlock();
			auto actor = GetActor(ev.object);
			if (!actor->isActive ||
				(ev.checkCondition != nullptr && !ev.checkCondition())){
				continue;
			}

			auto over = actor->GetOver();
			over->callback = ev.callback;
			PostQueuedCompletionStatus(hIocp, 1, ev.object, &over->over);
		}
		else{
			timerLock.unlock();
			this_thread::sleep_for(10ms);
		}
	}
}

bool CanSee(int id1, int id2) {
	auto actor1 = GetActor(id1);
	auto actor2 = GetActor(id2);
	int ax = actor1->x;
	int ay = actor1->y;
	int bx = actor2->x;
	int by = actor2->y;
	return HALF_VIEW_RADIUS >=
		abs(ax - bx) + abs(ay - by);
}
/// <summary>
/// p_id�� ���� �÷��̾ �����ϰ� �ش� ��ǥ ���Ϳ� �ִ� �÷��̾ main_sector�� �߰��մϴ�.
/// </summary>
/// <param name="y"></param>
/// <param name="id"></param>
/// <param name="x"></param>
/// <param name="returnSector"></param>
void AddSectorPlayersToMainSector(int id, int y, int x, vector<int>& returnSector) {
	lock_guard<mutex> lock(world_sector[y][x].sectorLock);
	auto& otherSet = world_sector[y][x].sector;
	auto isNpc = IsNpc(id);
	for (auto otherId : otherSet) {
		auto otherIsNpc = IsNpc(otherId);
		if ((isNpc && otherIsNpc) || // p_id�� NPC�� NPC�� �ʿ����. ����� �ν��Ѵ�
			otherId == id || // �� id�� ���� �ȴ�
			!CanSee(id, otherId)) { // �Ⱥ��̴°� �ʿ����.
			continue;
		}
		returnSector.push_back(otherId);
	}
}

/// <summary>
/// ���Ϳ� �ִ� ���� ���͸� �����մϴ�. p_id�� �������� �ʽ��ϴ�.
/// npc id�� ȣ���ϸ� npc�� �ɷ��� �����մϴ�.
/// </summary>
/// <param name="p_id"></param>
/// <returns></returns>
vector<int>& GetIdFromOverlappedSector(int p_id) {
	auto actor = GetActor(p_id);
	auto amINpc = IsNpc(p_id);
	int y = actor->y;
	int x = actor->x;
	auto sectorY = y / WORLD_SECTOR_SIZE;
	auto sectorX = x / WORLD_SECTOR_SIZE;
	auto& mainSector = world_sector[sectorY][sectorX];

	auto& returnSector = actor->selectedSector;
	lock_guard<mutex> lock(actor->selectedSectorLock);
	returnSector.clear();
	mainSector.sectorLock.lock();
	for (auto otherId : mainSector.sector) {
		if ((amINpc && IsNpc(otherId)) || 
			otherId == p_id ||
			!CanSee(otherId, p_id)) {
			continue;
		}
		returnSector.push_back(otherId);
	}
	mainSector.sectorLock.unlock();

	auto sectorViewFrustumTop = (y - HALF_VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sectorViewFrustumBottom = (y + HALF_VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sectorViewFrustumLeft = (x - HALF_VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sectorViewFrustumRight = (x + HALF_VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto isLeft = sectorViewFrustumLeft != sectorX && 0 < sectorX;
	auto isRight = sectorViewFrustumRight != sectorX && sectorX < static_cast<int>(world_sector[sectorViewFrustumTop].size() - 1);
	if (sectorViewFrustumTop != sectorY && 0 < sectorY) {
		AddSectorPlayersToMainSector(p_id, sectorViewFrustumTop, sectorX, returnSector);
		if (isLeft) {
			AddSectorPlayersToMainSector(p_id, sectorViewFrustumTop, sectorViewFrustumLeft, returnSector);
		} else if (isRight) {
			AddSectorPlayersToMainSector(p_id, sectorViewFrustumTop, sectorViewFrustumRight,
				returnSector);
		}
	} else if (sectorViewFrustumBottom != sectorY && sectorY < static_cast<int>(world_sector.size() - 1)) {
		AddSectorPlayersToMainSector(p_id, sectorViewFrustumBottom, sectorX, returnSector);
		if (isLeft) {
			AddSectorPlayersToMainSector(p_id, sectorViewFrustumBottom, sectorViewFrustumLeft,
				returnSector);
		} else if (isRight) {
			AddSectorPlayersToMainSector(p_id, sectorViewFrustumBottom, sectorViewFrustumRight,
				returnSector);
		}
	}
	if (isLeft) {
		AddSectorPlayersToMainSector(p_id, sectorY, sectorViewFrustumLeft, returnSector);
	} else if (isRight) {
		AddSectorPlayersToMainSector(p_id, sectorY, sectorViewFrustumRight, returnSector);
	}
	return returnSector;
}

void AddNpcLoopEvent(int id) {
	TimerQueueManager::Add(id, 1000, nullptr, [=](int) {
		AddNpcLoopEvent(id);
		GetActor(id)->Update();
		});
}

void WakeUpNpc(int id) {
	auto actor = GetActor(id);
	if (actor->isActive == false) {
		bool old_state = false;
		if (true == atomic_compare_exchange_strong(&actor->isActive, &old_state, true)) {
			cout << "wake up id: " << id << " is active: "<< actor->isActive << endl;
			AddNpcLoopEvent(id);
		}
	}
}

void SleepNpc(int id) {
	auto actor = GetActor(id);
	actor->isActive = false;
	TimerQueueManager::RemoveAll(id);
}

Monster* WorldManager::GetMonster(int id) {
	auto monster = Monster::Get(id);
	auto& fileMonster = monsters[id-MONSTER_ID_START];
	monster->Init(fileMonster);
	return monster;
}

void Actor::RemoveFromViewSet(int otherId) {
	viewSetLock.lock();
	viewSet.erase(otherId);
	viewSetLock.unlock();
}

void Actor::Die() {
	RemoveFromAll();
}

void Actor::RemoveFromAll() {
	viewSetLock.lock();
	for (auto viewId : viewSet) {
		if (!IsNpc(viewId)) {
			GetPlayer(viewId)->SendRemoveActor(id);
		}
		GetActor(viewId)->RemoveFromViewSet(id);
	}
	viewSet.clear();
	viewSetLock.unlock();
	RemoveFromSector();
	TimerQueueManager::RemoveAll(id);
}

void Actor::SendStatChange() {
	{
		lock_guard<mutex> lock(viewSetLock);
		for (auto viewId : viewSet){
			_ASSERT(viewId != id);
			if(IsNpc(viewId)){
				continue;
			}
			GetPlayer(viewId)->SendChangedStat(id, GetHp(), GetLevel(), GetExp());
		}
	}
	if(GetHp() == 0){
		TimerQueueManager::Add(id, 1, nullptr, [this](int) {
			Die();
			});
	}
}

void UpdateSector(int actorId, int prevX, int prevY, int x, int y) {
	const auto sectorViewFrustumX = prevX / WORLD_SECTOR_SIZE;
	const auto sectorViewFrustumY = prevY / WORLD_SECTOR_SIZE;
	const auto newSectorViewFrustumX = x / WORLD_SECTOR_SIZE;
	const auto newSectorViewFrustumY = y / WORLD_SECTOR_SIZE;
	if (y != prevY || x != prevX) {
		// ���� ���Ͷ� �ٸ��� �ٸ� ���ͷ� �̵��� ����
		{
			lock_guard<mutex> gl2{ world_sector[sectorViewFrustumY][sectorViewFrustumX].sectorLock };
			world_sector[sectorViewFrustumY][sectorViewFrustumX].sector.erase(actorId);
		}
		{
			lock_guard<mutex> gl2{ world_sector[newSectorViewFrustumY][newSectorViewFrustumX].sectorLock };
			world_sector[newSectorViewFrustumY][newSectorViewFrustumX].sector.insert(actorId);
		}
	}
}

void NonPlayer::InitLua(const char* path) {
	L = luaL_newstate();
	luaL_openlibs(L);
	int luaError = luaL_loadfile(L, path);
	
	CallLuaFunction(L, 0, 0);

	// API�Լ����� ���ɸ� LUA�Լ��� ȣ���ϱ⿡ ���� �ʿ����.
	lua_register(L, "LuaSetPos", LuaSetPos);
	lua_register(L, "LuaGetX", LuaGetX);
	lua_register(L, "LuaGetY", LuaGetY);
	lua_register(L, "LuaSendMess", LuaSendMess);
	lua_register(L, "LuaSendStatChange", LuaSendStatChange);
	lua_register(L, "LauGetHp", LauGetHp);
	lua_register(L, "LuaGetLevel", LuaGetLevel);
	lua_register(L, "LuaGetExp", LuaGetExp);
	lua_register(L, "LuaTakeDamage", LuaTakeDamage);
	lua_register(L, "LuaPrint", LuaPrint);
	lua_register(L, "LuaAddEventNpcRandomMove", LuaAddEventNpcRandomMove);
	lua_register(L, "LuaAddEventSendMess", LuaAddEventSendMess);
	lua_register(L, "LuaIsMovable", LuaIsMovable);
	lua_register(L, "LuaSetPathStartAndTarget", [](lua_State* L) {
		auto monsterId = lua_tointeger(L, -3);
		auto startX = GetActor(monsterId)->x;
		auto startY = GetActor(monsterId)->y;
		auto targetX = lua_tointeger(L, -2);
		auto targetY = lua_tointeger(L, -1);
		lua_pop(L, 4);
		// TODO Actor�� ��ã�� �� �� ������ �����ؾ���
		GetMonster(monsterId)->SetPathStartAndTarget(startX, startY, targetX, targetY);
		return 1;
	});

	lua_getglobal(L, "SetId");
	lua_pushinteger(L, id);
	CallLuaFunction(L, 1, 0);

}

void NonPlayer::AddToViewSet(int otherId) {
	Actor::AddToViewSet(otherId);
	OnNearActorWithSelfMove(otherId);
	WakeUpNpc(id);
}

void NonPlayer::Die() {
	Actor::Die();
	SleepNpc(id);
}

void NonPlayer::SetPos(int x, int y) {
	if(this->x == x && this->y == y){
		return;
	}
	auto prevX = this->x;
	auto prevY = this->y;
	this->x = x;
	this->y = y;
	{
		lock_guard<mutex> lockLua(luaLock);
		lua_pushinteger(L, this->x);
		lua_setglobal(L, "mX");
		lua_pushinteger(L, this->y);
		lua_setglobal(L, "mY");
	}

	UpdateSector(id, prevX, prevY, x, y);
	
	newViewList = GetIdFromOverlappedSector(id);
#ifdef NPCLOG
		lock_guard<mutex> coutLock{ coutMutex };
		cout << "npc[" << id << "] (" << x << "," << y << ") �̵� " << oldViewList.size() << "��[";
		for (auto tViewId : oldViewList) {
			cout << tViewId << ",";
		}
		cout << "] -> " << new_vl.size() << "��[";
		for (auto tViewId : new_vl) {
			cout << tViewId << ",";
		}
		cout << "]���� ����";
#endif // NPCLOG

	lock_guard<mutex> lock(oldNewViewListLock);
	CopyViewSetToOldViewList();
	if (newViewList.empty()){
		SleepNpc(id); // �ƹ��� ������ �����Ƿ� ��ħ
#ifdef NPCLOG
			cout << " & �ƹ����Ե� �Ⱥ����� ��ħ" << endl;
#endif
		for (auto otherId : oldViewList) {
			GetActor(otherId)->RemoveFromViewSet(id);
		}
		return;
	}
	for (auto otherId : newViewList) {
		if (oldViewList.end() == std::find(oldViewList.begin(), oldViewList.end(), otherId)) {
			// �÷��̾��� �þ߿� ����
			AddToViewSet(otherId);
			GetPlayer(otherId)->AddToViewSet(id);
#ifdef NPCLOG
			cout << " &[" << otherId << "]���� ����";
#endif
		} else {
			// �÷��̾ ��� ��������.
#ifdef NPCLOG
			cout << " &[" << otherId << "]���� ��ġ ����";
#endif
			OnNearActorWithSelfMove(otherId);
			GetPlayer(otherId)->SendMove(id);
		}
	}
	for (auto otherId : oldViewList) {
		if (newViewList.end() == std::find(newViewList.begin(), newViewList.end(), otherId)) {
			RemoveFromViewSet(otherId);
			GetActor(otherId)->RemoveFromViewSet(id);
#ifdef NPCLOG
			cout << " &[" << otherId << "]���Լ� �����";
#endif
		}
	}
#ifdef NPCLOG
		cout << endl;
#endif
}

void Npc::Update() {
	Move(static_cast<DIRECTION>(rand() % 4));
}

void Monster::Update() {
	lock_guard<mutex> lock(monsterLock);
	{
		lock_guard<mutex> lockLua(luaLock);
		lua_getglobal(L, "Tick");
		{
			lock_guard<mutex> lock(viewSetLock);
			if(viewSet.empty()){
				SleepNpc(id);
				lua_pop(L, 1);
				return;
			}
			asTable(L, viewSet.begin(), viewSet.end());
		}
		CallLuaFunction(L, 1, 0);
	}

	PathFindHelper::FindStatus findWay;
	for (size_t i = 0; i < VIEW_RADIUS; i++) {
	//for (size_t i = 0; i < 100; i++) {
		findWay = pathFindHelper->FindWayOnce();
		if (PathFindHelper::FindStatus::Finding < findWay) { // ã�ų� ã���� ������
			break;
		}
	}
	if(findWay == PathFindHelper::FindStatus::FoundWay || 
		findWay == PathFindHelper::FindStatus::Finding){
		int tx = x;
		int ty = y;
		pathFindHelper->GetNextPos(tx, ty);
		MoveTo(tx, ty);
	}
}

Player::Player(int id): Actor(id) {
	actors[id] = this;
	session.state = PLST_FREE;
	session.recvOver.callback = [this](int bufSize) {
		auto exOver = session.recvOver;
		unsigned char* recvPacketPtr;
		auto totalRecvBufSize = static_cast<unsigned char>(bufSize);
		if (session.recvedBufSize > 0) {
			// �����ִ°� ������ �����ִ� ����Ŷ�� ó��
			const unsigned char prevPacketSize = session.recvedBuf[0];
			const unsigned char splitBufSize = prevPacketSize - session.recvedBufSize;
			session.recvedBuf.resize(prevPacketSize);
			memcpy(1 + &session.recvedBuf.back(), exOver.packetBuf, splitBufSize);
			recvPacketPtr = &session.recvedBuf[0];
			ProcessPacket(recvPacketPtr);
			recvPacketPtr = exOver.packetBuf + splitBufSize;
			totalRecvBufSize -= splitBufSize;
		} else {
			recvPacketPtr = exOver.packetBuf;
		}

		for (unsigned char recvPacketSize = recvPacketPtr[0];
			0 < totalRecvBufSize;
			recvPacketSize = recvPacketPtr[0]) {
			ProcessPacket(recvPacketPtr);
			totalRecvBufSize -= recvPacketSize;
			recvPacketPtr += recvPacketSize;
		}
		{
			lock_guard<mutex> lock(session.recvedBufLock);
			if (0 < totalRecvBufSize) {
				session.recvedBuf.resize(totalRecvBufSize);
				session.recvedBufSize = totalRecvBufSize;
				memcpy(&session.recvedBuf[0], recvPacketPtr, totalRecvBufSize);
			} else {
				session.recvedBuf.clear();
				session.recvedBufSize = 0;
			}
		}
		CallRecv();
	};
}

void Player::SetPos(int x, int y) {
	auto prevX = this->x;
	auto prevY = this->y;
	this->x = x;
	this->y = y;
	
	SendMove(id);

	UpdateSector(id, prevX, prevY, x, y);
	
	auto&& new_vl = GetIdFromOverlappedSector(id);

	lock_guard<mutex> lock(oldNewViewListLock);
	CopyViewSetToOldViewList();
	
	for (auto otherId : new_vl) {
		if (oldViewList.end() == std::find(oldViewList.begin(), oldViewList.end(), otherId)) {
			//1. ���� �þ߿� ������ �÷��̾�
			AddToViewSet(otherId);
			GetActor(otherId)->AddToViewSet(id);
		} else {
			//2. ���� �þ߿��� �ְ� �� �þ߿��� �ִ� ���
			if (!IsNpc(otherId)) {
				GetPlayer(otherId)->SendMove(id);
			} else {
#ifdef PLAYERLOG
				{
					lock_guard<mutex> coutLock{ coutMutex };
					cout << "�÷��̾�[" << id << "]�� " << actor->x << "," << actor->y << " �������� npc[" << pl << "]�� ����" << endl;
				}
#endif
				GetActor(otherId)->OnNearActorWithPlayerMove(id);
			}
		}
	}
	for (auto otherId : oldViewList) {
		if (new_vl.end() == std::find(new_vl.begin(), new_vl.end(), otherId)) {
			// ���� �þ߿� �־��µ� �� �þ߿� ���� ���
			RemoveFromViewSet(otherId);
			GetActor(otherId)->RemoveFromViewSet(id);
		}
	}
}

void Player::SendStatChange() {
	SendChangedStat(id, hp, level, exp);
	Actor::SendStatChange();
}

bool Player::TakeDamage(int attackerId) {
	hp -= damage;
	if (hp < 0){
		hp = 0;
		SendStatChange();
		return true;
	}
	SendStatChange();
	return false;
}

void Player::ProcessPacket(unsigned char* buf) {
	switch (buf[1]){
	case CS_LOGIN: {
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(buf);
		session.state = PLST_INGAME;
		{
			// ��ġ �̸� �ʱ�ȭ
			lock_guard<mutex> lock{session.socketLock};
			strcpy_s(name, packet->player_id);

			x = rand() % WORLD_WIDTH;
			y = rand() % WORLD_HEIGHT;
#ifdef PLAYER_NOT_RANDOM_SPAWN
			x = 0;
			y = 0;
#endif
			Init();
		}
		isActive = true;
		InitSector();
		send_login_ok_packet(id);

		auto& selected_sector = GetIdFromOverlappedSector(id);

		viewSetLock.lock();
		for (auto otherId : selected_sector){
			viewSet.insert(otherId);
		}
		viewSetLock.unlock();
		for (auto otherId : selected_sector){
			auto other = GetActor(otherId);
			other->AddToViewSet(id);
			SendAddActor(otherId);
			if (!IsNpc(otherId)){
				GetPlayer(otherId)->SendAddActor(id);
			}
		}
		break;
	}
	case CS_MOVE: {
		auto packet = reinterpret_cast<cs_packet_move*>(buf);
		auto actor = GetActor(id);
		actor->moveTime = packet->move_time;
		Move(static_cast<DIRECTION>(packet->direction));
		break;
	}
	case CS_ATTACK: {
		auto* packet = reinterpret_cast<cs_packet_attack*>(buf);
		Attack();
		break;
	}
	case CS_CHAT: {
		auto* packet = reinterpret_cast<cs_packet_chat*>(buf);
		lock_guard<mutex> lock(viewSetLock);
		for (auto viewId : viewSet){
			if(!IsNpc(viewId)){
				GetPlayer(viewId)->SendChat(id, packet->message);
			}
		}
		break;
	}
	default: {
		cout << "Unknown Packet Type from Client[" << id;
		cout << "] Packet Type [" << +buf[1] << "]";
		while (true);
	}
	}
}

void Player::CallRecv() const {
	auto session = GetPlayer(id)->GetSession();
	auto& recvOver = session->recvOver;
	recvOver.wsabuf[0].buf =
		reinterpret_cast<char*>(recvOver.packetBuf);
	recvOver.wsabuf[0].len = MAX_BUFFER;
	memset(&recvOver.over, 0, sizeof(recvOver.over));
	DWORD r_flag = 0;
	int ret = WSARecv(session->socket, recvOver.wsabuf, 1, NULL, &r_flag, &recvOver.over, NULL);
	if (0 != ret){
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("WSARecv : ", WSAGetLastError());
	}
}

void Worker(HANDLE hIocp) {
	while (true) {
		DWORD recvBufSize;
		ULONG_PTR recvKey;
		WSAOVERLAPPED* recvOver;

		BOOL ret = GetQueuedCompletionStatus(hIocp, &recvBufSize,
			&recvKey, &recvOver, INFINITE);

		int key = static_cast<int>(recvKey);
		if (FALSE == ret) {
			display_error("GQCS : ", WSAGetLastError());
			if (SERVER_ID == key) {
				cout << "����Ű�� Key�� �Ѿ��" << endl;
				exit(-1);
			}
			GetPlayer(key)->Disconnect();
		}
		if ((key != SERVER_ID) && (0 == recvBufSize)) {
			GetPlayer(key)->Disconnect();
			continue;
		}
		auto over = reinterpret_cast<MiniOver*>(reinterpret_cast<char*>(recvOver)-sizeof(void*)); // vtable �� ���� ������ ����Ʈ�� ��ŭ ��

		over->callback(recvBufSize);
		over->Recycle();
	}
}

void CallAccept(AcceptOver& over, SOCKET listenSocket) {
	memset(&over.over, 0, sizeof(over.over));
	const SOCKET cSocket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	over.cSocket = cSocket;
	const auto ret = AcceptEx(listenSocket, cSocket,
	                          &over.wsabuf[0], 0, 32, 32, nullptr, &over.over);
	if (FALSE == ret) {
		const int err_num = WSAGetLastError();
		if (err_num != WSA_IO_PENDING)
			display_error("AcceptEx Error", err_num);
	}
}

int main() {
	worldManager.Generate();
	worldManager.Load();
	for (int i = SERVER_ID + 1; i <= MAX_PLAYER; ++i) {
		Player::Get(i);
	}
	for (int i = MAX_PLAYER + 1; i <= MAX_USER; i++) {
		if(i < MONSTER_ID_START){
			Npc::Get(i)->Init();
		}else{
			worldManager.GetMonster(i);
		}
	}

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	wcout.imbue(locale("korean"));
	hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listenSocket), hIocp, SERVER_ID, 0);
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	::bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
	listen(listenSocket, SOMAXCONN);
	
	AcceptOver accept_over;

	accept_over.callback = [&](int) {
		int acceptId = Player::GetNewId(accept_over.cSocket);
		if (-1 != acceptId) {
			auto session = GetPlayer(acceptId)->GetSession();
			CreateIoCompletionPort(
				reinterpret_cast<HANDLE>(session->socket), hIocp, acceptId, 0);
			GetPlayer(acceptId)->CallRecv();
		} else {
			closesocket(accept_over.cSocket);
			cout << "�ο� �� ����" << endl;
		}
		CallAccept(accept_over, listenSocket);
	};
	CallAccept(accept_over, listenSocket);

	cout << "���� ����" << endl;

	thread timer_thread(TimerQueueManager::Do);
	vector <thread> worker_threads;
	for (int i = 0; i < 3; ++i)
		worker_threads.emplace_back(Worker, hIocp);
	for (auto& th : worker_threads)
		th.join();
	closesocket(listenSocket);
	WSACleanup();
}
