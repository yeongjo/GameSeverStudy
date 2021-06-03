#include <iostream>
#include <unordered_map>
#include <thread>
#include <vector>
#include <mutex>
#include <array>
#include <queue>
#include <unordered_set>
using namespace std;
#include <WS2tcpip.h>
#include <MSWSock.h>
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

constexpr int32_t ceil_const(float num) {
	return (static_cast<float>(static_cast<int32_t>(num)) == num)
		? static_cast<int32_t>(num)
		: static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
}

constexpr int WORLD_SECTOR_SIZE = WORLD_WIDTH / 15;
constexpr int WORLD_SECTOR_X_COUNT = ceil_const(WORLD_WIDTH / (float)WORLD_SECTOR_SIZE);
constexpr int WORLD_SECTOR_Y_COUNT = ceil_const(WORLD_HEIGHT / (float)WORLD_SECTOR_SIZE);

enum OP_TYPE { OP_RECV, OP_SEND, OP_ACCEPT, OP_RANDOM_MOVE, OP_ATTACK, OP_DELAY_SEND };

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

constexpr size_t SENDEXOVER_INCREASEMENT_SIZE = 128;

/// <summary>
/// get으로 EX_OVER 꺼내서 쓰고, 쓴 EX_OVER를 ExOverUsableGroup로 형변환하고 recycle 호출하면 됨
/// </summary>
struct SendExOverManager {
	struct ExOverUsableGroup {
	private:
		EX_OVER ex_over;
		bool is_used = false;
		SendExOverManager* manager;

	public:
		ExOverUsableGroup(SendExOverManager* manager) : manager(manager) {

		}
		void setIsUsed(bool used) {
			is_used = used;
		}

		bool getIsUsed() const {
			return is_used;
		}

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
	vector<unsigned char> m_send_data;
	mutex m_send_data_lock;
	mutex m_send_over_vector_lock;

public:
	SendExOverManager() {
		m_send_over.resize(SENDEXOVER_INCREASEMENT_SIZE);
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
		lock_guard<mutex> lg{ m_send_data_lock };
		unsigned char p_size = reinterpret_cast<unsigned char*>(p)[0];

		auto prev_size = m_send_data.size();
		auto send_data_total_size = prev_size + static_cast<size_t>(p_size);
		m_send_data.resize(send_data_total_size);
		//if (prev_size > 20) {
		//	cout << "packet size: " << +p_size << "; prev_size: " << prev_size << "; send_data_total_size: " << send_data_total_size << endl;
		//}
		memcpy(static_cast<void*>(&m_send_data[prev_size]), p, p_size);
	}

	void clearSendData() {
		lock_guard<mutex> lg{ m_send_data_lock };
		m_send_data.clear();
		for (int i = 0; i < m_send_over.size(); ++i) {
			m_send_over[0]->setIsUsed(false);
		}
	}

	void sendAddedData(int p_id);

	EX_OVER& get() {
		ExOverUsableGroup* send_ex_over;
		{
			lock_guard<mutex> lg{ m_send_over_vector_lock };
			//return m_send_over[0].getExOver();
			size_t size = m_send_over.size();
			if (size == 0) {
				m_send_over.resize(size + SENDEXOVER_INCREASEMENT_SIZE);
				for (size_t i = size; i < size + SENDEXOVER_INCREASEMENT_SIZE; ++i) {
					m_send_over[i] = new ExOverUsableGroup(this);
				}
				size += SENDEXOVER_INCREASEMENT_SIZE;
			}
			send_ex_over = m_send_over[size - 1];
			m_send_over.pop_back();
		}
		send_ex_over->setIsUsed(true);
		return send_ex_over->getExOver();
	}

private:
	void recycle(ExOverUsableGroup* usableGroup) {
		usableGroup->setIsUsed(false);
		lock_guard<mutex> lg{ m_send_over_vector_lock };
		m_send_over.push_back(usableGroup);
	}
};

struct TIMER_EVENT {
	int object;
	OP_TYPE e_type;
	chrono::system_clock::time_point start_time;
	int target_id;

	constexpr bool operator< (const TIMER_EVENT& L) const {
		return (start_time > L.start_time);
	}
};

priority_queue<TIMER_EVENT> timer_queue;
unordered_set<int> timer_dequeue_objects;
mutex timer_l;
HANDLE h_iocp;

struct S_ACTOR {
	int		id;
	char m_name[200];
	short	x, y;
	atomic_bool is_active;
	vector<int> m_selected_sector;
	vector<int> m_old_view_list;
	unordered_set<int> m_view_set;
	mutex m_vl;
	NPC_OVER npcOver;
};

struct SESSION : public S_ACTOR {
	mutex  m_slock;
	atomic<PL_STATE> m_state;
	SOCKET m_socket;
	EX_OVER m_recv_over;
	
	int m_prev_size;
	int move_time;
	
	SendExOverManager sendExOverManager;
};


constexpr int SERVER_ID = 0;
array <S_ACTOR, MAX_NPC> npcs;
array <SESSION, MAX_PLAYER> players;
void disconnect(int p_id);
void do_npc_random_move(S_ACTOR& npc);

bool is_npc(int id) {
	return id > NPC_ID_START;
}

S_ACTOR* get_actor(int id) {
	return is_npc(id) ? &npcs[id - NPC_START - 1] : &players[id - 1];
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
	auto session = static_cast<SESSION*>(get_actor(p_id));
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

void add_event(int obj, OP_TYPE ev_t, int delay_ms) {
	using namespace chrono;
	TIMER_EVENT ev;
	ev.e_type = ev_t;
	ev.object = obj;
	ev.start_time = system_clock::now() + milliseconds(delay_ms);
	timer_l.lock();
	timer_queue.push(ev);
	timer_l.unlock();
}

void send_packet(int p_id, void* p) {
	add_event(p_id, OP_DELAY_SEND, 12);
	--p_id; // SERVER_ID때문에 id가 1부터 시작해서 배열에 쓸라면 1빼야함
	players[p_id].sendExOverManager.addSendData(p);
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
	--key; // SERVER_ID때문에 id가 1부터 시작해서 배열에 쓸라면 1빼야함
	players[key].m_recv_over.m_wsabuf[0].buf =
		reinterpret_cast<char*>(players[key].m_recv_over.m_packetbuf)
		+ players[key].m_prev_size;
	players[key].m_recv_over.m_wsabuf[0].len = MAX_BUFFER - players[key].m_prev_size;
	memset(&players[key].m_recv_over.m_over, 0, sizeof(players[key].m_recv_over.m_over));
	DWORD r_flag = 0;
	int ret = WSARecv(players[key].m_socket, players[key].m_recv_over.m_wsabuf, 1, NULL, &r_flag, &players[key].m_recv_over.m_over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("WSARecv : ", WSAGetLastError());
	}
}

int get_new_player_id(SOCKET p_socket) {
	for (int i = 0; i < MAX_PLAYER; ++i) {
		if (PLST_FREE == players[i].m_state) {
			lock_guard<mutex> lg{ players[i].m_slock };
			players[i].m_state = PLST_CONNECTED;
			players[i].m_socket = p_socket;
			players[i].m_name[0] = 0;
			return ++i; // SERVER_ID 때문에 +1된거 넘김
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

void send_move_packet(int c_id, int p_id) {
	s2c_move_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_MOVE_PLAYER;
	auto actor = get_actor(p_id);
	p.x = actor->x;
	p.y = actor->y;
	p.move_time = is_npc(p_id) ? chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count() :
		static_cast<SESSION*>(actor)->move_time;
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
	return VIEW_RADIUS >= 
		abs(actor_a->x - actor_b->x) + abs(actor_a->y - actor_b->y);
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
		if (isNpc == is_npc(id) || !can_see(p_id, id) || id == p_id || (!is_npc(id) && players[id].m_state != PLST_INGAME)) {
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
	auto actor = static_cast<SESSION*>(get_actor(p_id));
	int y = actor->y;
	int x = actor->x;
	auto sector_y = y / WORLD_SECTOR_SIZE;
	auto sector_x = x / WORLD_SECTOR_SIZE;
	auto& main_sector = world_sector[sector_y][sector_x];

	actor->m_selected_sector.clear();
	main_sector.m_sector_lock.lock();
	for (auto iter : main_sector.m_sector) {
		auto iter_is_npc = is_npc(iter);
		if (iter != actor->id &&
			(iter_is_npc || 
				!iter_is_npc && static_cast<SESSION*>(get_actor(iter))->m_state == PLST_INGAME)&&
			can_see(iter, actor->id)) {
			actor->m_selected_sector.push_back(iter);
		}
	}
	main_sector.m_sector_lock.unlock();
	auto& selected_sector = actor->m_selected_sector;

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
	if(actor->is_active == false){
		bool old_state = false;
		if(true == atomic_compare_exchange_strong(&actor->is_active, &old_state, true)){
			timer_dequeue_objects.erase(id);
			add_event(id, OP_RANDOM_MOVE, 1000);
		}
	}
}

void sleep_npc(int id) {
	auto actor = get_actor(id);
	if (actor->is_active == true) {
		bool old_state = true;
		if (true == atomic_compare_exchange_strong(&actor->is_active, &old_state, false)) {
			timer_dequeue_objects.insert(id);
		}
	}
}

void do_move(int p_id, DIRECTION dir);


void process_packet(int p_id, unsigned char* p_buf) {
	switch (p_buf[1]) {
	case C2S_LOGIN: {
		c2s_login* packet = reinterpret_cast<c2s_login*>(p_buf);
		auto session = static_cast<SESSION*>(get_actor(p_id));
		session->m_state = PLST_INGAME;
		{
			lock_guard <mutex> gl1{ session->m_slock };
			strcpy_s(session->m_name, packet->name);
			//session->x = 1;
			//session->y = 1;
			session->x = rand() % WORLD_X_SIZE;
			session->y = rand() % WORLD_Y_SIZE;
		}
		{
			auto& sector = world_sector[session->y / WORLD_SECTOR_SIZE][session->x / WORLD_SECTOR_SIZE];
			lock_guard <mutex> gl2{ sector.m_sector_lock };
			sector.m_sector.insert(p_id);
		}
		send_login_ok_packet(p_id);

		auto&& selected_sector = get_id_from_overlapped_sector(p_id);

		session->m_vl.lock();
		for (auto id : selected_sector) {
			session->m_view_set.insert(id);
		}
		session->m_vl.unlock();
		for (auto id : selected_sector) {
			auto actor = get_actor(id);
			auto& pl = actor;
			send_add_object(p_id, id);
			if (!is_npc(id)) {
				send_add_object(id, p_id);
				auto session1 = static_cast<SESSION*>(actor);
				session1->m_vl.lock();
				session1->m_view_set.insert(p_id);
				session1->m_vl.unlock();
			} else {
				wake_up_npc(actor->id);
			}
		}
		break;
	}
	case C2S_MOVE: {
		c2s_move* packet = reinterpret_cast<c2s_move*>(p_buf);
		auto session = static_cast<SESSION*>(get_actor(p_id));
		session->move_time = packet->move_time;
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
	auto session = static_cast<SESSION*>(get_actor(p_id));
	if (session->m_state == PLST_FREE) {
		return;
	}
	closesocket(session->m_socket);
	session->m_state = PLST_FREE;

	// remove from sector
	session->sendExOverManager.clearSendData();
	int y = session->y;
	int x = session->x;
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

	session->m_vl.lock();
	session->m_old_view_list.resize(session->m_view_set.size());
	std::copy(session->m_view_set.begin(), session->m_view_set.end(), session->m_old_view_list.begin());
	session->m_view_set.clear();
	session->m_vl.unlock();
	auto& m_old_view_list = session->m_old_view_list;
	for (auto pl : m_old_view_list) {
		if (is_npc(pl)) {
			break;
		}
		auto session2 = static_cast<SESSION*>(get_actor(p_id));
		if (PLST_INGAME == session2->m_state){
			send_remove_object(session2->id, p_id);
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
			auto session = static_cast<SESSION*>(get_actor(key));
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
			auto session = static_cast<SESSION*>(get_actor(c_id));
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
	switch(rand() % 4){
	case 0: if (x < WORLD_X_SIZE-1) ++x; break;
	case 1: if(x >0) --x; break;
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

	if(new_vl.empty()){
		sleep_npc(npc.id); // 아무도 보이지 않으므로 취침
		return;
	}
	for(auto pl : new_vl){
		if(is_npc(pl)){
			continue;
		}
		if(oldViewList.end() == std::find(oldViewList.begin(), oldViewList.end(), pl)){
			// 플레이어의 시야에 등장
			auto session = static_cast<SESSION*>(get_actor(pl));
			session->m_vl.lock();
			session->m_view_set.insert(npc.id);
			session->m_vl.unlock();
			send_add_object(pl, npc.id);
		}else{
			// 플레이어가 계속 보고있음.
			send_move_packet(pl, npc.id);
		}
	}
	for (auto pl : oldViewList){
		if (is_npc(pl)) {
			continue;
		}
		if(new_vl.end() == std::find(new_vl.begin(), new_vl.end(), pl)){
			auto session = static_cast<SESSION*>(get_actor(pl));
			session->m_vl.lock();
			if (session->m_view_set.count(pl) != 0) {
				session->m_view_set.erase(pl);
				session->m_vl.unlock();
				send_remove_object(pl, npc.id);
			} else {
				session->m_vl.unlock();
			}
		}
	}
}


void do_move(int p_id, DIRECTION dir) {
	auto session = static_cast<SESSION*>(get_actor(p_id));
	auto& x = session->x;
	auto& y = session->y;
	auto prevX = x;
	auto prevY = y;
	switch (dir){
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

	session->m_vl.lock();
	auto view_set_size = session->m_view_set.size();
	session->m_old_view_list.resize(view_set_size);
	if(view_set_size > 0){
		std::copy(session->m_view_set.begin(), session->m_view_set.end(), session->m_old_view_list.begin());
	}
	auto& m_old_view_list = session->m_old_view_list;
	session->m_vl.unlock();
	auto&& new_vl = get_id_from_overlapped_sector(p_id);

	for (auto pl : new_vl){
		if (m_old_view_list.end() == std::find(m_old_view_list.begin(), m_old_view_list.end(), pl)){
			//1. 새로 시야에 들어오는 플레이어
			session->m_vl.lock();
			session->m_view_set.insert(pl);
			session->m_vl.unlock();
			send_add_object(p_id, pl);

			if (false == is_npc(pl)){
				auto session2 = static_cast<SESSION*>(get_actor(p_id));
				session2->m_vl.lock();
				if (0 == session2->m_view_set.count(p_id)){
					session2->m_view_set.insert(p_id);
					session2->m_vl.unlock();
					send_add_object(pl, p_id);
					//cout << "시야추가1: " << pl << ", " << p_id << endl;
				}
				else{
					session2->m_vl.unlock();
					send_move_packet(pl, p_id);
				}
			}
			else{
				wake_up_npc(pl);
			}
		}
		else{
			//2. 기존 시야에도 있고 새 시야에도 있는 경우
			if (false == is_npc(pl)){
				auto session2 = static_cast<SESSION*>(get_actor(p_id));
				session2->m_vl.lock();
				if (0 == session2->m_view_set.count(p_id)){
					session2->m_view_set.insert(p_id);
					session2->m_vl.unlock();
					send_add_object(pl, p_id);
					//cout << "시야추가2: " << pl << ", " << p_id << endl;
				}
				else{
					session2->m_vl.unlock();
					send_move_packet(p_id, pl);
				}
			}
		}
	}
	for (auto pl : m_old_view_list){
		if (new_vl.end() == std::find(new_vl.begin(), new_vl.end(), pl)){
			// 기존 시야에 있었는데 새 시야에 없는 경우
			session->m_vl.lock();
			session->m_view_set.erase(pl);
			session->m_vl.unlock();
			send_remove_object(p_id, pl);
			//cout << "시야에서 사라짐: " << pl << endl;

			if (false == is_npc(pl)){
				auto session2 = static_cast<SESSION*>(get_actor(p_id));
				session2->m_vl.lock();
				if (0 != session2->m_view_set.count(p_id)){
					session2->m_view_set.erase(p_id);
					session2->m_vl.unlock();
					send_remove_object(pl, p_id);
				}
				else{
					session2->m_vl.unlock();
				}
			}
		}
	}
}

void do_timer() {
	using namespace chrono;
	for(;;){
		timer_l.lock();
		if (false == timer_queue.empty() && timer_queue.top().start_time < system_clock::now()){
			TIMER_EVENT ev = timer_queue.top();
			timer_queue.pop();
			timer_l.unlock();
			if(timer_dequeue_objects.count(ev.object) != 0){
				continue;
			}
			if(ev.e_type == OP_DELAY_SEND && 
				!players[ev.object - 1].sendExOverManager.hasSendData()){
				continue;
			}
			if(is_npc(ev.object)){
				auto over = &get_actor(ev.object)->npcOver;
				over->m_op = ev.e_type;
				memset(&over->m_over, 0, sizeof(over->m_over));

				PostQueuedCompletionStatus(h_iocp, 1, ev.object, &over->m_over);
			}else{
				auto over = &static_cast<SESSION*>(get_actor(ev.object))->sendExOverManager.get();
				over->m_op = ev.e_type;
				PostQueuedCompletionStatus(h_iocp, 1, ev.object, &over->m_over);
			}
		}else{
			timer_l.unlock();
			this_thread::sleep_for(10ms);
		}
	}
}


int main() {
	for (int i = 0; i < MAX_PLAYER; ++i) {
		auto& pl = players[i];
		pl.id = i+1;
		pl.m_state = PLST_FREE;
	}
	for (int i = 0; i < MAX_NPC; i++) {
		auto& npc = npcs[i];
		npc.id = i + NPC_START + 1;
		sprintf_s(npc.m_name, "N%d", npc.id);
		npc.x = rand() % WORLD_X_SIZE;
		npc.y = rand() % WORLD_Y_SIZE;
		auto sectorViewFrustumX = npc.x / WORLD_SECTOR_SIZE;
		auto sectorViewFrustumY = npc.y / WORLD_SECTOR_SIZE;
		memset(&npc.npcOver.m_over, 0, sizeof(npc.npcOver.m_over));
		world_sector[sectorViewFrustumY][sectorViewFrustumX].m_sector.insert(npc.id);
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
