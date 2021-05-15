#include <array>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <queue>

#include "../2021_WF_STRESS/st_iocp_server/st_iocp_server/protocol.h"
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")
using namespace std;

enum OP_TYPE { OP_RECV, OP_SEND, OP_ACCEPT };
struct EX_OVER {
	WSAOVERLAPPED   m_over;
	WSABUF         m_wsabuf[1];
	unsigned char   m_packetbuf[MAX_BUFFER];
	OP_TYPE         m_op;
	SOCKET m_csocket; // OP_ACCEPT 에서만 사용
};

enum PL_STATE { PLST_FREE, PLST_CONNECTED, PLST_INGAME };

constexpr int SENDEXOVER_INCREASEMENT_SIZE = 64;

struct SendExOverManager {
	struct ExOverUsableGroup {
	private:
		EX_OVER ex_over;
		bool is_used = false;
		SendExOverManager* manager;

	public:
		void setIsUsed(bool used) {
			is_used = used;
		}

		bool getIsUsed() const {
			return is_used;
		}

		EX_OVER& getExOver() {
			return ex_over;
		}

		SendExOverManager* getManager() { return manager; }
		void setManager(SendExOverManager* manager) { this->manager = manager; }
	};

private:
	vector<ExOverUsableGroup> m_send_over;
	vector<unsigned char> m_send_data;
	mutex m_slock;

public:
	SendExOverManager() {
		//auto size = m_send_over.size();
		//m_send_over.resize(SENDEXOVER_INCREASEMENT_SIZE);
		//for (size_t i = 0; i < size;++i) {
		//	m_send_over[i].setManager(this);
		//}
		m_send_over.resize(1);
	}

	void addSendData(void* p) {
		lock_guard<mutex> lg{ m_slock };
		unsigned char p_size = reinterpret_cast<unsigned char*>(p)[0];

		auto prev_size = m_send_data.size();
		auto send_data_total_size = prev_size + p_size;
		m_send_data.resize(send_data_total_size);
		memcpy(static_cast<void*>(&m_send_data[prev_size]), p, p_size);
	}

	void clearSendData() {
		lock_guard<mutex> lg{ m_slock };
		m_send_data.clear();
	}

	void sendAddedData(int p_id);

	EX_OVER& get() {
		return m_send_over[0].getExOver();
		//size_t size = m_send_over.size();
		//if (size == 0) {
		//	m_send_over.resize(size + SENDEXOVER_INCREASEMENT_SIZE);
		//	for (size_t i = size; i < size+SENDEXOVER_INCREASEMENT_SIZE; ++i) {
		//		m_send_over[i].setManager(this);
		//	}
		//	size += SENDEXOVER_INCREASEMENT_SIZE;
		//}
		//auto& send_ex_over = m_send_over[size - 1];
		//m_send_over.pop_back();
		//return send_ex_over.getExOver();
	}

	void recycle(ExOverUsableGroup *usableGroup) {
		//usableGroup->setIsUsed(false);
		//m_send_over.push_back(*usableGroup);
	}
};

struct SESSION {
	EX_OVER m_recv_over;
	SendExOverManager sendExOverManager;
	PL_STATE m_state;
	SOCKET   m_socket;
	int      id;

	int m_prev_size;
	//Game Contents Data
	char   m_name[200];
	short   x, y;
	int move_time;
};

constexpr int SERVER_ID = 0;

array <SESSION, MAX_USER + 1> players;

void disconnect(int p_id);
void CALLBACK recv_callback(DWORD Error, DWORD dataBytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags);
void CALLBACK send_callback(DWORD Error, DWORD dataBytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags);

void display_error(const char* msg, int err_no) {
	WCHAR* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	cout << msg;
	wcout << lpMsgBuf << endl;
	LocalFree(lpMsgBuf);
}

void send_packet(int p_id, void* p) {
	unsigned char p_size = reinterpret_cast<unsigned char*>(p)[0];
	unsigned char p_type = reinterpret_cast<unsigned char*>(p)[1];
	//cout << "To client [ " << +p_id << "]  Packet [" << +p_type << "]\n";

	//EX_OVER* s_over = new EX_OVER;
	EX_OVER* s_over = &players[p_id].sendExOverManager.get(); //로컬 변수로 절때 하지말것 send계속 사용할것이니
	s_over->m_op = OP_SEND;
	memset(&s_over->m_over, 0, sizeof(s_over->m_over));
	memcpy(s_over->m_packetbuf, p, p_size);
	s_over->m_wsabuf[0].buf = reinterpret_cast<CHAR*>(s_over->m_packetbuf);
	s_over->m_wsabuf[0].len = p_size;
	auto ret = WSASend(players[p_id].m_socket, s_over->m_wsabuf, 1, NULL, 0, &s_over->m_over, send_callback);
	if (0 != ret) {
		auto err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) {
			display_error("WSASend : ", WSAGetLastError());
			disconnect(p_id);
		}
	}
}

void send_login_ok_packet(int p_id) {
	s2c_login_ok p;
	p.hp = 10;
	p.id = p_id;
	p.level = 2;
	p.race = 1;
	p.size = sizeof(p);
	p.type = S2C_LOGIN_OK;
	p.x = players[p_id].x;
	p.y = players[p_id].y;
	send_packet(p_id, &p);
}

void do_recv(int s_id) {
	players[s_id].m_recv_over.m_wsabuf[0].buf = reinterpret_cast<char*>(players[s_id].m_recv_over.m_packetbuf) + players[s_id].m_prev_size;
	players[s_id].m_recv_over.m_wsabuf[0].len = MAX_BUFFER - players[s_id].m_prev_size;
	memset(&players[s_id].m_recv_over.m_over, 0, sizeof(players[s_id].m_recv_over.m_over));
	DWORD r_flag = 0;
	auto ret = WSARecv(players[s_id].m_socket, players[s_id].m_recv_over.m_wsabuf, 1, NULL, &r_flag, &players[s_id].m_recv_over.m_over, recv_callback);

	if (0 != ret) {
		auto err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("Error in RecvPacket: ", err_no);
	}
}

int get_new_player_id(SOCKET p_socket) {
	for (int i = SERVER_ID + 1; i <= MAX_USER; ++i) {
		if (PLST_FREE == players[i].m_state) {
			players[i].m_state = PLST_CONNECTED;
			players[i].m_socket = p_socket;
			players[i].m_name[0] = 0;
			return i;
		}
	}
	return -1;
}

void send_add_player(int c_id, int p_id) {
	s2c_add_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_ADD_PLAYER;
	p.x = players[p_id].x;
	p.y = players[p_id].y;
	p.race = 0;
	send_packet(c_id, &p);
}

void send_remove_player(int c_id, int p_id) {
	s2c_remove_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_REMOVE_PLAYER;
	send_packet(c_id, &p);
}

void send_move_packet(int c_id, int p_id) {
	s2c_move_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_MOVE_PLAYER;
	p.x = players[p_id].x;
	p.y = players[p_id].y;
	p.move_time = players[p_id].move_time;
	players[c_id].sendExOverManager.addSendData(&p);
	//send_packet(c_id, &p);
}

void do_move(int p_id, DIRECTION dir) {
	auto& x = players[p_id].x;
	auto& y = players[p_id].y;
	switch (dir) {
	case D_N: if (y > 0)y--; break;
	case D_S: if (y < (WORLD_Y_SIZE - 1))y++; break;
	case D_W: if (x > 0)x--; break;
	case D_E: if (x < (WORLD_X_SIZE - 1))x++; break;
	}

	for (auto& pl : players) {
		if (PLST_INGAME == pl.m_state) {
			send_move_packet(pl.id, p_id);
		}
	}
}

void process_packet(int p_id, unsigned char* p_buf) {

	switch (p_buf[1]) {
	case C2S_LOGIN: {
		c2s_login* packet = reinterpret_cast<c2s_login*>(p_buf);
		strcpy_s(players[p_id].m_name, packet->name);
		players[p_id].m_state = PLST_INGAME;
		send_login_ok_packet(p_id);

		for (auto& pl : players) {
			if (p_id != pl.id) {
				if (PLST_INGAME == pl.m_state) {
					send_add_player(pl.id, p_id);
					send_add_player(p_id, pl.id);
				}
			}
		}
	}break;
	case C2S_MOVE: {
		c2s_move* packet = reinterpret_cast<c2s_move*>(p_buf);
		players[p_id].move_time = packet->move_time;
		do_move(p_id, packet->dr);
	}break;
	default:
		cout << "Unknown Packet Type from Client[" << p_id << "] Packet Type [" << +p_buf[1] << "]" << endl;
		while (true);
	}
}


void SendExOverManager::sendAddedData(int p_id) {
	m_slock.lock();
	while (!m_send_data.empty()) {
		auto send_data_begin = m_send_data.begin();
		auto p_size = min(MAX_BUFFER, (int)m_send_data.size());
		void* p = &*send_data_begin;
		auto s_over = &get();
		s_over->m_op = OP_SEND;
		memset(&s_over->m_over, 0, sizeof(s_over->m_over));
		memcpy(s_over->m_packetbuf, p, p_size);
		s_over->m_wsabuf[0].buf = reinterpret_cast<CHAR*>(s_over->m_packetbuf);
		s_over->m_wsabuf[0].len = p_size;
		auto ret = WSASend(players[p_id].m_socket, s_over->m_wsabuf, 1, NULL, 0, &s_over->m_over, NULL);
		if (0 != ret) {
			auto err_no = WSAGetLastError();
			if (WSA_IO_PENDING != err_no) {
				display_error("WSASend : ", WSAGetLastError());
				m_slock.unlock();
				disconnect(p_id);
				m_slock.lock();
			}
		} else{
			m_send_data.erase(send_data_begin, send_data_begin + p_size);
		}
	}
	m_slock.unlock();
}

void disconnect(int p_id) {
	//cout << "disconnect: " << p_id << endl;
	if (players[p_id].m_state == PLST_FREE) {
		return;
	}
	closesocket(players[p_id].m_socket);
	players[p_id].m_state = PLST_FREE;
	players[p_id].sendExOverManager.clearSendData();
	for (auto& pl : players) {
		if (PLST_INGAME == pl.m_state) {
			send_remove_player(pl.id, p_id);
		}
	}
}

void CALLBACK recv_callback(DWORD error, DWORD num_bytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags) {
	auto session = reinterpret_cast<SESSION*>(overlapped);
	SOCKET client_s = session->m_socket; // 구조체 시작지점이 LPWSAOVERLAPPED이라 가능한 일. 꼼수
	auto ex_over = &session->m_recv_over;

	// 패킷 조립 및 처리
	unsigned char* packet_ptr = ex_over->m_packetbuf;
	int num_data = num_bytes + session->m_prev_size;
	int packet_size = packet_ptr[0];

	while (num_data >= packet_size) {
		process_packet(session->id, packet_ptr);
		num_data -= packet_size;
		packet_ptr += packet_size;
		if (0 >= num_data) break;
		packet_size = packet_ptr[0];
	}
	session->m_prev_size = num_data;
	if (0 != num_data)
		memcpy(ex_over->m_packetbuf, packet_ptr, num_data);
	// 패킷 조립 및 처리 ==================

	do_recv(session->id);
}

void CALLBACK send_callback(DWORD Error, DWORD dataBytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags) {

	//auto usableGroup = reinterpret_cast<SendExOverManager::ExOverUsableGroup*>(overlapped);
	//usableGroup->getManager()->recycle(usableGroup);
	//
	//DWORD flags = 0;
	//auto session = reinterpret_cast<SESSION*>(overlapped);
	//SOCKET client_s = session->m_socket; // 구조체 시작지점이 LPWSAOVERLAPPED이라 가능한 일. 꼼수
	//auto ex_over = &session->m_recv_over;

	// WSASend(응답에 대한)의 콜백일 경우

	//cout << "TRACE - Send message : " << clients[client_s].messageBuffer << " (" << dataBytes << " bytes)\n";
	//memset(&(clients[client_s].overlapped), 0, sizeof(WSAOVERLAPPED));
	//clients[client_s].dataBuffer.len = MAX_BUFFER;
	////players[client_s].overlapped.hEvent = (HANDLE)client_s; // 안쓰기로함
	//WSARecv(client_s, &clients[client_s].dataBuffer, 1, 0, &flags, overlapped, recv_callback);
}

void sendWorker() {
	while(true){
		auto size = players.size();
		for (size_t i = 0; i < size; ++i){
			players[i].sendExOverManager.sendAddedData(players[i].id);
		}
		Sleep(50);
	}
}

int main() {
	for (int i = 0; i < players.size(); ++i) {
		auto& pl = players[i];
		pl.id = i;
		pl.m_state = PLST_FREE;
	}

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	wcout.imbue(locale("korean"));

	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	::bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
	listen(listenSocket, SOMAXCONN);

	SOCKADDR_IN clientAddr;
	int addrLen = sizeof(SOCKADDR_IN);
	memset(&clientAddr, 0, addrLen);

	thread sendWorkerThread(sendWorker);

	while (true) {
		DWORD num_bytes;
		ULONG_PTR ikey;
		WSAOVERLAPPED* over;

		SOCKET clientSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &addrLen);
		int c_id = get_new_player_id(clientSocket);
		if (-1 != c_id) {
			players[c_id].m_recv_over.m_op = OP_RECV;
			players[c_id].m_prev_size = 0;
			do_recv(c_id);
		} else {
			closesocket(clientSocket);
		}
	}
	sendWorkerThread.join();
	closesocket(listenSocket);
	WSACleanup();
}