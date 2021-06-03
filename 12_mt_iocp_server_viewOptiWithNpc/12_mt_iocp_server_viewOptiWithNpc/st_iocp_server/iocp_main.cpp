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

enum OP_TYPE { OP_RECV, OP_SEND, OP_ACCEPT, OP_RANDOM_MOVE, OP_ATTACK };
struct EX_OVER {
	WSAOVERLAPPED	m_over;
	WSABUF			m_wsabuf[1];
	unsigned char	m_packetbuf[MAX_BUFFER];
	OP_TYPE			m_op;
	SOCKET			m_csocket;					// OP_ACCEPT에서만 사용
};

enum PL_STATE { PLST_FREE, PLST_CONNECTED, PLST_INGAME };

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

struct S_OBJECT {
	mutex  m_slock;
	atomic<PL_STATE> m_state;
	SOCKET m_socket;
	int		id;

	EX_OVER m_recv_over;
	int m_prev_size;
	atomic_bool is_active;

	char m_name[200];
	short	x, y;
	int move_time;
	unordered_set<int> m_view_list;
	mutex m_vl;

};

constexpr int SERVER_ID = 0;
int VIEW_RADIUS = 5;
array <S_OBJECT, MAX_USER + 1> objects;
void disconnect(int p_id);
void do_npc_random_move(S_OBJECT& npc);

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

bool is_npc(int id) {
	return id >= NPC_START;
}

void display_error(const char* msg, int err_no) {
	WCHAR* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	//cout << msg;
	//wcout << lpMsgBuf << endl;
	LocalFree(lpMsgBuf);
}
void send_packet(int p_id, void* p) {
	int p_size = reinterpret_cast<unsigned char*>(p)[0];
	int p_type = reinterpret_cast<unsigned char*>(p)[1];
	//cout << "To client [" << p_id << "] : ";
	//cout << "Packet [" << p_type << "]\n";
	EX_OVER* s_over = new EX_OVER;
	s_over->m_op = OP_SEND;
	memset(&s_over->m_over, 0, sizeof(s_over->m_over));
	memcpy(s_over->m_packetbuf, p, p_size);
	s_over->m_wsabuf[0].buf = reinterpret_cast<CHAR*>(s_over->m_packetbuf);
	s_over->m_wsabuf[0].len = p_size;
	int ret = WSASend(objects[p_id].m_socket, s_over->m_wsabuf, 1,
		NULL, 0, &s_over->m_over, 0);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) {
			display_error("WSASend : ", WSAGetLastError());
			disconnect(p_id);
		}
	}
}

void do_recv(int key) {

	objects[key].m_recv_over.m_wsabuf[0].buf =
		reinterpret_cast<char*>(objects[key].m_recv_over.m_packetbuf)
		+ objects[key].m_prev_size;
	objects[key].m_recv_over.m_wsabuf[0].len = MAX_BUFFER - objects[key].m_prev_size;
	memset(&objects[key].m_recv_over.m_over, 0, sizeof(objects[key].m_recv_over.m_over));
	DWORD r_flag = 0;
	int ret = WSARecv(objects[key].m_socket, objects[key].m_recv_over.m_wsabuf, 1,
		NULL, &r_flag, &objects[key].m_recv_over.m_over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("WSARecv : ", WSAGetLastError());
	}
}

int get_new_player_id(SOCKET p_socket) {
	for (int i = SERVER_ID + 1; i <= MAX_USER; ++i) {
		lock_guard<mutex> lg{ objects[i].m_slock };
		if (PLST_FREE == objects[i].m_state) {
			objects[i].m_state = PLST_CONNECTED;
			objects[i].m_socket = p_socket;
			objects[i].m_name[0] = 0;
			return i;
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
	p.x = objects[p_id].x;
	p.y = objects[p_id].y;
	send_packet(p_id, &p);
}

void send_move_packet(int c_id, int p_id) {
	s2c_move_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_MOVE_PLAYER;
	p.x = objects[p_id].x;
	p.y = objects[p_id].y;
	p.move_time = objects[p_id].move_time;
	send_packet(c_id, &p);
}

void send_add_objects(int c_id, int p_id) {
	s2c_add_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_ADD_PLAYER;
	p.x = objects[p_id].x;
	p.y = objects[p_id].y;
	p.race = 0;
	strcpy_s(p.name, objects[p_id].m_name);
	send_packet(c_id, &p);
}

void send_remove_object(int c_id, int p_id) {
	s2c_remove_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_REMOVE_PLAYER;
	send_packet(c_id, &p);
}

bool can_see(int id_a, int id_b) {
	return VIEW_RADIUS >= abs(objects[id_a].x - objects[id_b].x) + abs(objects[id_a].y - objects[id_b].y);
}

void wake_up_npc(int id) {
	if(objects[id].is_active == false){
		bool old_state = false;
		if(true == atomic_compare_exchange_strong(&objects[id].is_active, &old_state, true)){
			timer_dequeue_objects.erase(id);
			add_event(id, OP_RANDOM_MOVE, 1000);
		}
	}
}

void sleep_npc(int id) {
	if (objects[id].is_active == true) {
		bool old_state = true;
		if (true == atomic_compare_exchange_strong(&objects[id].is_active, &old_state, false)) {
			timer_dequeue_objects.insert(id);
		}
	}
}

void do_move(int p_id, DIRECTION dir) {
	auto& x = objects[p_id].x;
	auto& y = objects[p_id].y;
	switch (dir) {
	case D_N: if (y > 0) y--; break;
	case D_S: if (y < (WORLD_Y_SIZE - 1)) y++; break;
	case D_W: if (x > 0) x--; break;
	case D_E: if (x < (WORLD_X_SIZE - 1)) x++; break;
	}
	for (auto& pl : objects) {
		//lock_guard <mutex> lg (pl.m_slock);
		if (PLST_INGAME == pl.m_state)
			send_move_packet(pl.id, p_id);
	}
	unordered_set<int> old_vl;
	objects[p_id].m_vl.lock();
	old_vl = objects[p_id].m_view_list;
	objects[p_id].m_vl.unlock();
	unordered_set<int> new_vl;
	for (auto& pl : objects) {
		if (pl.id == p_id) continue;
		if (pl.m_state == PLST_INGAME && can_see(p_id, pl.id)) {
			new_vl.insert(pl.id);
		}
	}
	send_move_packet(p_id, p_id);
	for (auto pl : new_vl) {
		if (0 == old_vl.count(pl)) {	//1. 새로 시야에 들어오는 객체
			objects[p_id].m_vl.lock();
			objects[p_id].m_view_list.insert(pl);
			objects[p_id].m_vl.unlock();
			send_add_objects(p_id, pl);

			if(false == is_npc(pl)){
				objects[pl].m_vl.lock();
				if (0 == objects[pl].m_view_list.count(p_id)) {
					objects[pl].m_view_list.insert(p_id);
					objects[pl].m_vl.unlock();
					send_add_objects(pl, p_id);
				} else {
					objects[pl].m_vl.unlock();
					send_move_packet(pl, p_id);
				}
			} else {
				wake_up_npc(p_id);
			}
		} else {						//2. 기존 시야에도 있고 새 시야에도 있는 경우
			if (false == is_npc(pl)) {
				objects[pl].m_vl.lock();
				if (0 == objects[pl].m_view_list.count(p_id)) {
					objects[pl].m_view_list.insert(p_id);
					objects[pl].m_vl.unlock();
					send_add_objects(pl, p_id);
				} else {
					objects[pl].m_vl.unlock();
					send_move_packet(pl, p_id);
				}
			}
		}
	}
	for (auto pl : old_vl) {
		if (0 == new_vl.count((pl))) { // TODO 움직일때 remove 되는듯
			// 기존 시야에 있었는데 새 시야에 없는 경우
			objects[p_id].m_vl.lock();
			objects[p_id].m_view_list.erase(pl);
			objects[p_id].m_vl.unlock();
			send_remove_object(p_id, pl);

			if (false == is_npc(pl)) {
				objects[pl].m_vl.lock();
				if (0 != objects[pl].m_view_list.count(p_id)) {
					objects[pl].m_view_list.erase(p_id);
					objects[pl].m_vl.unlock();
					send_remove_object(pl, p_id);
				} else {
					objects[pl].m_vl.unlock();

				}
			}else{
				sleep_npc(pl);
			}
		}
	}
}


void process_packet(int p_id, unsigned char* p_buf) {
	switch (p_buf[1]) {
	case C2S_LOGIN: {
		c2s_login* packet = reinterpret_cast<c2s_login*>(p_buf);
		{
			lock_guard <mutex> gl2{ objects[p_id].m_slock };
			strcpy_s(objects[p_id].m_name, packet->name);
			objects[p_id].x = rand() % WORLD_X_SIZE;
			objects[p_id].y = rand() % WORLD_Y_SIZE;
			objects[p_id].m_state = PLST_INGAME;
		}
		send_login_ok_packet(p_id);

		for (auto& pl : objects) {
			if (p_id != pl.id) {
				lock_guard <mutex> gl{ pl.m_slock };
				if (PLST_INGAME == pl.m_state) {
					if (can_see(p_id, pl.id)) {
						objects[p_id].m_vl.lock();
						objects[p_id].m_view_list.insert(pl.id);
						objects[p_id].m_vl.unlock();
						send_add_objects(pl.id, p_id);

						if(!is_npc(pl.id)){
							objects[pl.id].m_vl.lock();
							objects[pl.id].m_view_list.insert(p_id);
							objects[pl.id].m_vl.unlock();
							send_add_objects(p_id, pl.id);
						}else{
							wake_up_npc(pl.id);
						}
					}
				}
			}
		}


	}
				  break;
	case C2S_MOVE: {
		c2s_move* packet = reinterpret_cast<c2s_move*>(p_buf);
		objects[p_id].move_time = packet->move_time;
		do_move(p_id, packet->dr);
	}
				 break;
	default:
		cout << "Unknown Packet Type from Client[" << p_id;
		cout << "] Packet Type [" << p_buf[1] << "]";
		while (true);
	}
}

void disconnect(int p_id) {
	if (objects[p_id].m_state == PLST_FREE) {
		return;
	}
	closesocket(objects[p_id].m_socket);
	objects[p_id].m_state = PLST_FREE;

	for (auto& pl : objects) {
		if (false == is_npc(pl.id)) {
			break;
		}
		if (PLST_INGAME == pl.m_state)
			send_remove_object(pl.id, p_id);
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
			int num_data = num_bytes + objects[key].m_prev_size;
			int packet_size = packet_ptr[0];

			while (num_data >= packet_size) {
				process_packet(key, packet_ptr);
				num_data -= packet_size;
				packet_ptr += packet_size;
				if (0 >= num_data) break;
				packet_size = packet_ptr[0];
			}
			objects[key].m_prev_size = num_data;
			if (0 != num_data)
				memcpy(ex_over->m_packetbuf, packet_ptr, num_data);
			do_recv(key);
			break;
		}
		case OP_SEND:
			delete ex_over;
			break;
		case OP_ACCEPT: {
			int c_id = get_new_player_id(ex_over->m_csocket);
			if (-1 != c_id) {
				objects[c_id].m_recv_over.m_op = OP_RECV;
				objects[c_id].m_prev_size = 0;
				CreateIoCompletionPort(
					reinterpret_cast<HANDLE>(objects[c_id].m_socket), h_iocp, c_id, 0);
				do_recv(c_id);
			} else {
				closesocket(objects[c_id].m_socket);
			}

			memset(&ex_over->m_over, 0, sizeof(ex_over->m_over));
			SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			ex_over->m_csocket = c_socket;
			AcceptEx(l_socket, c_socket,
				ex_over->m_packetbuf, 0, 32, 32, NULL, &ex_over->m_over);
			break;
		}
		case OP_RANDOM_MOVE: {
			// npc가 주변에 없으면 안움직이는거도 만들어야한다.
			do_npc_random_move(objects[key]);
			add_event(key, OP_RANDOM_MOVE, 1000);
			delete ex_over;
			break;
		}
		case OP_ATTACK:
			delete ex_over;
			break;
		}
	}
}

void do_npc_random_move(S_OBJECT& npc) {
	unordered_set<int> old_vl;
	for(auto &obj : objects){
		if(PLST_INGAME != obj.m_state){
			continue;
		}
		if (!can_see(obj.id, npc.id)) {
			continue;
		}
		if(true == is_npc(obj.id)){
			break;
		}
		old_vl.insert(obj.id);
	}
	
	auto& x = npc.x;
	auto& y = npc.y;
	switch(rand() % 4){
	case 0:
		x = (x < WORLD_X_SIZE-1) ? ++x : x;
		break;
	case 1:
		x = (x >0) ? --x : x;
		break;
	case 2:
		y = (y < WORLD_Y_SIZE - 1) ? ++y : y;
		break;
	case 3:
		y = (y > 0) ? --y : y;
		break;
	}


	unordered_set<int> new_vl;
	for (auto& obj : objects) {
		if (PLST_INGAME != obj.m_state) {
			continue;
		}
		if (!can_see(obj.id, npc.id)) {
			continue;
		}
		if (true == is_npc(obj.id)) {
			break;
		}
		new_vl.insert(obj.id);
	}

	for(auto pl : new_vl){
		if(0 == old_vl.count(pl)){
			// 플레이어의 시야에 등장
			objects[pl].m_vl.lock();
			objects[pl].m_view_list.insert(npc.id);
			objects[pl].m_vl.unlock();
			send_add_objects(pl, npc.id);
		}else{
			// 플레이어가 계속 보고있음.
			send_move_packet(pl, npc.id);
		}
	}
	for (auto pl : old_vl){
		if(0 == new_vl.count(pl)){
			objects[pl].m_vl.lock();
			if(objects[pl].m_view_list.count(pl) != 0){
				objects[pl].m_view_list.erase(pl);
				send_remove_object(pl, npc.id);
				objects[pl].m_vl.unlock();
			} else {
				objects[pl].m_vl.unlock();
			}
		}
	}
}

void do_ai() {
	using namespace chrono;
	for (;;) {
		auto start_t = system_clock::now();
		for (auto& npc : objects) {
			if (true == is_npc(npc.id)) {
				do_npc_random_move(npc);
			}
		}
		auto end_t = system_clock::now();
		auto ai_time = end_t - start_t;
		cout << "AI Exec Time: " << 
			duration_cast<milliseconds>(ai_time).count() << 
			"ms.\n";
		if(end_t < start_t + 1s){
			this_thread::sleep_for(start_t + seconds(1) - end_t);
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
				return;
			}
			EX_OVER* ex_over = new EX_OVER;
			ex_over->m_op = OP_RANDOM_MOVE;
			PostQueuedCompletionStatus(h_iocp, 1, ev.object, (LPOVERLAPPED)ex_over);
		}else{
			timer_l.unlock();
			this_thread::sleep_for(10ms);
		}
	}
}


int main() {
	for (int i = 0; i < MAX_USER + 1; ++i) {
		auto& pl = objects[i];
		pl.id = i;
		pl.m_state = PLST_FREE;
		if (true == is_npc(i)) {
			sprintf_s(pl.m_name, "N%d", i);
			pl.m_state = PLST_INGAME;
			pl.x = rand() % WORLD_X_SIZE;
			pl.y = rand() % WORLD_Y_SIZE;
		}
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

	//thread ai_thread(do_ai);
	thread timer_thread(do_timer);
	vector <thread> worker_threads;
	for (int i = 0; i < 4; ++i)
		worker_threads.emplace_back(worker, h_iocp, listenSocket);
	for (auto& th : worker_threads)
		th.join();
	closesocket(listenSocket);
	WSACleanup();
}
