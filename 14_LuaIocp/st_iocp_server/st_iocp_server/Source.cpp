#include <iostream>
#include <unordered_map>
#include <thread>
#include <vector>
#include <mutex>
#include <array>
#include <queue>
#include <set>
#include <unordered_set>
using namespace std;
#include <WS2tcpip.h>
#include <MSWSock.h>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

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

enum OP_TYPE { OP_RECV, OP_SEND, OP_ACCEPT, OP_RANDOM_MOVE, OP_ATTACK, OP_PLAYER_APPROACH, OP_SEND_MESSAGE, OP_DELAY_SEND };

struct NPC_OVER {
	WSAOVERLAPPED	m_over;
	OP_TYPE			m_op;
};
struct EX_OVER : public NPC_OVER {
	WSABUF			m_wsabuf[1];
	unsigned char	m_packetbuf[MAX_BUFFER];
	SOCKET			m_csocket;					// OP_ACCEPT에서만 사용
};

enum PL_STATE { PLST_FREE, PLST_CONNECTED, PLST_INGAME };

constexpr size_t SEND_EXOVER_INCREASEMENT_SIZE = 4;

/// <summary>
/// get으로 EX_OVER 꺼내서 쓰고, 쓴 EX_OVER를 ExOverUsableGroup로 형변환하고 recycle 호출하면 됨
/// </summary>
struct SendExOverManager {
	struct ExOverUsableGroup {
	private:
		EX_OVER ex_over;
		//bool is_used = false;
		SendExOverManager* manager;

	public:
		ExOverUsableGroup(SendExOverManager* manager) : manager(manager) {

		}
		//void setIsUsed(bool used) {
		//	is_used = used;
		//}

		//bool getIsUsed() const {
		//	return is_used;
		//}

		EX_OVER& getExOver() {
			return ex_over;
		}

		void recycle() {
			manager->recycle(this);
		}

		SendExOverManager* getSendExOverManager() const {
			return manager;
		}

		friend SendExOverManager;
	};

private:
	vector<ExOverUsableGroup*> m_send_over;
	mutex m_send_over_vector_lock;
	vector<unsigned char> m_send_data;
	mutex m_send_data_lock;

public:
	SendExOverManager() {
		m_send_over.resize(2);
		auto size = m_send_over.size();
		for (size_t i = 0; i < size; ++i) {
			m_send_over[i] = new ExOverUsableGroup(this);
		}
	}

	bool hasSendData() {
		m_send_data_lock.lock();
		if (m_send_data.empty()) {
			m_send_data_lock.unlock();
			return false;
		}
		m_send_data_lock.unlock();
		return true;
	}

	void addSendData(void* p) {
		unsigned char p_size = reinterpret_cast<unsigned char*>(p)[0];
		m_send_data_lock.lock();

		auto prev_size = m_send_data.size();
		auto send_data_total_size = prev_size + static_cast<size_t>(p_size);
		m_send_data.resize(send_data_total_size);
		//if (prev_size > 20) {
		//	cout << "packet size: " << +p_size << "; prev_size: " << prev_size << "; send_data_total_size: " << send_data_total_size << endl;
		//}
		memcpy(&m_send_data[prev_size], p, p_size);
		m_send_data_lock.unlock();
	}

	void clearSendData() {
		m_send_data_lock.lock();
		m_send_data.clear();
		m_send_data_lock.unlock();
		//for (int i = 0; i < m_send_over.size(); ++i) {
		//	m_send_over[i]->setIsUsed(false);
		//}
	}

	void sendAddedData(int p_id);

	EX_OVER& get() {
		ExOverUsableGroup* send_ex_over;
		{
			lock_guard<mutex> lg{ m_send_over_vector_lock };
			//return m_send_over[0].getExOver();
			size_t size = m_send_over.size();
			if (size == 0) {
				m_send_over.resize(size + SEND_EXOVER_INCREASEMENT_SIZE);
				for (size_t i = size; i < size + SEND_EXOVER_INCREASEMENT_SIZE; ++i) {
					m_send_over[i] = new ExOverUsableGroup(this);
				}
				size += SEND_EXOVER_INCREASEMENT_SIZE;
			}
			send_ex_over = m_send_over[size - 1];
			m_send_over.pop_back();
		}
		//send_ex_over->setIsUsed(true);
		return send_ex_over->getExOver();
	}

private:
	void recycle(ExOverUsableGroup* usableGroup) {
		//usableGroup->setIsUsed(false);
		lock_guard<mutex> lg{ m_send_over_vector_lock };
		m_send_over.push_back(usableGroup);
	}
};

struct TIMER_EVENT {
	int object;
	OP_TYPE e_type;
	chrono::system_clock::time_point start_time;
	int target_id;
	char buffer[MESSAGE_MAX_BUFFER];
	bool hasBuffer;

	constexpr bool operator< (const TIMER_EVENT& L) const {
		return (start_time > L.start_time);
	}
};

priority_queue<TIMER_EVENT> timer_queue;
mutex timer_l;
HANDLE h_iocp;

struct SESSION {
	mutex  m_slock;
	atomic<PL_STATE> m_state;
	SOCKET m_socket;
	EX_OVER m_recv_over;

	int m_prev_size;

	SendExOverManager sendExOverManager;
};

struct S_ACTOR {
	int		id;
	char m_name[200];
	short	x, y;
	int		move_time;
	unordered_set <int> m_view_list;
	mutex   m_vl;

	lua_State* L;
	mutex   m_sl;

	atomic_bool is_active;
	vector<int> m_selected_sector;
	vector<int> m_old_view_list;
	unordered_set<int> m_view_set;

	NPC_OVER npcOver;
	SESSION* session;
};

constexpr int SERVER_ID = 0;
array <S_ACTOR, MAX_USER + 1> actors;
void disconnect(int p_id);
void do_npc_random_move(S_ACTOR& npc);

bool is_npc(int id) {
	return id > NPC_ID_START;
}

S_ACTOR* get_actor(int id) {
	return &actors[id];
}

struct SECTOR {
	unordered_set<int> m_sector;
	mutex m_sector_lock;
};
array <array <SECTOR, WORLD_SECTOR_X_COUNT>, WORLD_SECTOR_Y_COUNT> world_sector;

void display_error(const char* msg, int err_no) {
	//WCHAR* lpMsgBuf;
	//FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
	//	NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	//	(LPTSTR)&lpMsgBuf, 0, NULL);
	//cout << msg;
	//wcout << lpMsgBuf << endl;
	//LocalFree(lpMsgBuf);
}

void SendExOverManager::sendAddedData(int p_id) {
	vector<unsigned char> copyed_send_data;
	m_send_data_lock.lock();
	if (m_send_data.empty()) {
		m_send_data_lock.unlock();
		return;
	}
	copyed_send_data.resize(m_send_data.size());
	std::copy(m_send_data.begin(), m_send_data.end(), copyed_send_data.begin());
	m_send_data.clear();
	m_send_data_lock.unlock();
	auto session = get_actor(p_id)->session;
	while (!copyed_send_data.empty()) {
		auto send_data_begin = copyed_send_data.begin();
		auto p_size = min(MAX_BUFFER, (int)copyed_send_data.size());
		void* p = &*send_data_begin;
		auto s_over = &get();
		s_over->m_op = OP_SEND;
		memset(&s_over->m_over, 0, sizeof(s_over->m_over));
		memcpy(s_over->m_packetbuf, p, p_size);
		s_over->m_wsabuf[0].buf = reinterpret_cast<CHAR*>(s_over->m_packetbuf);
		s_over->m_wsabuf[0].len = p_size;
		auto ret = WSASend(session->m_socket, s_over->m_wsabuf, 1, NULL, 0, &s_over->m_over, NULL);
		if (0 != ret) {
			auto err_no = WSAGetLastError();
			if (WSA_IO_PENDING != err_no) {
				display_error("WSASend : ", WSAGetLastError());
				disconnect(p_id);
				return;
			}
		} else {
			copyed_send_data.erase(send_data_begin, send_data_begin + p_size);
		}
	}
}

void add_event(int obj, OP_TYPE ev_t, int delay_ms, const char* buffer = nullptr, int target_id = -1) {
	using namespace chrono;
	TIMER_EVENT ev;
	ev.e_type = ev_t;
	ev.object = obj;
	ev.start_time = system_clock::now() + milliseconds(delay_ms);
	if (nullptr != buffer) {
		memcpy(ev.buffer, buffer, sizeof(char) * strlen(buffer));
		ev.hasBuffer = true;
	} else {
		ev.hasBuffer = false;
	}
	ev.target_id = target_id;
	timer_l.lock();
	timer_queue.push(ev);
	timer_l.unlock();
}

void send_packet(int p_id, void* p) {
	add_event(p_id, OP_DELAY_SEND, 12);
	actors[p_id].session->sendExOverManager.addSendData(p);
	//int p_size = reinterpret_cast<unsigned char*>(p)[0];
	//int p_type = reinterpret_cast<unsigned char*>(p)[1];
	//cout << "To client [" << p_id << "] : ";
	//cout << "Packet [" << p_type << "]\n";
	/*EX_OVER* s_over = &players[p_id].sendExOverManager.get();
	s_over->m_op = OP_SEND;
	memset(&s_over->m_over, 0, sizeof(s_over->m_over));
	memcpy(s_over->m_packetbuf, p, p_size);
	s_over->m_wsabuf[0].buf = reinterpret_cast<CHAR*>(s_over->m_packetbuf);
	s_over->m_wsabuf[0].len = p_size;
	int ret = WSASend(players[p_id].m_socket, s_over->m_wsabuf, 1,
		NULL, 0, &s_over->m_over, 0);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) {
			display_error("WSASend : ", WSAGetLastError());
			disconnect(p_id);
		}
	}*/
}

void do_recv(int key) {
	actors[key].session->m_recv_over.m_wsabuf[0].buf =
		reinterpret_cast<char*>(actors[key].session->m_recv_over.m_packetbuf)
		+ actors[key].session->m_prev_size;
	actors[key].session->m_recv_over.m_wsabuf[0].len = MAX_BUFFER - actors[key].session->m_prev_size;
	memset(&actors[key].session->m_recv_over.m_over, 0, sizeof(actors[key].session->m_recv_over.m_over));
	DWORD r_flag = 0;
	int ret = WSARecv(actors[key].session->m_socket, actors[key].session->m_recv_over.m_wsabuf, 1, NULL, &r_flag, &actors[key].session->m_recv_over.m_over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("WSARecv : ", WSAGetLastError());
	}
}

int get_new_player_id(SOCKET p_socket) {
	for (int i = 1; i <= MAX_PLAYER; ++i) {
		if (PLST_FREE == actors[i].session->m_state) {
			lock_guard<mutex> lg{ actors[i].session->m_slock };
			actors[i].session->m_state = PLST_CONNECTED;
			actors[i].session->m_socket = p_socket;
			actors[i].m_name[0] = 0;
			return i; // SERVER_ID 때문에 +1된거 넘김
		}
	}
	return -1;
}

void send_login_ok_packet(int p_id) {
	s2c_login_ok p;
	p.hp = 10;
	p.id = p_id;
	p.level = 2;
	p.race = 1;
	p.size = sizeof(p);
	p.type = S2C_LOGIN_OK;
	auto actor = get_actor(p_id);
	p.x = actor->x;
	p.y = actor->y;
	send_packet(p_id, &p);
}

void send_chat(int c_id, int p_id, const char* mess) {
	s2c_chat p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_CHAT;
	strcpy_s(p.mess, mess);
	send_packet(c_id, &p);
}

void send_move_packet(int c_id, int p_id) {
	s2c_move_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_MOVE_PLAYER;
	auto actor = get_actor(p_id);
	p.x = actor->x;
	p.y = actor->y;
	p.move_time = actor->move_time;
	send_packet(c_id, &p);
}

void send_add_object(int targetId, int addedId) {
	s2c_add_player p;
	p.id = addedId;
	p.size = sizeof(p);
	p.type = S2C_ADD_PLAYER;
	auto actor = get_actor(addedId);
	p.x = actor->x;
	p.y = actor->y;
	p.race = 0;
	strcpy_s(p.name, actor->m_name);
	send_packet(targetId, &p);
}

void send_remove_object(int c_id, int p_id) {
	s2c_remove_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_REMOVE_PLAYER;
	send_packet(c_id, &p);
}

bool can_see(int id_a, int id_b) {
	auto actor_a = get_actor(id_a);
	auto actor_b = get_actor(id_b);
	int ax = actor_a->x;
	int ay = actor_a->y;
	int bx = actor_b->x;
	int by = actor_b->y;
	return VIEW_RADIUS >=
		abs(ax - bx) + abs(ay - by);
}
/// <summary>
/// p_id를 가진 플레이어를 제외하고 해당 좌표 섹터에 있는 플레이어를 main_sector에 추가합니다.
/// </summary>
/// <param name="y"></param>
/// <param name="p_id"></param>
/// <param name="x"></param>
/// <param name="main_sector"></param>
void add_sector_players_to_main_sector(int p_id, int y, int x, vector<int> main_sector) {
	auto& other_set = world_sector[y][x].m_sector;
	auto isNpc = is_npc(p_id);
	for (auto id : other_set) {
		if (isNpc == is_npc(id) || !can_see(p_id, id) || id == p_id || (!is_npc(id) && actors[id].session->m_state != PLST_INGAME)) {
			continue;
		}
		main_sector.push_back(id);
	}
}

/// <summary>
/// 섹터에 있는 세션 벡터를 리턴합니다. p_id는 포함하지 않습니다.
/// </summary>
/// <param name="p_id"></param>
/// <returns></returns>
vector<int>& get_id_from_overlapped_sector(int p_id) {
	auto actor = get_actor(p_id);
	int y = actor->y;
	int x = actor->x;
	auto sector_y = y / WORLD_SECTOR_SIZE;
	auto sector_x = x / WORLD_SECTOR_SIZE;
	auto& main_sector = world_sector[sector_y][sector_x];

	auto& selected_sector = actor->m_selected_sector;
	selected_sector.clear();
	main_sector.m_sector_lock.lock();
	for (auto id : main_sector.m_sector) {
		if (id != actor->id && can_see(id, actor->id)) {
			selected_sector.push_back(id);
		}
	}
	main_sector.m_sector_lock.unlock();

	auto sector_view_frustum_top = (y - VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sector_view_frustum_bottom = (y + VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sector_view_frustum_left = (x - VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sector_view_frustum_right = (x + VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	if (sector_view_frustum_top != sector_y && 0 < sector_y) {
		add_sector_players_to_main_sector(actor->id, sector_view_frustum_top, sector_x, selected_sector);
		if (sector_view_frustum_left != sector_x && 0 < sector_x) {
			add_sector_players_to_main_sector(actor->id, sector_view_frustum_top, sector_view_frustum_left, selected_sector);
		} else if (sector_view_frustum_right != sector_x && sector_x < static_cast<int>(world_sector[
			sector_view_frustum_top].size() - 1)) {
			add_sector_players_to_main_sector(actor->id, sector_view_frustum_top, sector_view_frustum_right,
				selected_sector);
		}
	} else if (sector_view_frustum_bottom != sector_y && sector_y < static_cast<int>(world_sector.size() - 1)) {
		add_sector_players_to_main_sector(actor->id, sector_view_frustum_bottom, sector_x, selected_sector);
		if (sector_view_frustum_left != sector_x && 0 < sector_x) {
			add_sector_players_to_main_sector(actor->id, sector_view_frustum_bottom, sector_view_frustum_left,
				selected_sector);
		} else if (sector_view_frustum_right != sector_x && sector_x < static_cast<int>(world_sector[
			sector_view_frustum_bottom].size() - 1)) {
			add_sector_players_to_main_sector(actor->id, sector_view_frustum_bottom, sector_view_frustum_right,
				selected_sector);
		}
	}
	if (sector_view_frustum_left != sector_x && 0 < sector_x) {
		add_sector_players_to_main_sector(actor->id, sector_y, sector_view_frustum_left, selected_sector);
	} else if (sector_view_frustum_right != sector_x && sector_x < static_cast<int>(world_sector[sector_y].size() - 1)) {
		add_sector_players_to_main_sector(actor->id, sector_y, sector_view_frustum_right, selected_sector);
	}
	return selected_sector;
}

void wake_up_npc(int id) {
	auto actor = get_actor(id);
	if (actor->is_active == false) {
		bool old_state = false;
		if (true == atomic_compare_exchange_strong(&actor->is_active, &old_state, true)) {
			//cout << "wake up id: " << id << " is active: "<< actor->is_active << endl;
			add_event(id, OP_RANDOM_MOVE, 1000);
		}
	}
}

void sleep_npc(int id) {
	auto actor = get_actor(id);
	actor->is_active = false;
	//cout << "sleep id: " << id << endl;
}

void do_move(int p_id, DIRECTION dir);


void process_packet(int p_id, unsigned char* p_buf) {
	switch (p_buf[1]) {
	case C2S_LOGIN: {
		c2s_login* packet = reinterpret_cast<c2s_login*>(p_buf);
		auto actor1 = get_actor(p_id);
		actor1->session->m_state = PLST_INGAME;
		{
			lock_guard <mutex> gl1{ actor1->session->m_slock };
			strcpy_s(actor1->m_name, packet->name);
			//session->x = 1;
			//session->y = 1;
			actor1->x = rand() % WORLD_X_SIZE;
			actor1->y = rand() % WORLD_Y_SIZE;
		}
		{
			auto& sector = world_sector[actor1->y / WORLD_SECTOR_SIZE][actor1->x / WORLD_SECTOR_SIZE];
			lock_guard <mutex> gl2{ sector.m_sector_lock };
			sector.m_sector.insert(p_id);
		}
		send_login_ok_packet(p_id);

		auto&& selected_sector = get_id_from_overlapped_sector(p_id);

		actor1->m_vl.lock();
		for (auto id : selected_sector) {
			actor1->m_view_set.insert(id);
		}
		actor1->m_vl.unlock();
		for (auto id : selected_sector) {
			auto actor2 = get_actor(id);
			send_add_object(p_id, id);
			if (!is_npc(id)) {
				send_add_object(id, p_id);
				actor2->m_vl.lock();
				actor2->m_view_set.insert(p_id);
				actor2->m_vl.unlock();
			} else {
				wake_up_npc(actor2->id);
			}
		}
		break;
	}
	case C2S_MOVE: {
		c2s_move* packet = reinterpret_cast<c2s_move*>(p_buf);
		auto actor = get_actor(p_id);
		actor->move_time = packet->move_time;
		do_move(p_id, packet->dr);
		break;
	}
	default: {
		cout << "Unknown Packet Type from Client[" << p_id;
		cout << "] Packet Type [" << p_buf[1] << "]";
		while (true);
	}
	}
}

void disconnect(int p_id) {
	auto actor = get_actor(p_id);
	auto session = actor->session;
	if (session->m_state == PLST_FREE) {
		return;
	}
	closesocket(session->m_socket);
	session->m_state = PLST_FREE;

	// remove from sector
	session->sendExOverManager.clearSendData();
	int y = actor->y;
	int x = actor->x;
	auto sector_y = y / WORLD_SECTOR_SIZE;
	auto sector_x = x / WORLD_SECTOR_SIZE;
	for (int y = -1; y <= 1; ++y) {
		for (int x = -1; x <= 1; ++x) {
			if (sector_y + y < 0 || world_sector.size() <= sector_y + y ||
				sector_x + x < 0 || world_sector[sector_y + y].size() <= sector_x + x)
				continue;
			auto& main_sector = world_sector[sector_y + y][sector_x + x];
			main_sector.m_sector_lock.lock();
			main_sector.m_sector.erase(p_id);
			main_sector.m_sector_lock.unlock();
		}
	}
	//

	actor->m_vl.lock();
	actor->m_old_view_list.resize(actor->m_view_set.size());
	std::copy(actor->m_view_set.begin(), actor->m_view_set.end(), actor->m_old_view_list.begin());
	actor->m_view_set.clear();
	actor->m_vl.unlock();
	auto& m_old_view_list = actor->m_old_view_list;
	for (auto pl : m_old_view_list) {
		if (is_npc(pl)) {
			break;
		}
		auto actor2 = get_actor(p_id);
		if (PLST_INGAME == actor2->session->m_state) {
			send_remove_object(actor2->id, p_id);
		}
	}
}

void worker(HANDLE h_iocp, SOCKET l_socket) {
	while (true) {
		DWORD num_bytes;
		ULONG_PTR ikey;
		WSAOVERLAPPED* over;

		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes,
			&ikey, &over, INFINITE);

		int key = static_cast<int>(ikey);
		if (FALSE == ret) {
			if (SERVER_ID == key) {
				display_error("GQCS : ", WSAGetLastError());
				exit(-1);
			} else {
				display_error("GQCS : ", WSAGetLastError());
				disconnect(key);
			}
		}
		if ((key != SERVER_ID) && (0 == num_bytes)) {
			disconnect(key);
			continue;
		}
		EX_OVER* ex_over = reinterpret_cast<EX_OVER*>(over);

		switch (ex_over->m_op) {
		case OP_RECV: {
			unsigned char* packet_ptr = ex_over->m_packetbuf;
			auto session = get_actor(key)->session;
			int num_data = num_bytes + session->m_prev_size;
			int packet_size = packet_ptr[0];

			while (num_data >= packet_size) {
				process_packet(key, packet_ptr);
				num_data -= packet_size;
				packet_ptr += packet_size;
				if (0 >= num_data) break;
				packet_size = packet_ptr[0];
			}
			session->m_prev_size = num_data;
			if (0 != num_data)
				memcpy(ex_over->m_packetbuf, packet_ptr, num_data);
			do_recv(key);
			break;
		}
		case OP_SEND: {
			auto usableGroup = reinterpret_cast<SendExOverManager::ExOverUsableGroup*>(ex_over);
			usableGroup->recycle();
			break;
		}
		case OP_ACCEPT: {
			int c_id = get_new_player_id(ex_over->m_csocket);
			auto session = get_actor(c_id)->session;
			if (-1 != c_id) {
				session->m_recv_over.m_op = OP_RECV;
				session->m_prev_size = 0;
				CreateIoCompletionPort(
					reinterpret_cast<HANDLE>(session->m_socket), h_iocp, c_id, 0);
				do_recv(c_id);
			} else {
				closesocket(session->m_socket);
				cout << "인원 수 꽉참" << endl;
			}

			memset(&ex_over->m_over, 0, sizeof(ex_over->m_over));
			SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			ex_over->m_csocket = c_socket;
			AcceptEx(l_socket, c_socket,
				ex_over->m_packetbuf, 0, 32, 32, NULL, &ex_over->m_over);
			break;
		}
		case OP_RANDOM_MOVE: {
			auto npc = get_actor(key);
			do_npc_random_move(*npc);
			add_event(key, OP_RANDOM_MOVE, 1000);
			break;
		}
		case OP_ATTACK: {
			auto usableGroup = reinterpret_cast<SendExOverManager::ExOverUsableGroup*>(ex_over);
			usableGroup->recycle();
			break;
		}
		case OP_DELAY_SEND: {
			// TODO 플레이어 키 상대로만 호출돼야함
			auto usableGroup = reinterpret_cast<SendExOverManager::ExOverUsableGroup*>(ex_over);
			usableGroup->getSendExOverManager()->sendAddedData(key);
			usableGroup->recycle();
			break;
		}
		case OP_PLAYER_APPROACH: {
			actors[key].m_sl.lock();
			int move_player = *reinterpret_cast<int*>(ex_over->m_packetbuf);
			lua_State* L = actors[key].L;
			lua_getglobal(L, "player_is_near");
			lua_pushnumber(L, move_player);
			int res = lua_pcall(L, 1, 0, 0);
			if (0 != res) {
				cout << "LUA error in exec: " << lua_tostring(L, -1) << endl;
			}
			actors[key].m_sl.unlock();
			auto usableGroup = reinterpret_cast<SendExOverManager::ExOverUsableGroup*>(ex_over);
			usableGroup->recycle();
			break;
		}
		case OP_SEND_MESSAGE: {
			int c_id = *reinterpret_cast<int*>(ex_over->m_packetbuf);
			char* mess = reinterpret_cast<char*>(ex_over->m_packetbuf + sizeof(int));
			send_chat(c_id, key, mess);
			auto usableGroup = reinterpret_cast<SendExOverManager::ExOverUsableGroup*>(ex_over);
			usableGroup->recycle();
			break;
		}
		}
	}
}

void updateSector(int p_id, int prevX, int prevY, int x, int y) {
	auto sector_view_frustum_x = prevX / WORLD_SECTOR_SIZE;
	auto sector_view_frustum_y = prevY / WORLD_SECTOR_SIZE;
	auto new_sector_view_frustum_x = x / WORLD_SECTOR_SIZE;
	auto new_sector_view_frustum_y = y / WORLD_SECTOR_SIZE;
	if (y != prevY || x != prevX) {
		// 원래 섹터랑 다르면 다른 섹터로 이동한 것임
		{
			lock_guard<mutex> gl2{ world_sector[sector_view_frustum_y][sector_view_frustum_x].m_sector_lock };
			world_sector[sector_view_frustum_y][sector_view_frustum_x].m_sector.erase(p_id);
		}
		{
			lock_guard<mutex> gl2{ world_sector[new_sector_view_frustum_y][new_sector_view_frustum_x].m_sector_lock };
			world_sector[new_sector_view_frustum_y][new_sector_view_frustum_x].m_sector.insert(p_id);
		}
	}
}

void do_npc_random_move(S_ACTOR& npc) {
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
	updateSector(npc.id, prevX, prevY, x, y);

	npc.m_vl.lock();
	npc.m_old_view_list.resize(npc.m_view_set.size());
	std::copy(npc.m_view_set.begin(), npc.m_view_set.end(), npc.m_old_view_list.begin());
	auto& oldViewList = npc.m_old_view_list;
	npc.m_vl.unlock();

	auto&& new_vl = get_id_from_overlapped_sector(npc.id);

	if (new_vl.empty()) {
		sleep_npc(npc.id); // 아무도 보이지 않으므로 취침
		return;
	}
	for (auto pl : new_vl) {
		if (is_npc(pl)) {
			continue;
		}
		if (oldViewList.end() == std::find(oldViewList.begin(), oldViewList.end(), pl)) {
			// 플레이어의 시야에 등장
			auto actor = get_actor(pl);
			actor->m_vl.lock();
			actor->m_view_set.insert(npc.id);
			actor->m_vl.unlock();
			send_add_object(pl, npc.id);
		} else {
			// 플레이어가 계속 보고있음.
			send_move_packet(pl, npc.id);
		}
	}
	for (auto pl : oldViewList) {
		if (is_npc(pl)) {
			continue;
		}
		if (new_vl.end() == std::find(new_vl.begin(), new_vl.end(), pl)) {
			auto actor = get_actor(pl);
			actor->m_vl.lock();
			if (actor->m_view_set.count(pl) != 0) {
				actor->m_view_set.erase(pl);
				actor->m_vl.unlock();
				send_remove_object(pl, npc.id);
			} else {
				actor->m_vl.unlock();
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
	int p_id = lua_tonumber(L, -3);
	int o_id = lua_tonumber(L, -2);
	const char* mess = lua_tostring(L, -1);
	lua_pop(L, 4);
	send_chat(p_id, o_id, mess);
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
	int p_id = lua_tonumber(L, -4);
	int o_id = lua_tonumber(L, -3);
	const char* mess = lua_tostring(L, -2);
	int delay = lua_tonumber(L, -1);
	lua_pop(L, 5);
	add_event(p_id, OP_SEND_MESSAGE, delay, mess, o_id);
	return 0;
}

void do_move(int p_id, DIRECTION dir) {
	auto actor = get_actor(p_id);
	auto& x = actor->x;
	auto& y = actor->y;
	auto prevX = x;
	auto prevY = y;
	switch (dir) {
	case D_N: if (y > 0) y--;
		break;
	case D_S: if (y < (WORLD_Y_SIZE - 1)) y++;
		break;
	case D_W: if (x > 0) x--;
		break;
	case D_E: if (x < (WORLD_X_SIZE - 1)) x++;
		break;
	}
	send_move_packet(p_id, p_id);

	updateSector(p_id, prevX, prevY, x, y);

	actor->m_vl.lock();
	auto view_set_size = actor->m_view_set.size();
	actor->m_old_view_list.resize(view_set_size);
	if (view_set_size > 0) {
		std::copy(actor->m_view_set.begin(), actor->m_view_set.end(), actor->m_old_view_list.begin());
	}
	auto& m_old_view_list = actor->m_old_view_list;
	actor->m_vl.unlock();
	auto&& new_vl = get_id_from_overlapped_sector(p_id);

	for (auto pl : new_vl) {
		if (m_old_view_list.end() == std::find(m_old_view_list.begin(), m_old_view_list.end(), pl)) {
			//1. 새로 시야에 들어오는 플레이어
			actor->m_vl.lock();
			actor->m_view_set.insert(pl);
			actor->m_vl.unlock();
			send_add_object(p_id, pl);

			if (false == is_npc(pl)) {
				auto actor2 = get_actor(p_id);
				actor2->m_vl.lock();
				if (0 == actor2->m_view_set.count(p_id)) {
					actor2->m_view_set.insert(p_id);
					actor2->m_vl.unlock();
					send_add_object(pl, p_id);
					//cout << "시야추가1: " << pl << ", " << p_id << endl;
				} else {
					actor2->m_vl.unlock();
					send_move_packet(pl, p_id);
				}
			} else {
				wake_up_npc(pl);
			}
		} else {
			//2. 기존 시야에도 있고 새 시야에도 있는 경우
			if (false == is_npc(pl)) {
				auto actor2 = get_actor(p_id);
				actor2->m_vl.lock();
				if (0 == actor2->m_view_set.count(p_id)) {
					actor2->m_view_set.insert(p_id);
					actor2->m_vl.unlock();
					send_add_object(pl, p_id);
					//cout << "시야추가2: " << pl << ", " << p_id << endl;
				} else {
					actor2->m_vl.unlock();
					send_move_packet(p_id, pl);
				}
			} else {
				// NPC라면 OP_PLAYER_APPROACH 호출
				auto ex_over = &get_actor(p_id)->session->sendExOverManager.get();
				ex_over->m_op = OP_PLAYER_APPROACH;
				*reinterpret_cast<int*>(ex_over->m_packetbuf) = p_id;
				PostQueuedCompletionStatus(h_iocp, 1, pl, &ex_over->m_over);
			}
		}
	}
	for (auto pl : m_old_view_list) {
		if (new_vl.end() == std::find(new_vl.begin(), new_vl.end(), pl)) {
			// 기존 시야에 있었는데 새 시야에 없는 경우
			actor->m_vl.lock();
			actor->m_view_set.erase(pl);
			actor->m_vl.unlock();
			send_remove_object(p_id, pl);
			//cout << "시야에서 사라짐: " << pl << endl;

			if (false == is_npc(pl)) {
				auto actor2 = get_actor(p_id);
				actor2->m_vl.lock();
				if (0 != actor2->m_view_set.count(p_id)) {
					actor2->m_view_set.erase(p_id);
					actor2->m_vl.unlock();
					send_remove_object(pl, p_id);
				} else {
					actor2->m_vl.unlock();
				}
			}
		}
	}
}

void do_timer() {
	using namespace chrono;
	for (;;) {
		timer_l.lock();
		if (false == timer_queue.empty() && timer_queue.top().start_time < system_clock::now()) {
			TIMER_EVENT ev = timer_queue.top();
			timer_queue.pop();
			timer_l.unlock();
			if (ev.e_type == OP_DELAY_SEND &&
				!actors[ev.object].session->sendExOverManager.hasSendData()) {
				continue;
			}

			if (is_npc(ev.object)) {
				//cout << "timer queue id: " << ev.object << " is active: "<< actors[ev.object].is_active << endl;
				if (!actors[ev.object].is_active) {
					continue;
				}
				auto over = &get_actor(ev.object)->npcOver;
				over->m_op = ev.e_type;
				memset(&over->m_over, 0, sizeof(over->m_over));

				PostQueuedCompletionStatus(h_iocp, 1, ev.object, &over->m_over);
			} else {
				auto over = &get_actor(ev.object)->session->sendExOverManager.get();
				over->m_op = ev.e_type;
				memcpy(over->m_packetbuf, &ev.target_id, sizeof(ev.target_id));
				if (ev.hasBuffer) {
					memcpy(over->m_packetbuf + sizeof(ev.target_id), ev.buffer, sizeof(ev.buffer));
				}
				PostQueuedCompletionStatus(h_iocp, 1, ev.object, &over->m_over);
			}
		} else {
			timer_l.unlock();
			this_thread::sleep_for(10ms);
		}
	}
}


int main() {
	for (int i = SERVER_ID + 1; i <= MAX_PLAYER; ++i) {
		auto& pl = actors[i];
		pl.id = i;
		pl.session = new SESSION;
		pl.session->m_state = PLST_FREE;
		pl.is_active = true;
	}
	for (int i = MAX_PLAYER + 1; i < MAX_USER; i++) {
		auto& npc = actors[i];
		npc.id = i;
		sprintf_s(npc.m_name, "N%d", npc.id);
		npc.x = rand() % WORLD_X_SIZE;
		npc.y = rand() % WORLD_Y_SIZE;
		npc.move_time = 0;
		auto sectorViewFrustumX = npc.x / WORLD_SECTOR_SIZE;
		auto sectorViewFrustumY = npc.y / WORLD_SECTOR_SIZE;
		memset(&npc.npcOver.m_over, 0, sizeof(npc.npcOver.m_over));
		npc.is_active = false;
		world_sector[sectorViewFrustumY][sectorViewFrustumX].m_sector.insert(npc.id);
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
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listenSocket), h_iocp, SERVER_ID, 0);
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	::bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
	listen(listenSocket, SOMAXCONN);

	EX_OVER accept_over;
	accept_over.m_op = OP_ACCEPT;
	memset(&accept_over.m_over, 0, sizeof(accept_over.m_over));
	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	accept_over.m_csocket = c_socket;
	BOOL ret = AcceptEx(listenSocket, c_socket,
		accept_over.m_packetbuf, 0, 32, 32, NULL, &accept_over.m_over);
	if (FALSE == ret) {
		int err_num = WSAGetLastError();
		if (err_num != WSA_IO_PENDING)
			display_error("AcceptEx Error", err_num);
	}

	cout << "서버 열림" << endl;

	thread timer_thread(do_timer);
	vector <thread> worker_threads;
	for (int i = 0; i < 4; ++i)
		worker_threads.emplace_back(worker, h_iocp, listenSocket);
	for (auto& th : worker_threads)
		th.join();
	closesocket(listenSocket);
	WSACleanup();
}
