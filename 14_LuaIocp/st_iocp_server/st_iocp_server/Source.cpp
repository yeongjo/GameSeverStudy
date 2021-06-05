#include <iostream>
#include <unordered_map>
#include <thread>
#include <vector>
#include <mutex>
#include <array>
#include <queue>
#include <unordered_set>
struct ExOverManager;
using namespace std;
#include <WS2tcpip.h>
#include <MSWSock.h>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
//#define DISPLAYLOG
//#define PLAYERLOG
//#define NPCLOG
mutex coutMutex;

#pragma comment(lib, "lua54.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")

#include "protocol.h"

//#define MAX_BUFFER 1024
#define NPC_ID_START NPC_START
#define MAX_PLAYER (NPC_START)
#define MAX_NPC (MAX_USER-NPC_ID_START)
#define WORLD_WIDTH WORLD_X_SIZE
#define WORLD_HEIGHT WORLD_Y_SIZE
#define VIEW_RADIUS 5
#define MESSAGE_MAX_BUFFER MAX_NAME

constexpr int32_t ceil_const(float num) {
	return (static_cast<float>(static_cast<int32_t>(num)) == num)
		? static_cast<int32_t>(num)
		: static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
}

constexpr int WORLD_SECTOR_SIZE = (VIEW_RADIUS + 2);
constexpr int WORLD_SECTOR_X_COUNT = ceil_const(WORLD_WIDTH / (float)WORLD_SECTOR_SIZE);
constexpr int WORLD_SECTOR_Y_COUNT = ceil_const(WORLD_HEIGHT / (float)WORLD_SECTOR_SIZE);

enum EOpType { OP_RECV, OP_SEND, OP_ACCEPT, OP_RANDOM_MOVE_LOOP, OP_RANDOM_MOVE, OP_ATTACK, OP_PLAYER_APPROACH, OP_SEND_MESSAGE, OP_DELAY_SEND };
enum EPlayerState { PLST_FREE, PLST_CONNECTED, PLST_INGAME };

struct NpcOver {
	WSAOVERLAPPED	m_over;
	EOpType			m_op;
};
struct ExOver : public NpcOver {
	WSABUF			wsabuf[1];
	unsigned char	packetBuf[MAX_BUFFER];
	SOCKET			csocket;					// OP_ACCEPT에서만 사용
private:
	ExOverManager* manager;
public:
	ExOver() = default;
	ExOver(ExOverManager* manager) : manager(manager) {}

	void Recycle();
	ExOverManager* GetManager() {
		return manager;
	}
};

/// <summary>
/// Get()으로 ExOver 쓰고
/// ExOver->Recycle() 호출하면 됨
/// </summary>
struct ExOverManager {
private:
	vector<ExOver*> managedExOvers;
	mutex managedExOversLock;
	vector<unsigned char> sendingData;
	mutex sendingDataLock;
	static size_t EX_OVER_SIZE_INCREMENT;

public:
	ExOverManager() {
		managedExOvers.resize(2);
		auto size = managedExOvers.size();
		for (size_t i = 0; i < size; ++i) {
			managedExOvers[i] = new ExOver(this);
		}
	}

	/// <summary>
	/// 보낼 데이터가 남아있으면 true 반환
	/// 내부에서 lock 사용함
	/// </summary>
	/// <returns></returns>
	bool HasSendData() {
		sendingDataLock.lock();
		if (sendingData.empty()) {
			sendingDataLock.unlock();
			return false;
		}
		sendingDataLock.unlock();
		return true;
	}

	/// <summary>
	/// 보낼 데이터를 큐에 쌓아둡니다.
	/// </summary>
	/// <param name="p"></param>
	void AddSendingData(void* p) {
		const unsigned char packetSize = static_cast<unsigned char*>(p)[0];
		sendingDataLock.lock();
		const auto prevSize = sendingData.size();
		const auto totalSendPacketSize = prevSize + static_cast<size_t>(packetSize);
		sendingData.resize(totalSendPacketSize);
		memcpy(&sendingData[prevSize], p, packetSize);
		sendingDataLock.unlock(); // TODO 패킷을 보내는데 받는쪽에서 모르는 패킷이라함
	}

	/// <summary>
	/// 저장해둔 데이터를 모두 초기화합니다.
	/// </summary>
	void ClearSendingData() {
		sendingDataLock.lock();
		sendingData.clear();
		sendingDataLock.unlock();
	}

	/// <summary>
	/// 저장해둔 데이터를 send를 호출하여 id에 해당하는 플레이어에 보냅니다.
	/// NPC에게 보내지 않게 예외처리하지 않습니다.
	/// </summary>
	/// <param name="p_id"></param>
	void SendAddedData(int p_id);

	ExOver& Get() {
		ExOver* result;
		{
			lock_guard<mutex> lg{ managedExOversLock };
			size_t size = managedExOvers.size();
			if (size == 0) {
				managedExOvers.resize(size + EX_OVER_SIZE_INCREMENT);
				for (size_t i = size; i < size + EX_OVER_SIZE_INCREMENT; ++i) {
					managedExOvers[i] = new ExOver(this);
				}
				size += EX_OVER_SIZE_INCREMENT;
			}
			result = managedExOvers[size - 1];
			managedExOvers.pop_back();
		}
		return *result;
	}

	/// <summary>
	/// 직접 호출하는 함수 아님 ExOver::Recycle사용할것
	/// </summary>
	/// <param name="usableGroup"></param>
	void Recycle(ExOver* usableGroup) {
		lock_guard<mutex> lg{ managedExOversLock };
		managedExOvers.push_back(usableGroup);
	}
};
size_t ExOverManager::EX_OVER_SIZE_INCREMENT = 4;
struct TimerEvent {
	int object;
	EOpType e_type;
	chrono::system_clock::time_point start_time;
	int target_id;
	char buffer[MESSAGE_MAX_BUFFER];
	bool hasBuffer;

	constexpr bool operator< (const TimerEvent& L) const {
		return (start_time > L.start_time);
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
		for(auto iter = this->c.begin(); iter != this->c.end();){
			if(*iter == value){
				this->c.erase(iter);
				continue;
			}
			++iter;
		}
	}
};

struct Session {
	mutex  socketLock;
	atomic<EPlayerState> state;
	SOCKET socket;
	ExOver recvOver;

	vector<char> recvedBuf;
	int recvedBufSize;

	ExOverManager exOverManager;
};

struct Actor {
	int		id;
	char name[MAX_NAME];
	short	x, y;
	int		moveTime;

	atomic_bool isActive;
	vector<int> selectedSector;
	vector<int> oldViewList;
	unordered_set<int> viewSet;
	mutex   viewSetLock;

	NpcOver* npcOver;	// Npc만 사용
	lua_State* L;		// Npc만 사용
	mutex luaLock;		// Npc만 사용
	Session* session;	// 플레이어와 같은 세션 접속 유저만 사용
};

struct SECTOR {
	unordered_set<int> sector;
	mutex sectorLock;
};

array <array <SECTOR, WORLD_SECTOR_X_COUNT>, WORLD_SECTOR_Y_COUNT> world_sector;
constexpr int SERVER_ID = 0;
array <Actor, MAX_USER + 1> actors;
removable_priority_queue<TimerEvent> timerQueue;
mutex timerLock;
HANDLE hIocp;

void Disconnect(int playerId);
void DoNpcRandomMove(Actor& npc);

bool IsNpc(int id) {
	return id > NPC_ID_START;
}

Actor* GetActor(int id) {
	return &actors[id];
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

void ExOver::Recycle() {
	manager->Recycle(this);
}

void ExOverManager::SendAddedData(int p_id) {
	vector<unsigned char> copyedSendData;
	sendingDataLock.lock();
	if (sendingData.empty()) {
		sendingDataLock.unlock();
		return;
	}
	copyedSendData.resize(sendingData.size());
	std::copy(sendingData.begin(), sendingData.end(), copyedSendData.begin());
	sendingData.clear();
	sendingDataLock.unlock();
	auto session = GetActor(p_id)->session;
	while (!copyedSendData.empty()) {
		auto sendDataBegin = copyedSendData.begin();
		auto sendDataSize = min(MAX_BUFFER, (int)copyedSendData.size());
		void* p = &*sendDataBegin;
		auto exOver = &Get();
		exOver->m_op = OP_SEND;
		memset(&exOver->m_over, 0, sizeof(exOver->m_over));
		memcpy(exOver->packetBuf, p, sendDataSize);
		exOver->wsabuf[0].buf = reinterpret_cast<CHAR*>(exOver->packetBuf);
		exOver->wsabuf[0].len = sendDataSize;
		auto ret = WSASend(session->socket, exOver->wsabuf, 1, NULL, 0, &exOver->m_over, NULL);
		if (0 != ret) {
			auto err = WSAGetLastError();
			if (WSA_IO_PENDING != err) {
				display_error("WSASend : ", WSAGetLastError());
				Disconnect(p_id);
				return;
			}
		} else {
			copyedSendData.erase(sendDataBegin, sendDataBegin + sendDataSize);
		}
	}
}

void add_event(int obj, EOpType type, int delayMs, const char* buffer = nullptr, int target_id = -1) {
	using namespace chrono;
	TimerEvent ev;
	ev.e_type = type;
	ev.object = obj;
	ev.start_time = system_clock::now() + milliseconds(delayMs);
	ev.target_id = target_id;
	if (nullptr != buffer) {
		memcpy(ev.buffer, buffer, sizeof(char) * strlen(buffer)+1);
		ev.hasBuffer = true;
	} else {
		ev.hasBuffer = false;
	}
	timerLock.lock();
	timerQueue.push(ev);
	timerLock.unlock();
}

void AddSendingData(int targetId, void* buf) {
	add_event(targetId, OP_DELAY_SEND, 12);
	actors[targetId].session->exOverManager.AddSendingData(buf);
}

void CallRecv(int key) {
	actors[key].session->recvOver.wsabuf[0].buf =
		reinterpret_cast<char*>(actors[key].session->recvOver.packetBuf)
		+ actors[key].session->recvedBufSize;
	actors[key].session->recvOver.wsabuf[0].len = MAX_BUFFER - actors[key].session->recvedBufSize;
	memset(&actors[key].session->recvOver.m_over, 0, sizeof(actors[key].session->recvOver.m_over));
	DWORD r_flag = 0;
	int ret = WSARecv(actors[key].session->socket, actors[key].session->recvOver.wsabuf, 1, NULL, &r_flag, &actors[key].session->recvOver.m_over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("WSARecv : ", WSAGetLastError());
	}
}

int GetNewPlayerId(SOCKET socket) {
	for (int i = 1; i <= MAX_PLAYER; ++i) {
		if (PLST_FREE == actors[i].session->state) {
			actors[i].session->state = PLST_CONNECTED;
			lock_guard<mutex> lg{ actors[i].session->socketLock };
			actors[i].session->socket = socket;
			actors[i].name[0] = 0;
			return i;
		}
	}
	return -1;
}

void send_login_ok_packet(int targetId) {
	s2c_login_ok p;
	p.hp = 10;
	p.id = targetId;
	p.level = 2;
	p.race = 1;
	p.size = sizeof(p);
	p.type = S2C_LOGIN_OK;
	auto actor = GetActor(targetId);
	p.x = actor->x;
	p.y = actor->y;
	AddSendingData(targetId, &p);
}

void send_chat(int receiverId, int senderId, const char* mess) {
	s2c_chat p;
	p.id = senderId;
	p.size = sizeof(p);
	p.type = S2C_CHAT;
	strcpy_s(p.mess, mess);
	AddSendingData(receiverId, &p);
}

void send_move_packet(int c_id, int p_id) {
	s2c_move_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_MOVE_PLAYER;
	auto actor = GetActor(p_id);
	p.x = actor->x;
	p.y = actor->y;
	p.move_time = actor->moveTime;
	AddSendingData(c_id, &p);
}

void send_add_object(int targetId, int addedId) {
	s2c_add_player p;
	p.id = addedId;
	p.size = sizeof(p);
	p.type = S2C_ADD_PLAYER;
	auto actor = GetActor(addedId);
	p.x = actor->x;
	p.y = actor->y;
	p.race = 0;
	strcpy_s(p.name, actor->name);
	AddSendingData(targetId, &p);
}

void send_remove_object(int receiverId, int removeTargetId) {
	s2c_remove_player p;
	p.id = removeTargetId;
	p.size = sizeof(p);
	p.type = S2C_REMOVE_PLAYER;
	AddSendingData(receiverId, &p);
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
/// p_id를 가진 플레이어를 제외하고 해당 좌표 섹터에 있는 플레이어를 main_sector에 추가합니다.
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
		if ((isNpc && otherIsNpc) || // p_id가 NPC면 NPC는 필요없다. 사람만 인식한다
			otherId == id || // 내 id는 몰라도 된다
			!CanSee(id, otherId)) { // 안보이는건 필요없다.
			continue;
		}
		returnSector.push_back(otherId);
	}
}

/// <summary>
/// 섹터에 있는 세션 벡터를 리턴합니다. p_id는 포함하지 않습니다.
/// npc id로 호출하면 npc는 걸러서 리턴합니다.
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

void WakeUpNpc(int id) {
	auto actor = GetActor(id);
	if (actor->isActive == false) {
		bool old_state = false;
		if (true == atomic_compare_exchange_strong(&actor->isActive, &old_state, true)) {
			//cout << "wake up id: " << id << " is active: "<< actor->isActive << endl;
			add_event(id, OP_RANDOM_MOVE_LOOP, 1000);
		}
	}
}

void SleepNpc(int id) {
	auto actor = GetActor(id);
	actor->isActive = false;
	TimerEvent timerEvent;
	timerEvent.object = id;
	lock_guard<mutex> lock(timerLock);
	timerQueue.remove_all(timerEvent);
	//cout << "sleep id: " << id << endl;
}

void MovePlayer(int playerId, DIRECTION dir);

/// <summary>
/// 한 스레드에서만 호출안되기때문에 lock 안걸어도됨
/// </summary>
/// <param name="id"></param>
void ProcessPacket(int playerId, unsigned char* buf) {
	switch (buf[1]) {
	case C2S_LOGIN: {
		c2s_login* packet = reinterpret_cast<c2s_login*>(buf);
		auto logined_actor = GetActor(playerId);
		logined_actor->session->state = PLST_INGAME;
		{
			// 위치 이름 초기화
			lock_guard <mutex> lock{ logined_actor->session->socketLock };
			strcpy_s(logined_actor->name, packet->name);
			//session->x = 1; session->y = 1;
			logined_actor->x = rand() % WORLD_X_SIZE;
			logined_actor->y = rand() % WORLD_Y_SIZE;
		}
		{
			// 섹터에 추가
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
	case C2S_MOVE: {
		c2s_move* packet = reinterpret_cast<c2s_move*>(buf);
		auto actor = GetActor(playerId);
		actor->moveTime = packet->move_time;
		MovePlayer(playerId, packet->dr);
		break;
	}
	default: {
		cout << "Unknown Packet Type from Client[" << playerId;
		cout << "] Packet Type [" << buf[1] << "]";
		while (true);
	}
	}
}

void Disconnect(int playerId) {
	auto actor = GetActor(playerId);
	auto session = actor->session;
	if (session->state == PLST_FREE) {
		return;
	}
	closesocket(session->socket);
	session->state = PLST_FREE;

	// remove from sector
	session->exOverManager.ClearSendingData();
	int y = actor->y;
	int x = actor->x;
	auto sector_y = y / WORLD_SECTOR_SIZE;
	auto sector_x = x / WORLD_SECTOR_SIZE;
	auto& main_sector = world_sector[sector_y][sector_x];
	main_sector.sectorLock.lock();
	main_sector.sector.erase(playerId);
	main_sector.sectorLock.unlock();
	/*for (int y = -1; y <= 1; ++y) {
		for (int x = -1; x <= 1; ++x) {
			auto offsetY = sector_y + y;
			auto offsetX = sector_x + x;
			if (offsetY < 0 || world_sector.size() <= offsetY ||
				offsetX < 0 || world_sector[offsetY].size() <= offsetX)
				continue;
			auto& main_sector = world_sector[offsetY][offsetX];
			main_sector.sectorLock.lock();
			main_sector.sector.erase(playerId);
			main_sector.sectorLock.unlock();
		}
	}*/
	//

	actor->viewSetLock.lock();
	actor->oldViewList.resize(actor->viewSet.size());
	std::copy(actor->viewSet.begin(), actor->viewSet.end(), actor->oldViewList.begin());
	actor->viewSet.clear();
	actor->viewSetLock.unlock();
	auto& m_old_view_list = actor->oldViewList;
	for (auto pl : m_old_view_list) {
		if (IsNpc(pl)) {
			lock_guard<mutex> lock(GetActor(pl)->viewSetLock);
			GetActor(pl)->viewSet.erase(actor->id);
			continue;
		}
		auto actor2 = GetActor(playerId);
		if (PLST_INGAME == actor2->session->state) {
			send_remove_object(actor2->id, playerId);
			lock_guard<mutex> lock(actor2->viewSetLock);
			actor2->viewSet.erase(actor->id);
		}
	}
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
			if (SERVER_ID == key) {
				display_error("GQCS : ", WSAGetLastError());
				exit(-1);
			}
			display_error("GQCS : ", WSAGetLastError());
			Disconnect(key);
		}
		if ((key != SERVER_ID) && (0 == recvBufSize)) {
			Disconnect(key);
			continue;
		}
		ExOver* exOver = reinterpret_cast<ExOver*>(recvOver);

		switch (exOver->m_op) {
		case OP_RECV: {
			unsigned char* recvPacketPtr = exOver->packetBuf;
			auto session = GetActor(key)->session;
			int totalRecvBufSize = recvBufSize + session->recvedBufSize;

			for (int recvPacketSize = recvPacketPtr[0];
				recvPacketSize <= totalRecvBufSize; ) {// 또는 받은 데이터가 없음
				ProcessPacket(key, recvPacketPtr);
				totalRecvBufSize -= recvPacketSize;
				recvPacketPtr += recvPacketSize;
				if (totalRecvBufSize == 0)
					break;
				recvPacketSize = recvPacketPtr[0];
			}
			session->recvedBufSize = totalRecvBufSize;
			if (0 < totalRecvBufSize)
				memcpy(exOver->packetBuf, recvPacketPtr, totalRecvBufSize);
			CallRecv(key);
			break;
		}
		case OP_SEND: {
			exOver->Recycle();
			break;
		}
		case OP_ACCEPT: {
			int c_id = GetNewPlayerId(exOver->csocket);
			auto session = GetActor(c_id)->session;
			if (-1 != c_id) {
				session->recvOver.m_op = OP_RECV;
				session->recvedBufSize = 0;
				CreateIoCompletionPort(
					reinterpret_cast<HANDLE>(session->socket), h_iocp, c_id, 0);
				CallRecv(c_id);
			} else {
				closesocket(session->socket);
				cout << "인원 수 꽉참" << endl;
			}

			memset(&exOver->m_over, 0, sizeof(exOver->m_over));
			SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			exOver->csocket = c_socket;
			AcceptEx(l_socket, c_socket,
				exOver->packetBuf, 0, 32, 32, NULL, &exOver->m_over);
			break;
		}
		case OP_RANDOM_MOVE_LOOP: {
			add_event(key, OP_RANDOM_MOVE_LOOP, 1000);
		}
		case OP_RANDOM_MOVE: {
			DoNpcRandomMove(actors[key]);
			break;
		}
		case OP_ATTACK: {
			exOver->Recycle();
			break;
		}
		case OP_DELAY_SEND: {
			// TODO 플레이어 키 상대로만 호출돼야함
			exOver->GetManager()->SendAddedData(key);
			exOver->Recycle();
			break;
		}
		case OP_PLAYER_APPROACH: {
			actors[key].luaLock.lock();
			int moved_player = *reinterpret_cast<int*>(exOver->packetBuf);
			lua_State* L = actors[key].L;
			lua_getglobal(L, "player_is_near");
			lua_pushnumber(L, moved_player);
			int res = lua_pcall(L, 1, 0, 0);
			if (0 != res) {
				cout << "LUA error in exec: " << lua_tostring(L, -1) << endl;
			}
			actors[key].luaLock.unlock();
			exOver->Recycle();
			break;
		}
		case OP_SEND_MESSAGE: {
			int c_id = *reinterpret_cast<int*>(exOver->packetBuf);
			char* mess = reinterpret_cast<char*>(exOver->packetBuf + sizeof(int));
			send_chat(key, c_id, mess);
			exOver->Recycle();
			break;
		}
		}
	}
}

void UpdateSector(int actorId, int prevX, int prevY, int x, int y) {
	const auto sectorViewFrustumX = prevX / WORLD_SECTOR_SIZE;
	const auto sectorViewFrustumY = prevY / WORLD_SECTOR_SIZE;
	const auto newSectorViewFrustumX = x / WORLD_SECTOR_SIZE;
	const auto newSectorViewFrustumY = y / WORLD_SECTOR_SIZE;
	if (y != prevY || x != prevX) {
		// 원래 섹터랑 다르면 다른 섹터로 이동한 것임
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

void DoNpcRandomMove(Actor& npc) {
	auto& x = npc.x;
	auto& y = npc.y;
	auto prevX = x;
	auto prevY = y;
	switch (rand() % 4) {
	case 0: if (x < WORLD_X_SIZE - 1) ++x; break;
	case 1: if (x > 0) --x; break;
	case 2: if (y < WORLD_Y_SIZE - 1) ++y; break;
	case 3: if (y > 0) --y; break;
	}
	UpdateSector(npc.id, prevX, prevY, x, y);

	npc.viewSetLock.lock();
	npc.oldViewList.resize(npc.viewSet.size());
	std::copy(npc.viewSet.begin(), npc.viewSet.end(), npc.oldViewList.begin());
	auto& oldViews = npc.oldViewList;
	npc.viewSetLock.unlock();

	auto&& newViews = GetIdFromOverlappedSector(npc.id);
#ifdef NPCLOG
	lock_guard<mutex> coutLock{coutMutex};
	cout << "npc[" << npc.id << "] (" << npc.x << "," << npc.y << ") 이동 " << oldViewList.size() << "명[";
	for (auto tViewId : oldViewList) {
		cout << tViewId << ",";
	}
	cout << "] -> " << new_vl.size() << "명[";
	for (auto tViewId : new_vl) {
		cout << tViewId << ",";
	}
	cout << "]한테 보임";
#endif // NPCLOG

	if (newViews.empty()) {
		SleepNpc(npc.id); // 아무도 보이지 않으므로 취침
#ifdef NPCLOG
			cout << " & 아무에게도 안보여서 취침" << endl;
#endif
		for (auto otherId : oldViews) {
			auto actor = GetActor(otherId);
			actor->viewSetLock.lock();
			if(0 != actor->viewSet.count(npc.id)){
				actor->viewSet.erase(npc.id);
				actor->viewSetLock.unlock();
				send_remove_object(otherId, npc.id);
			}else{
				actor->viewSetLock.unlock();
			}
		}
		return;
	}
	for (auto otherId : newViews) {
		if (oldViews.end() == std::find(oldViews.begin(), oldViews.end(), otherId)) {
			// 플레이어의 시야에 등장
			auto actor = GetActor(otherId);
			npc.viewSetLock.lock();
			npc.viewSet.insert(otherId);
			npc.viewSetLock.unlock();
#ifdef NPCLOG
				cout << " &[" << pl << "]에게 등장";
#endif
			actor->viewSetLock.lock();
			if(0 == actor->viewSet.count(otherId)){
				send_add_object(otherId, npc.id);
				actor->viewSet.insert(npc.id);
			}
			actor->viewSetLock.unlock();
		} else {
			// 플레이어가 계속 보고있음.
#ifdef NPCLOG
				cout << " &[" << pl << "]에게 위치 갱신";
#endif

			send_move_packet(otherId, npc.id);
		}
	}
	for (auto otherId : oldViews) {
		if (newViews.end() == std::find(newViews.begin(), newViews.end(), otherId)) {
			auto actor = GetActor(otherId);
			npc.viewSetLock.lock();
			npc.viewSet.erase(otherId);
			npc.viewSetLock.unlock();
#ifdef NPCLOG
				cout << " &[" << pl << "]에게서 사라짐";
#endif
			actor->viewSetLock.lock();
			if (actor->viewSet.count(otherId) != 0) {
				actor->viewSet.erase(otherId);
				actor->viewSetLock.unlock();
				send_remove_object(otherId, npc.id);
			} else {
				actor->viewSetLock.unlock();
			}
		}
	}
#ifdef NPCLOG
		cout << endl;
#endif
}

void MovePlayer(int playerId, DIRECTION dir) {
	auto actor = GetActor(playerId);
	auto& x = actor->x;
	auto& y = actor->y;
	auto prevX = x;
	auto prevY = y;
	switch (dir) {
	case D_N: if (y > 0) y--; break;
	case D_S: if (y < (WORLD_Y_SIZE - 1)) y++; break;
	case D_W: if (x > 0) x--; break;
	case D_E: if (x < (WORLD_X_SIZE - 1)) x++; break;
	}
	send_move_packet(playerId, playerId);

	UpdateSector(playerId, prevX, prevY, x, y);

	actor->viewSetLock.lock();
	auto view_set_size = actor->viewSet.size();
	actor->oldViewList.resize(view_set_size);
	if (view_set_size > 0) {
		std::copy(actor->viewSet.begin(), actor->viewSet.end(), actor->oldViewList.begin());
	}
	auto& m_old_view_list = actor->oldViewList;
	actor->viewSetLock.unlock();
	auto&& new_vl = GetIdFromOverlappedSector(playerId);

	for (auto pl : new_vl) {
		if (m_old_view_list.end() == std::find(m_old_view_list.begin(), m_old_view_list.end(), pl)) {
			//1. 새로 시야에 들어오는 플레이어
			actor->viewSetLock.lock();
			actor->viewSet.insert(pl);
			actor->viewSetLock.unlock();
			send_add_object(playerId, pl);

			auto other_actor = GetActor(pl);
			if (false == IsNpc(pl)) {
				other_actor->viewSetLock.lock();
				if (0 == other_actor->viewSet.count(playerId)) {
					other_actor->viewSet.insert(playerId);
					other_actor->viewSetLock.unlock();
					send_add_object(pl, playerId);
#ifdef PLAYERLOG
					{
						lock_guard<mutex> coutLock{coutMutex};
						cout << "시야추가1: " << pl << "," << playerId << endl;
					}
#endif
				} else {
					other_actor->viewSetLock.unlock();
					send_move_packet(pl, playerId);
				}
			} else {
				WakeUpNpc(pl);
#ifdef PLAYERLOG
				{
					lock_guard<mutex> coutLock{coutMutex};
					cout << "플레이어[" << playerId << "]이 " << actor->x << "," << actor->y << " 움직여서 npc[" << pl << "]가 등장 그리고 깨움" << endl;
				}
#endif
				lock_guard<mutex> lock(other_actor->viewSetLock);
				other_actor->viewSet.insert(playerId);
			}
		} else {
			//2. 기존 시야에도 있고 새 시야에도 있는 경우
			if (false == IsNpc(pl)) {
				auto other_actor = GetActor(pl);
				other_actor->viewSetLock.lock();
				if (0 == other_actor->viewSet.count(playerId)) {
					other_actor->viewSet.insert(playerId);
					other_actor->viewSetLock.unlock();
					send_add_object(pl, playerId);
#ifdef PLAYERLOG
					{
						lock_guard<mutex> coutLock{coutMutex};
						cout << "시야추가2 [이게 왜불려]: " << pl << "," << playerId << endl;
					}
#endif
				} else {
					other_actor->viewSetLock.unlock();
					send_move_packet(playerId, pl);
				}
			} else {
				// NPC라면 OP_PLAYER_APPROACH 호출
#ifdef PLAYERLOG
				{
					lock_guard<mutex> coutLock{coutMutex};
					cout << "플레이어[" << playerId << "]이 " << actor->x << "," << actor->y << " 움직여서 npc[" << pl << "]가 갱신" << endl;
				}
#endif
				auto ex_over = &actor->session->exOverManager.Get();
				ex_over->m_op = OP_PLAYER_APPROACH;
				*reinterpret_cast<int*>(ex_over->packetBuf) = playerId;
				PostQueuedCompletionStatus(hIocp, 1, pl, &ex_over->m_over);
			}
		}
	}
	for (auto pl : m_old_view_list) {
		if (new_vl.end() == std::find(new_vl.begin(), new_vl.end(), pl)) {
			// 기존 시야에 있었는데 새 시야에 없는 경우
			actor->viewSetLock.lock();
			actor->viewSet.erase(pl);
			actor->viewSetLock.unlock();
			send_remove_object(playerId, pl);
			//cout << "시야에서 사라짐: " << pl << endl;

			auto other_actor = GetActor(pl);
			if (false == IsNpc(pl)) {
				other_actor->viewSetLock.lock();
				if (0 != other_actor->viewSet.count(playerId)) {
					other_actor->viewSet.erase(playerId);
					other_actor->viewSetLock.unlock();
					send_remove_object(pl, playerId);
				} else {
					other_actor->viewSetLock.unlock();
				}
			} else {
				// NPC 시야에서도 지우기
#ifdef PLAYERLOG
				{
					lock_guard<mutex> coutLock{coutMutex};
					cout << "플레이어[" << playerId << "]이 " << actor->x << "," << actor->y << " 움직여서 npc[" << pl << "]가 사라짐" << endl;
				}
#endif
				lock_guard<mutex> lock(other_actor->viewSetLock );
				other_actor->viewSet.erase(playerId);
			}
		}
	}
}

int API_get_x(lua_State* L) {
	int obj_id = lua_tonumber(L, -1);
	lua_pop(L, 2);
	int x = actors[obj_id].x;
	lua_pushnumber(L, x);
	return 1;
}
int API_get_y(lua_State* L) {
	int obj_id = lua_tonumber(L, -1);
	lua_pop(L, 2);
	int y = actors[obj_id].y;
	lua_pushnumber(L, y);
	return 1;
}

int API_send_mess(lua_State* L) {
	int recverId = lua_tonumber(L, -3);
	int senderId = lua_tonumber(L, -2);
	const char* mess = lua_tostring(L, -1);
	lua_pop(L, 4);
	send_chat(recverId, senderId, mess);
	return 0;
}
int API_print(lua_State* L) {
	const char* mess = lua_tostring(L, -1);
	lua_pop(L, 2);
	cout << mess;
	return 0;
}

int API_add_event_npc_random_move(lua_State* L) {
	int p_id = lua_tonumber(L, -2);
	int delay = lua_tonumber(L, -1);
	lua_pop(L, 3);
	add_event(p_id, OP_RANDOM_MOVE, delay);
	return 0;
}

int API_add_event_send_mess(lua_State* L) {
	int recverId = lua_tonumber(L, -4);
	int senderId = lua_tonumber(L, -3);
	const char* mess = lua_tostring(L, -2);
	int delay = lua_tonumber(L, -1);
	lua_pop(L, 5);
	add_event(recverId, OP_SEND_MESSAGE, delay, mess, senderId);
	return 0;
}

void do_timer() {
	using namespace chrono;
	for (;;) {
		timerLock.lock();
		if (false == timerQueue.empty() && timerQueue.top().start_time < system_clock::now()) {
			TimerEvent ev = timerQueue.top();
			timerQueue.pop();
			timerLock.unlock();
			if (ev.e_type == OP_DELAY_SEND &&
				!actors[ev.object].session->exOverManager.HasSendData()) {
				continue;
			}

			if (IsNpc(ev.object)) {
				if (!actors[ev.object].isActive) {
					continue;
				}
				auto over = GetActor(ev.object)->npcOver;
				over->m_op = ev.e_type;
				memset(&over->m_over, 0, sizeof(over->m_over));

				PostQueuedCompletionStatus(hIocp, 1, ev.object, &over->m_over);
			} else {
				auto over = &GetActor(ev.object)->session->exOverManager.Get();
				over->m_op = ev.e_type;
				memcpy(over->packetBuf, &ev.target_id, sizeof(ev.target_id));
				if (ev.hasBuffer) {
					memcpy(over->packetBuf + sizeof(ev.target_id),
						ev.buffer, 
						min(sizeof(ev.buffer), sizeof(over->packetBuf) - sizeof(ev.target_id)));
				}
				PostQueuedCompletionStatus(hIocp, 1, ev.object, &over->m_over);
			}
		} else {
			timerLock.unlock();
			this_thread::sleep_for(10ms);
		}
	}
}


int main() {
	for (int i = SERVER_ID + 1; i <= MAX_PLAYER; ++i) {
		auto& pl = actors[i];
		pl.id = i;
		pl.session = new Session;
		pl.session->state = PLST_FREE;
		pl.isActive = true;
	}
	for (int i = MAX_PLAYER + 1; i <= MAX_USER; i++) {
		auto& npc = actors[i];
		npc.id = i;
		sprintf_s(npc.name, "N%d", npc.id);
		npc.x = rand() % WORLD_X_SIZE;
		npc.y = rand() % WORLD_Y_SIZE;
		npc.moveTime = 0;
		auto sectorViewFrustumX = npc.x / WORLD_SECTOR_SIZE;
		auto sectorViewFrustumY = npc.y / WORLD_SECTOR_SIZE;
		npc.npcOver = new NpcOver;
		memset(&npc.npcOver->m_over, 0, sizeof(npc.npcOver->m_over));
		npc.isActive = false;
		world_sector[sectorViewFrustumY][sectorViewFrustumX].sector.insert(npc.id);
		lua_State* L = npc.L = luaL_newstate();
		luaL_openlibs(L);
		luaL_loadfile(L, "npc.lua");
		int res = lua_pcall(L, 0, 0, 0);
		if (0 != res) {
			cout << "LUA error in exec: " << lua_tostring(L, -1) << endl;
		}
		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 0, 0);

		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
		lua_register(L, "API_send_mess", API_send_mess);
		lua_register(L, "API_print", API_print);
		lua_register(L, "API_add_event_npc_random_move", API_add_event_npc_random_move);
		lua_register(L, "API_add_event_send_mess", API_add_event_send_mess);
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

	ExOver accept_over;
	accept_over.m_op = OP_ACCEPT;
	memset(&accept_over.m_over, 0, sizeof(accept_over.m_over));
	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	accept_over.csocket = c_socket;
	BOOL ret = AcceptEx(listenSocket, c_socket,
		accept_over.packetBuf, 0, 32, 32, NULL, &accept_over.m_over);
	if (FALSE == ret) {
		int err_num = WSAGetLastError();
		if (err_num != WSA_IO_PENDING)
			display_error("AcceptEx Error", err_num);
	}

	cout << "서버 열림" << endl;

	thread timer_thread(do_timer);
	vector <thread> worker_threads;
	for (int i = 0; i < 4; ++i)
		worker_threads.emplace_back(worker, hIocp, listenSocket);
	for (auto& th : worker_threads)
		th.join();
	closesocket(listenSocket);
	WSACleanup();
}
