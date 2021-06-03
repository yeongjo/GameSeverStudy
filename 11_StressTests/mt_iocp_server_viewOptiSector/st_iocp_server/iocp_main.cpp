#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <thread>
#include <vector>
#include <mutex>
#include <array>
#include <unordered_set>
using namespace std;
#include <WS2tcpip.h>
#include <MSWSock.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")

#include "protocol.h"

constexpr int32_t ceil_const(float num) {
	return (static_cast<float>(static_cast<int32_t>(num)) == num)
		? static_cast<int32_t>(num)
		: static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
}

constexpr int WORLD_SECTOR_SIZE = WORLD_X_SIZE / 15;
constexpr int WORLD_SECTOR_X_COUNT = ceil_const(WORLD_X_SIZE / (float)WORLD_SECTOR_SIZE);
constexpr int WORLD_SECTOR_Y_COUNT = ceil_const(WORLD_Y_SIZE / (float)WORLD_SECTOR_SIZE);

enum OP_TYPE { OP_RECV, OP_SEND, OP_ACCEPT };
struct EX_OVER {
	WSAOVERLAPPED	m_over;
	WSABUF			m_wsabuf[1];
	unsigned char	m_packetbuf[MAX_BUFFER];
	OP_TYPE			m_op;
	SOCKET			m_csocket;					// OP_ACCEPT에서만 사용
};

enum PL_STATE { PLST_FREE, PLST_CONNECTED, PLST_INGAME };

constexpr size_t SENDEXOVER_INCREASEMENT_SIZE = 128;

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

	void recycle(ExOverUsableGroup* usableGroup) {
		usableGroup->setIsUsed(false);
		lock_guard<mutex> lg{ m_send_over_vector_lock };
		m_send_over.push_back(usableGroup);
	}
};

struct SESSION {
	mutex  m_slock;
	atomic<PL_STATE> m_state;
	SOCKET m_socket;
	int		id;

	EX_OVER m_recv_over;
	int m_prev_size;

	char m_name[200];
	short	x, y;
	int move_time;
	unordered_set<int> m_view_list;
	vector<int> m_old_view_list;
	mutex m_vl;
	vector<int> m_selected_sector;
	SendExOverManager sendExOverManager;
};

constexpr int SERVER_ID = 0;
int VIEW_RADIUS = 5;
array <SESSION, MAX_USER + 1> objects;

struct SECTOR {
	unordered_set<int> m_sector;
	mutex m_sector_lock;
};
array <array <SECTOR, WORLD_SECTOR_X_COUNT>, WORLD_SECTOR_Y_COUNT> world_sector;

void disconnect(int p_id);

void display_error(const char* msg, int err_no) {
	//WCHAR* lpMsgBuf;
	//FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
	//	NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	//	(LPTSTR)&lpMsgBuf, 0, NULL);
	//cout << msg;
	//wcout << lpMsgBuf << endl;
	//if(lpMsgBuf != NULL){
	//	LocalFree(lpMsgBuf);
	//}
}
void send_packet(int p_id, void* p, bool dont_call_disconnect = false) {
	int p_size = reinterpret_cast<unsigned char*>(p)[0];
	int p_type = reinterpret_cast<unsigned char*>(p)[1];
	//cout << "To client [" << p_id << "] : ";
	//cout << "Packet [" << p_type << "]\n";
	//EX_OVER* s_over = new EX_OVER;
	EX_OVER* s_over = &objects[p_id].sendExOverManager.get();
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
			if(!dont_call_disconnect){
				disconnect(p_id);
			}
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

bool can_see(int id_a, int id_b) {
	return VIEW_RADIUS >= abs(objects[id_a].x - objects[id_b].x) && VIEW_RADIUS >= abs(objects[id_a].y - objects[id_b].y);
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
	for (auto id : other_set) {
		if (id == p_id || objects[id].m_state != PLST_INGAME || !can_see(p_id, id)) {
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
vector<int>& get_overlapped_sector(int p_id) {
	int y = objects[p_id].y;
	int x = objects[p_id].x;
	auto sector_y = y / WORLD_SECTOR_SIZE;
	auto sector_x = x / WORLD_SECTOR_SIZE;
	auto& main_sector = world_sector[sector_y][sector_x];

	int i = 0;
	main_sector.m_sector_lock.lock();
	objects[p_id].m_selected_sector.clear();
	for (auto iter : main_sector.m_sector){
		if (iter != p_id && objects[iter].m_state == PLST_INGAME && can_see(iter, p_id)){
			objects[p_id].m_selected_sector.push_back(iter);
		}
	}
	main_sector.m_sector_lock.unlock();
	auto& selected_sector = objects[p_id].m_selected_sector;

	auto sector_view_frustum_top = (y - VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sector_view_frustum_bottom = (y + VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sector_view_frustum_left = (x - VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	auto sector_view_frustum_right = (x + VIEW_RADIUS) / WORLD_SECTOR_SIZE;
	if (sector_view_frustum_top != sector_y && 0 < sector_y){
		add_sector_players_to_main_sector(p_id, sector_view_frustum_top, sector_x, selected_sector);
		if (sector_view_frustum_left != sector_x && 0 < sector_x){
			add_sector_players_to_main_sector(p_id, sector_view_frustum_top, sector_view_frustum_left, selected_sector);
		}
		else if (sector_view_frustum_right != sector_x && sector_x < static_cast<int>(world_sector[
			sector_view_frustum_top].size() - 1)){
			add_sector_players_to_main_sector(p_id, sector_view_frustum_top, sector_view_frustum_right,
			                                  selected_sector);
		}
	}
	else if (sector_view_frustum_bottom != sector_y && sector_y < static_cast<int>(world_sector.size() - 1)){
		add_sector_players_to_main_sector(p_id, sector_view_frustum_bottom, sector_x, selected_sector);
		if (sector_view_frustum_left != sector_x && 0 < sector_x){
			add_sector_players_to_main_sector(p_id, sector_view_frustum_bottom, sector_view_frustum_left,
			                                  selected_sector);
		}
		else if (sector_view_frustum_right != sector_x && sector_x < static_cast<int>(world_sector[
			sector_view_frustum_bottom].size() - 1)){
			add_sector_players_to_main_sector(p_id, sector_view_frustum_bottom, sector_view_frustum_right,
			                                  selected_sector);
		}
	}
	if (sector_view_frustum_left != sector_x && 0 < sector_x){
		add_sector_players_to_main_sector(p_id, sector_y, sector_view_frustum_left, selected_sector);
	}
	else if (sector_view_frustum_right != sector_x && sector_x < static_cast<int>(world_sector[sector_y].size() - 1)){
		add_sector_players_to_main_sector(p_id, sector_y, sector_view_frustum_right, selected_sector);
	}
	return selected_sector;
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

void send_add_object(int c_id, int p_id) {
	s2c_add_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_ADD_PLAYER;
	p.x = objects[p_id].x;
	p.y = objects[p_id].y;
	p.race = 0;
	send_packet(c_id, &p);
}

void send_remove_object(int c_id, int p_id, bool dont_call_disconnect = false) {
	s2c_remove_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_REMOVE_PLAYER;
	send_packet(c_id, &p, dont_call_disconnect);
}

void do_move(int p_id, DIRECTION dir) {
	auto& x = objects[p_id].x;
	auto& y = objects[p_id].y;
	auto sector_view_frustum_y = y / WORLD_SECTOR_SIZE;
	auto sector_view_frustum_x = x / WORLD_SECTOR_SIZE;
	switch (dir) {
	case D_N: if (y > 0) y--; break;
	case D_S: if (y < (WORLD_Y_SIZE - 1)) y++; break;
	case D_W: if (x > 0) x--; break;
	case D_E: if (x < (WORLD_X_SIZE - 1)) x++; break;
	}
	auto new_sector_view_frustum_y = y / WORLD_SECTOR_SIZE;
	auto new_sector_view_frustum_x = x / WORLD_SECTOR_SIZE;
	if(new_sector_view_frustum_y != sector_view_frustum_y || new_sector_view_frustum_x != sector_view_frustum_x){
		// 원래 섹터랑 다르면 다른 섹터로 이동한 것임
		{
			lock_guard <mutex> gl2{ world_sector[sector_view_frustum_y][sector_view_frustum_x].m_sector_lock };
			world_sector[sector_view_frustum_y][sector_view_frustum_x].m_sector.erase(p_id);
		}
		{
			lock_guard <mutex> gl2{ world_sector[new_sector_view_frustum_y][new_sector_view_frustum_x].m_sector_lock };
			world_sector[new_sector_view_frustum_y][new_sector_view_frustum_x].m_sector.insert(p_id);
		}
	}
	
	objects[p_id].m_vl.lock();
	objects[p_id].m_old_view_list.resize(objects[p_id].m_view_list.size());
	//memcpy(&*objects[p_id].m_old_view_list.begin(), &*objects[p_id].m_view_list.begin(), objects[p_id].m_old_view_list.size() * sizeof(int));
	std::copy(objects[p_id].m_view_list.begin(), objects[p_id].m_view_list.end(), objects[p_id].m_old_view_list.begin());
	auto& m_old_view_list = objects[p_id].m_old_view_list;
	objects[p_id].m_vl.unlock();
	auto&& new_vl = get_overlapped_sector(p_id);

	send_move_packet(p_id, p_id);
	for (auto pl : new_vl) {
		if (m_old_view_list.end() == std::find(m_old_view_list.begin(), m_old_view_list.end(), pl)) {	//1. 새로 시야에 들어오는 플레이어
			objects[p_id].m_vl.lock();
			objects[p_id].m_view_list.insert(pl);
			objects[p_id].m_vl.unlock();
			send_add_object(p_id, pl);

			objects[pl].m_vl.lock();
			if (0 == objects[pl].m_view_list.count(p_id)) {
				objects[pl].m_view_list.insert(p_id);
				objects[pl].m_vl.unlock();
				send_add_object(pl, p_id);
				//cout << "시야추가1: " << pl << ", " << p_id << endl;
			} else {
				objects[pl].m_vl.unlock();
				send_move_packet(pl, p_id);
			}
		} else {						//2. 기존 시야에도 있고 새 시야에도 있는 경우
			objects[pl].m_vl.lock();
			if (0 == objects[pl].m_view_list.count(p_id)) {
				objects[pl].m_view_list.insert(p_id);
				objects[pl].m_vl.unlock();
				send_add_object(pl, p_id);
				//cout << "시야추가2: " << pl << ", " << p_id << endl;
			} else {
				objects[pl].m_vl.unlock();
				send_move_packet(pl, p_id);
			}
		}
	}
	for (auto pl : m_old_view_list) {
		if (new_vl.end() == std::find(new_vl.begin(), new_vl.end(), pl)) {
			// 기존 시야에 있었는데 새 시야에 없는 경우
			objects[p_id].m_vl.lock();
			objects[p_id].m_view_list.erase(pl);
			objects[p_id].m_vl.unlock();
			send_remove_object(p_id, pl);
			//cout << "시야에서 사라짐: " << pl << endl;

			objects[pl].m_vl.lock();
			if (0 != objects[pl].m_view_list.count(p_id)) {
				objects[pl].m_view_list.erase(p_id);
				objects[pl].m_vl.unlock();
				send_remove_object(pl, p_id);
			} else {
				objects[pl].m_vl.unlock();
			}
		}
	}
}

void process_packet(int p_id, unsigned char* p_buf) {
	switch (p_buf[1]) {
	case C2S_LOGIN: {
		c2s_login* packet = reinterpret_cast<c2s_login*>(p_buf);
		objects[p_id].m_state = PLST_INGAME;
		{
			lock_guard <mutex> gl1{ objects[p_id].m_slock };
			strcpy_s(objects[p_id].m_name, packet->name);
			//objects[p_id].x = 1;
			//objects[p_id].y = 1;
			objects[p_id].x = rand() % WORLD_X_SIZE;
			objects[p_id].y = rand() % WORLD_Y_SIZE;
			auto& sector = world_sector[objects[p_id].y / WORLD_SECTOR_SIZE][objects[p_id].x / WORLD_SECTOR_SIZE];
			lock_guard <mutex> gl2{ sector.m_sector_lock };
			sector.m_sector.insert(p_id);
		}
		send_login_ok_packet(p_id);

		auto&& selected_sector = get_overlapped_sector(p_id);

		objects[p_id].m_vl.lock();
		for (auto id : selected_sector) {
			objects[p_id].m_view_list.insert(id);
		}
		objects[p_id].m_vl.unlock();
		for (auto id : selected_sector) {
			send_add_object(id, p_id);
		}

		for(auto id : selected_sector){
			auto& pl = objects[id];
			pl.m_vl.lock();
			pl.m_view_list.insert(p_id);
			pl.m_vl.unlock();
			send_add_object(p_id, id);
		}


		break;
	}
	case C2S_MOVE: {
		c2s_move* packet = reinterpret_cast<c2s_move*>(p_buf);
		objects[p_id].move_time = packet->move_time;
		do_move(p_id, packet->dr);
		break;
	}
	default:
		cout << "Unknown Packet Type from Client[" << p_id;
		cout << "] Packet Type [" << +p_buf[1] << "]";
		while (true);
	}
}

void disconnect(int p_id) {
	if (objects[p_id].m_state == PLST_FREE) {
		return;
	}
	closesocket(objects[p_id].m_socket);
	objects[p_id].m_state = PLST_FREE;
	objects[p_id].sendExOverManager.clearSendData();

	int y = objects[p_id].y;
	int x = objects[p_id].x;
	auto sector_y = y / WORLD_SECTOR_SIZE;
	auto sector_x = x / WORLD_SECTOR_SIZE;
	for (int y=-1;y<=1;++y){
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

	objects[p_id].m_vl.lock();
	objects[p_id].m_old_view_list.resize(objects[p_id].m_view_list.size());
	std::copy(objects[p_id].m_view_list.begin(), objects[p_id].m_view_list.end(), objects[p_id].m_old_view_list.begin());
	objects[p_id].m_view_list.clear();
	objects[p_id].m_vl.unlock();
	auto& m_old_view_list = objects[p_id].m_old_view_list;
	for (auto pl : m_old_view_list) {
		if (PLST_INGAME == objects[pl].m_state)
			send_remove_object(objects[pl].id, p_id);
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
		}
					break;
		case OP_SEND: {
			auto usableGroup = reinterpret_cast<SendExOverManager::ExOverUsableGroup*>(ex_over);
			objects[key].sendExOverManager.recycle(usableGroup);
			break;
		}
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
		}
	}

}



int main() {
	for (int i = 0; i < MAX_USER + 1; ++i) {
		auto& pl = objects[i];
		pl.id = i;
		pl.m_state = PLST_FREE;
	}

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	wcout.imbue(locale("korean"));
	HANDLE h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
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

	vector <thread> worker_threads;
	for (int i = 0; i < 4; ++i)
		worker_threads.emplace_back(worker, h_iocp, listenSocket);
	for (auto& th : worker_threads)
		th.join();
	closesocket(listenSocket);
	WSACleanup();
}
