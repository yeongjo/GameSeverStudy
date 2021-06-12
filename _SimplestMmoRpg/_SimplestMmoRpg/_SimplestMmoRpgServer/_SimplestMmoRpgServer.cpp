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
#include <queue>
#include <unordered_set>
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

Actor* GetActor(int id) {
	return actors[id];
}

Player* GetPlayer(int id) {
	_ASSERT(0 < id && id < NPC_ID_START);
	return reinterpret_cast<Player*>(actors[id]);
}
struct Actor {
	int		id;
	char name[MAX_NAME];
	short	x, y;
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
	
	/// <summary>
	/// luaLock���� ����ְ� ���
	/// </summary>
	lua_State* L;		// Npc�� ���
	mutex luaLock;		// Npc�� ���

protected:
	Actor(int id) {
		this->id = id;
		actors[id] = this;
	}

public:

	virtual void Init() {}

	virtual void Update() {}

	virtual void Move(DIRECTION dir) {}

	virtual void Interact(Actor* interactor) {}

	virtual void OnNearActorWithPlayerMove(int actorId) {}

	virtual void OnNearActorWithSelfMove(int actorId) {}

	virtual void AddToViewSet(int otherId) {
		lock_guard<mutex> lock(viewSetLock);
		viewSet.insert(otherId);
	}

	virtual void RemoveFromViewSet(int otherId); // TODO ���߿� RemoveAll ������ lock���� ���� �������°� �پ��

	virtual void Die();

	virtual void SendStatChange();

	virtual void SetPos(int x, int y) {
		this->x = x;
		this->y = y;
	}

	/// <summary>
	/// ������ false ����
	/// </summary>
	/// <param name="attackerId"></param>
	/// <returns></returns>
	virtual bool TakeDamage(int attackerId) {
		return true;
	}

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
		lock_guard<mutex> lock(oldNewViewListLock);
		lock_guard<mutex> lock2(viewSetLock);
		oldViewList.resize(viewSet.size());
		std::copy(viewSet.begin(), viewSet.end(), oldViewList.begin());
		return oldViewList;
	}
};

bool IsMonster(int id) {
	return MONSTER_ID_START < id;
}

bool IsNpc(int id) {
	return NPC_ID_START < id;
}

void AddSendingData(int targetId, void* buf);

void send_chat(int receiverId, int senderId, const char* mess) {
	sc_packet_chat p;
	p.id = senderId;
	p.size = sizeof(p);
	p.type = SC_CHAT;
	strcpy_s(p.message, mess);
	AddSendingData(receiverId, &p);
}

void send_move_packet(int c_id, int p_id) {
	sc_packet_position p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = SC_POSITION;
	auto actor = GetActor(p_id);
	p.x = actor->x;
	p.y = actor->y;
	p.move_time = actor->moveTime;
	AddSendingData(c_id, &p);
}

void send_add_object(int targetId, int addedId) {
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
	AddSendingData(targetId, &p);
}

void send_remove_object(int receiverId, int removeTargetId) {
	sc_packet_remove_object p;
	p.id = removeTargetId;
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	AddSendingData(receiverId, &p);
}

void send_stat_change(int receiverId, int statChangedId, int hp, int level, int exp) {
	sc_packet_stat_change p;
	p.id = statChangedId;
	p.size = sizeof(p);
	p.type = SC_STAT_CHANGE;
	p.HP = hp;
	p.LEVEL = level;
	p.EXP = exp;
	AddSendingData(receiverId, &p);
}

int API_take_damage(lua_State* L) {
	int obj_id = lua_tonumber(L, -2);
	int attackerId = lua_tonumber(L, -1);
	lua_pop(L, 3);
	GetActor(obj_id)->TakeDamage(attackerId);
	return 1;
}

int API_get_x(lua_State* L) {
	int obj_id = lua_tonumber(L, -1);
	lua_pop(L, 2);
	int x = GetActor(obj_id)->x;
	lua_pushnumber(L, x);
	return 1;
}
int API_get_y(lua_State* L) {
	int obj_id = lua_tonumber(L, -1);
	lua_pop(L, 2);
	int y = GetActor(obj_id)->y;
	lua_pushnumber(L, y);
	return 1;
}

int API_set_pos(lua_State* L) {
	int obj_id = lua_tonumber(L, -3);
	int x = lua_tonumber(L, -2);
	int y = lua_tonumber(L, -1);
	lua_pop(L, 3);
	GetActor(obj_id)->SetPos(x, y);
	return 1;
}

int API_send_mess(lua_State* L) {
	int recverId = lua_tonumber(L, -3);
	int senderId = lua_tonumber(L, -2);
	const char* mess = lua_tostring(L, -1);
	lua_pop(L, 4);
	send_chat(recverId, senderId, mess);
	return 1;
}
int API_get_hp(lua_State* L) {
	int targetId = lua_tonumber(L, -1);
	lua_pop(L, 2);
	lua_pushinteger(L, GetActor(targetId)->GetHp());
	return 1;
}
int API_get_level(lua_State* L) {
	int targetId = lua_tonumber(L, -1);
	lua_pop(L, 2);
	lua_pushinteger(L, GetActor(targetId)->GetLevel());
	return 1;
}
int API_get_exp(lua_State* L) {
	int targetId = lua_tonumber(L, -1);
	lua_pop(L, 2);
	lua_pushinteger(L, GetActor(targetId)->GetExp());
	return 1;
}
int API_send_stat_change(lua_State* L);
int API_print(lua_State* L) {
	const char* mess = lua_tostring(L, -1);
	lua_pop(L, 2);
	cout << mess;
	return 1;
}

int API_add_event_npc_random_move(lua_State* L) {
	int p_id = lua_tonumber(L, -2);
	int delay = lua_tonumber(L, -1);
	lua_pop(L, 3);
	TimerQueueManager::Add(p_id, delay, nullptr, [=](int size) {
		GetActor(p_id)->Move(static_cast<DIRECTION>(rand() % 4));
	});
	return 1;
}

int API_add_event_send_mess(lua_State* L) {
	int recverId = lua_tonumber(L, -4);
	int senderId = lua_tonumber(L, -3);
	const char* mess = lua_tostring(L, -2);
	int delay = lua_tonumber(L, -1);
	lua_pop(L, 5);
	TimerQueueManager::Add(recverId, delay, nullptr, [=](int size) {
		send_chat(recverId, senderId, mess);
		});
	return 1;
}
void UpdateSector(int actorId, int prevX, int prevY, int x, int y);

bool CallLuaFunction(lua_State* L, int argCount, int resultCount) {
	const int res = lua_pcall(L, argCount, resultCount, 0);
	if (0 != res) {
		cout << "LUA error in exec: " << lua_tostring(L, -1) << endl;
		lua_pop(L, 1);
		return false;
	}
	return true;
}

class Player : public Actor {
protected:
	int hp = 10;
	int level = 1;
	int exp = 0;
	int damage = 1;
	vector<int> attackViewList;
	Session session;	// �÷��̾�� ���� ���� ���� ������ ���
	
	Player(int id);

public:
	static Player* Get(int id) {
		return new Player(id);
	}
	void Move(DIRECTION dir) override;
	void SetPos(int x, int y) override;
	void SendStatChange() override;

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

	void AddToViewSet(int otherId) override {
		viewSetLock.lock();
		if (0 == viewSet.count(id)) {
			viewSet.insert(otherId);
			viewSetLock.unlock();
			send_add_object(id, otherId);
			return;
		}
		viewSetLock.unlock();
	}

	void RemoveFromViewSet(int otherId) override {
		viewSetLock.lock();
		if (0 != viewSet.count(otherId)) {
			viewSet.erase(otherId);
			viewSetLock.unlock();
			send_remove_object(id, otherId);
			return;
		}
		viewSetLock.unlock();
	}

	MiniOver* GetOver() override {
		return session.bufOverManager.Get();
	}
	Session* GetSession() { return &session; }
	int GetHp() override { return hp; }
	int GetLevel() override { return level; }
	int GetExp() override { return exp; }
	int GetDamage() override { return damage; }
	void SetExp(int exp) override { this->exp = exp; }
	void SetLevel(int level) override { this->level = level; }
	void SetHp(int hp) override { this->hp = hp; }
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
	NonPlayer(int id) : Actor(id) {}
	
	void InitLua(const char* path) {
		L = luaL_newstate();
		luaL_openlibs(L);
		luaL_loadfile(L, path);
		CallLuaFunction(L, 0, 0);
		
		lua_register(L, "API_set_pos", API_set_pos);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
		lua_register(L, "API_send_mess", API_send_mess);
		lua_register(L, "API_send_stat_change", API_send_stat_change);
		lua_register(L, "API_get_hp", API_get_hp);
		lua_register(L, "API_get_level", API_get_level);
		lua_register(L, "API_get_exp", API_get_exp);
		lua_register(L, "API_take_damage", API_take_damage);
		lua_register(L, "API_print", API_print);
		lua_register(L, "API_add_event_npc_random_move", API_add_event_npc_random_move);
		lua_register(L, "API_add_event_send_mess", API_add_event_send_mess);

		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, id);
		CallLuaFunction(L, 1, 0);

	}

	void Init() override {
		moveTime = 0;
		isActive = false;
		memset(&miniOver.over, 0, sizeof(miniOver.over));
		InitSector();
	}

	void OnNearActorWithPlayerMove(int actorId) override {
		lock_guard<mutex> lock(luaLock);
		lua_getglobal(L, "on_near_actor_with_player_move");
		lua_pushnumber(L, actorId);
		CallLuaFunction(L, 1, 0);
	}

	void OnNearActorWithSelfMove(int actorId) override {
		lock_guard<mutex> lock(luaLock);
		lua_getglobal(L, "on_near_actor_with_self_move");
		lua_pushnumber(L, actorId);
		CallLuaFunction(L, 1, 0);
	}

	bool TakeDamage(int attackerId) override {
		lock_guard<mutex> lock(luaLock);
		lua_getglobal(L, "take_damage");
		lua_pushinteger(L, attackerId);
		lua_pushinteger(L, GetActor(attackerId)->GetDamage());
		CallLuaFunction(L, 2, 0);
		auto result = lua_toboolean(L, -1);
		lua_pop(L, 1);
		return result;
	}

	void Die() override;

	void SetPos(int x, int y) override;
	MiniOver* GetOver() override {
		return &miniOver;
	}
	int GetHp() override {
		lua_getglobal(L, "get_hp");
		CallLuaFunction(L, 0, 1);
		int value = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return value;
	}
	int GetLevel() override {
		lua_getglobal(L, "get_level");
		CallLuaFunction(L, 0, 1);
		int value = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return value;
	}
	int GetExp() override {
		lua_getglobal(L, "get_exp");
		CallLuaFunction(L, 0, 1);
		int value = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return value;
	}
	int GetDamage() override {
		lua_getglobal(L, "get_damage");
		CallLuaFunction(L, 0, 1);
		int value = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return value;
	}
	void SetExp(int exp) override {
		lua_pushnumber(L, exp);
		lua_setglobal(L, "my_exp");
	}
	void SetLevel(int level) override {
		lua_pushnumber(L, level);
		lua_setglobal(L, "my_level");
	}
	void SetHp(int hp) override {
		lua_pushnumber(L, hp);
		lua_setglobal(L, "my_hp");
	}
private:
	void InitSector() {
		auto sectorViewFrustumX = x / WORLD_SECTOR_SIZE;
		auto sectorViewFrustumY = y / WORLD_SECTOR_SIZE;
		world_sector[sectorViewFrustumY][sectorViewFrustumX].sector.insert(id);
	}
};

class Npc : public NonPlayer {
protected:
	Npc(int id) : NonPlayer(id) {
		sprintf_s(name, "N%d", id);
		x = rand() % WORLD_WIDTH;
		y = rand() % WORLD_HEIGHT;
		NonPlayer::Init();
		InitLua("npc.lua");
	}
public:
	static Npc* Get(int id) {
		return new Npc(id);
	}

	void Update() override;
	void Move(DIRECTION dir) override;
};

class Monster : public NonPlayer {
protected:
	Monster(int id) : NonPlayer(id) {
		sprintf_s(name, "M%d", id);
		x = rand() % WORLD_WIDTH;
		y = rand() % WORLD_HEIGHT;
		NonPlayer::Init();
		InitLua("Monster.lua");
	}
public:
	static Monster* Get(int id) {
		return new Monster(id);
	}

	void Update() override;

	void Die() override {
		NonPlayer::Die();

	}
};

void Disconnect(int playerId);

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

int API_send_stat_change(lua_State* L) {
	int targetId = lua_tonumber(L, -4);
	int hp = lua_tonumber(L, -3);
	int level = lua_tonumber(L, -2);
	int exp = lua_tonumber(L, -1);
	lua_pop(L, 5);
	auto actor = GetActor(targetId);
	actor->SetHp(hp);
	actor->SetLevel(level);
	actor->SetExp(exp);
	actor->SendStatChange();
	return 1;
}

void BufOver::Recycle() {
	_ASSERT(manager != nullptr, "manager�� null�Դϴ�");
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
				Disconnect(playerId);
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
			if (ev.checkCondition != nullptr && !ev.checkCondition()){
				continue;
			}

			auto over = GetActor(ev.object)->GetOver();
			over->callback = ev.callback;
			memset(&over->over, 0, sizeof(over->over));
			PostQueuedCompletionStatus(hIocp, 1, ev.object, &over->over);
		}
		else{
			timerLock.unlock();
			this_thread::sleep_for(10ms);
		}
	}
}

void CallRecv(int key) {
	auto session = GetPlayer(key)->GetSession();
	auto& recvOver = session->recvOver;
	recvOver.wsabuf[0].buf =
		reinterpret_cast<char*>(recvOver.packetBuf);
	recvOver.wsabuf[0].len = MAX_BUFFER;
	memset(&recvOver.over, 0, sizeof(recvOver.over));
	DWORD r_flag = 0;
	int ret = WSARecv(session->socket, recvOver.wsabuf, 1, NULL, &r_flag, &recvOver.over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("WSARecv : ", WSAGetLastError());
	}
}

int GetNewPlayerId(SOCKET socket) {
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

bool CanSee(int id1, int id2) {
	auto actor1 = GetActor(id1);
	auto actor2 = GetActor(id2);
	int ax = actor1->x;
	int ay = actor1->y;
	int bx = actor2->x;
	int by = actor2->y;
	return VIEW_RADIUS >=
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
	for (auto id : mainSector.sector) {
		if ((!amINpc || (amINpc && !IsNpc(id))) &&
			id != p_id &&
			CanSee(id, p_id)) {
			returnSector.push_back(id);
		}
	}
	mainSector.sectorLock.unlock();

	auto sectorViewFrustumTop = (y - VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sectorViewFrustumBottom = (y + VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sectorViewFrustumLeft = (x - VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sectorViewFrustumRight = (x + VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	if (sectorViewFrustumTop != sectorY && 0 < sectorY) {
		AddSectorPlayersToMainSector(p_id, sectorViewFrustumTop, sectorX, returnSector);
		if (sectorViewFrustumLeft != sectorX && 0 < sectorX) {
			AddSectorPlayersToMainSector(p_id, sectorViewFrustumTop, sectorViewFrustumLeft, returnSector);
		} else if (sectorViewFrustumRight != sectorX && sectorX < static_cast<int>(world_sector[
			sectorViewFrustumTop].size() - 1)) {
			AddSectorPlayersToMainSector(p_id, sectorViewFrustumTop, sectorViewFrustumRight,
				returnSector);
		}
	} else if (sectorViewFrustumBottom != sectorY && sectorY < static_cast<int>(world_sector.size() - 1)) {
		AddSectorPlayersToMainSector(p_id, sectorViewFrustumBottom, sectorX, returnSector);
		if (sectorViewFrustumLeft != sectorX && 0 < sectorX) {
			AddSectorPlayersToMainSector(p_id, sectorViewFrustumBottom, sectorViewFrustumLeft,
				returnSector);
		} else if (sectorViewFrustumRight != sectorX && sectorX < static_cast<int>(world_sector[
			sectorViewFrustumBottom].size() - 1)) {
			AddSectorPlayersToMainSector(p_id, sectorViewFrustumBottom, sectorViewFrustumRight,
				returnSector);
		}
	}
	if (sectorViewFrustumLeft != sectorX && 0 < sectorX) {
		AddSectorPlayersToMainSector(p_id, sectorY, sectorViewFrustumLeft, returnSector);
	} else if (sectorViewFrustumRight != sectorX && sectorX < static_cast<int>(world_sector[sectorY].size() - 1)) {
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
			//cout << "wake up id: " << id << " is active: "<< actor->isActive << endl;
			AddNpcLoopEvent(id);
		}
	}
}

void SleepNpc(int id) {
	auto actor = GetActor(id);
	actor->isActive = false;
	TimerQueueManager::RemoveAll(id);
	//cout << "sleep id: " << id << endl;
}

/// <summary>
/// �� �����忡���� ȣ��ȵǱ⶧���� lock �Ȱɾ��
/// </summary>
/// <param name="id"></param>
void ProcessPacket(int playerId, unsigned char* buf) {
	switch (buf[1]) {
	case CS_LOGIN: {
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(buf);
		auto logined_actor = GetPlayer(playerId);
		auto session = logined_actor->GetSession();
		session->state = PLST_INGAME;
		{
			// ��ġ �̸� �ʱ�ȭ
			lock_guard <mutex> lock{ session->socketLock };
			strcpy_s(logined_actor->name, packet->player_id);
			//session->x = 1; session->y = 1;
			logined_actor->x = rand() % WORLD_WIDTH;
			logined_actor->y = rand() % WORLD_HEIGHT;
		}
		{
			// ���Ϳ� �߰�
			auto& sector = world_sector[logined_actor->y / WORLD_SECTOR_SIZE][logined_actor->x / WORLD_SECTOR_SIZE];
			lock_guard <mutex> lock{ sector.sectorLock };
			sector.sector.insert(playerId);
		}
		send_login_ok_packet(playerId);

		auto& selected_sector = GetIdFromOverlappedSector(playerId);

		logined_actor->viewSetLock.lock();
		for (auto id : selected_sector) {
			logined_actor->viewSet.insert(id);
		}
		logined_actor->viewSetLock.unlock();
		for (auto id : selected_sector) {
			auto other = GetActor(id);
			other->viewSetLock.lock();
			other->viewSet.insert(playerId);
			other->viewSetLock.unlock();
			send_add_object(playerId, id);
			if (!IsNpc(id)) {
				send_add_object(id, playerId);
			} else {
				WakeUpNpc(other->id);
			}
		}
		break;
	}
	case CS_MOVE: {
		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(buf);
		auto actor = GetActor(playerId);
		actor->moveTime = packet->move_time;
		//actor->moveTime = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
		GetActor(playerId)->Move((DIRECTION)packet->direction);
		break;
	}
	case CS_ATTACK: {
		cs_packet_attack* packet = reinterpret_cast<cs_packet_attack*>(buf);
		GetPlayer(playerId)->Attack();
		break;
	}
	default: {
		cout << "Unknown Packet Type from Client[" << playerId;
		cout << "] Packet Type [" << +buf[1] << "]";
		while (true);
	}
	}
}

void Disconnect(int playerId) {
	auto actor = GetPlayer(playerId);
	auto session = actor->GetSession();
	if (session->state == PLST_FREE) {
		return;
	}
	closesocket(session->socket);
	session->state = PLST_FREE;

	session->bufOverManager.ClearSendingData();
	{
		lock_guard<mutex> lock(session->recvedBufLock);
		session->recvedBuf.clear();
	}
	// remove from sector
	int y;
	int x;
	y = actor->y; // TODO y �Ҵ��ϴ� �߿� �ٸ� �����忡�� ���� ���ϸ� �˰��鰡�µ�
	x = actor->x;
	auto sector_y = y / WORLD_SECTOR_SIZE;
	auto sector_x = x / WORLD_SECTOR_SIZE;
	auto& main_sector = world_sector[sector_y][sector_x];
	main_sector.sectorLock.lock();
	main_sector.sector.erase(playerId);
	main_sector.sectorLock.unlock();
	//

	actor->viewSetLock.lock();
	vector<int> oldViewList(actor->viewSet.begin(), actor->viewSet.end());
	actor->viewSet.clear();
	actor->viewSetLock.unlock();
	auto& m_old_view_list = oldViewList;
	for (auto pl : m_old_view_list) {
		if (IsNpc(pl)) {
			lock_guard<mutex> lock(GetActor(pl)->viewSetLock);
			GetActor(pl)->viewSet.erase(actor->id);
			continue;
		}
		auto otherPlayer = GetPlayer(playerId);
		auto session2 = otherPlayer->GetSession();
		if (PLST_INGAME == session2->state) {
			send_remove_object(otherPlayer->id, playerId);
			lock_guard<mutex> lock(otherPlayer->viewSetLock);
			otherPlayer->viewSet.erase(actor->id);
		}
	}
}

void Actor::RemoveFromViewSet(int otherId) {
	viewSetLock.lock();
	viewSet.erase(otherId);
	viewSetLock.unlock();
}

void Actor::Die() {
	// TODO ���� �׾��µ� �þ߿��� �Ȼ����
	isActive = false;
	viewSetLock.lock();
	for (auto viewId : viewSet){
		if (!IsNpc(viewId)){
			send_remove_object(viewId, id);
		}
	}
	viewSet.clear();
	viewSetLock.unlock();
}

void Actor::SendStatChange() {
	{
		lock_guard<mutex> lock(viewSetLock);
		for (auto viewId : viewSet){
			_ASSERT(viewId != id);
			if(IsNpc(viewId)){
				continue;
			}
			send_stat_change(viewId, id, GetHp(), GetLevel(), GetExp());
		}
	}
	if(GetHp() == 0){
		Die();
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

void NonPlayer::Die() {
	Actor::Die();
	SleepNpc(id);
}

void NonPlayer::SetPos(int x, int y) {
	auto prevX = this->x;
	auto prevY = this->y;
	this->x = x;
	this->y = y;
	UpdateSector(id, prevX, prevY, x, y);

	CopyViewSetToOldViewList();

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

	if (newViewList.empty()){
		SleepNpc(id); // �ƹ��� ������ �����Ƿ� ��ħ
#ifdef NPCLOG
			cout << " & �ƹ����Ե� �Ⱥ����� ��ħ" << endl;
#endif
		for (auto otherId : oldViewList) {
			auto actor = GetActor(otherId);
			actor->RemoveFromViewSet(id);
		}
		return;
	}
	for (auto otherId : newViewList) {
		if (oldViewList.end() == std::find(oldViewList.begin(), oldViewList.end(), otherId)) {
			// �÷��̾��� �þ߿� ����
			AddToViewSet(otherId);
			OnNearActorWithSelfMove(otherId);
			auto otherActor = GetActor(otherId);
			otherActor->AddToViewSet(id);
#ifdef NPCLOG
			cout << " &[" << pl << "]���� ����";
#endif
		} else {
			// �÷��̾ ��� ��������.
#ifdef NPCLOG
			cout << " &[" << pl << "]���� ��ġ ����";
#endif

			OnNearActorWithSelfMove(otherId);
			send_move_packet(otherId, id);
		}
	}
	for (auto otherId : oldViewList){
		if (newViewList.end() == std::find(newViewList.begin(), newViewList.end(), otherId)){
			RemoveFromViewSet(otherId);
			auto actor = GetActor(otherId);
			actor->RemoveFromViewSet(id);
#ifdef NPCLOG
				cout << " &[" << pl << "]���Լ� �����";
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

void Npc::Move(DIRECTION dir) {
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
	SetPos(x, y);
}

void Monster::Update() {
	// TODO ���ڰ� ���ƴٴ�
	//if (GetIdFromOverlappedSector(id).empty()){
	//	SleepNpc(id);
	//}
	lua_getglobal(L, "tick");
	{
		lock_guard<mutex> lock(viewSetLock);
		asTable(L, viewSet.begin(), viewSet.end());
	}
	CallLuaFunction(L, 1, 0);
}

Player::Player(int id): Actor(id) {
	actors[id] = this;
	isActive = true;
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
			ProcessPacket(this->id, recvPacketPtr);
			recvPacketPtr = exOver.packetBuf + splitBufSize;
			totalRecvBufSize -= splitBufSize;
		} else {
			recvPacketPtr = exOver.packetBuf;
		}

		for (unsigned char recvPacketSize = recvPacketPtr[0];
			0 < totalRecvBufSize;
			recvPacketSize = recvPacketPtr[0]) {
			ProcessPacket(this->id, recvPacketPtr);
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
		CallRecv(this->id);
	};
}

void Player::Move(DIRECTION dir) {
	auto x = this->x;
	auto y = this->y;
	switch (dir){
	case D_N: if (y > 0) y--;
		break;
	case D_S: if (y < (WORLD_HEIGHT - 1)) y++;
		break;
	case D_W: if (x > 0) x--;
		break;
	case D_E: if (x < (WORLD_WIDTH - 1)) x++;
		break;
	}
	SetPos(x, y);
}

void Player::SetPos(int x, int y) {
	auto prevX = this->x;
	auto prevY = this->y;
	this->x = x;
	this->y = y;
	
	send_move_packet(id, id);

	UpdateSector(id, prevX, prevY, x, y);
	
	CopyViewSetToOldViewList();
	
	auto&& new_vl = GetIdFromOverlappedSector(id);

	for (auto otherId : new_vl) {
		if (oldViewList.end() == std::find(oldViewList.begin(), oldViewList.end(), otherId)) {
			//1. ���� �þ߿� ������ �÷��̾�
			AddToViewSet(otherId);

			auto other_actor = GetActor(otherId);
			other_actor->AddToViewSet(id);
			if (IsNpc(otherId)) {
				WakeUpNpc(otherId);
#ifdef PLAYERLOG
				{
					lock_guard<mutex> coutLock{ coutMutex };
					cout << "�÷��̾�[" << id << "]�� " << actor->x << "," << actor->y << " �������� npc[" << pl << "]�� ���� �׸��� ����" << endl;
				}
#endif
			}
		} else {
			//2. ���� �þ߿��� �ְ� �� �þ߿��� �ִ� ���
			if (false == IsNpc(otherId)) {
				send_move_packet(id, otherId);
			} else {
				// NPC��� OP_PLAYER_APPROACH ȣ��
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

			auto other_actor = GetActor(otherId);
			other_actor->RemoveFromViewSet(id);
		}
	}
}

void Player::SendStatChange() {
	send_stat_change(id, id, hp, level, exp);
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

void worker(HANDLE h_iocp, SOCKET l_socket) {
	while (true) {
		DWORD recvBufSize;
		ULONG_PTR recvKey;
		WSAOVERLAPPED* recvOver;

		BOOL ret = GetQueuedCompletionStatus(h_iocp, &recvBufSize,
			&recvKey, &recvOver, INFINITE);

		int key = static_cast<int>(recvKey);
		if (FALSE == ret) {
			display_error("GQCS : ", WSAGetLastError());
			if (SERVER_ID == key) {
				cout << "����Ű�� Key�� �Ѿ��" << endl;
				exit(-1);
			}
			Disconnect(key);
		}
		if ((key != SERVER_ID) && (0 == recvBufSize)) {
			Disconnect(key);
			continue;
		}
		auto over = reinterpret_cast<MiniOver*>(reinterpret_cast<char*>(recvOver)-sizeof(void*)); // vtable �� ���� ������ ����Ʈ�� ��ŭ ��
		cout << over << endl;

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
	for (int i = SERVER_ID + 1; i <= MAX_PLAYER; ++i) {
		Player::Get(i);
	}
	for (int i = MAX_PLAYER + 1; i <= MAX_USER; i++) {
		if(i < MONSTER_ID_START){
			Npc::Get(i);
		}else{
			if(i%2 == 0){
				Monster::Get(i);
			}else{
				Monster::Get(i);
			}
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
	cout << &accept_over << endl;

	accept_over.callback = [&](int) {
		int acceptId = GetNewPlayerId(accept_over.cSocket);
		if (-1 != acceptId) {
			auto session = GetPlayer(acceptId)->GetSession();
			CreateIoCompletionPort(
				reinterpret_cast<HANDLE>(session->socket), hIocp, acceptId, 0);
			CallRecv(acceptId);
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
		worker_threads.emplace_back(worker, hIocp, listenSocket);
	for (auto& th : worker_threads)
		th.join();
	closesocket(listenSocket);
	WSACleanup();
}
