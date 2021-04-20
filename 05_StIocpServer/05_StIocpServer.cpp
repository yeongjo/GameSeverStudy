#include <array>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
using namespace std;
#include <WS2tcpip.h>
#include <MSWSock.h>
#include "protocol.h"
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")




enum OP_TYPE { OP_RECV, OP_SEND, OP_ACCEPT };
struct EX_OVER {
	WSAOVERLAPPED   m_over;
	WSABUF         m_wsabuf[1];
	unsigned char   m_packetbuf[MAX_BUFFER];
	OP_TYPE         m_op;
	SOCKET m_csocket; // OP_ACCEPT 에서만 사용
};

enum PL_STATE{PLST_FREE, PLST_CONNECTED, PLST_INGAME};

struct SESSION {
	mutex m_slock;
	PL_STATE m_state;
	SOCKET   m_socket;
	int      id;

	EX_OVER m_recv_over;
	int m_prev_size;
	//Game Contents Data
	char   m_name[200];
	short   x, y;
};


constexpr int SERVER_ID = 0;

array <SESSION, MAX_USER+1> players;

void display_error(const char* msg, int err_no) {
	WCHAR* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	cout << msg;
	wcout << lpMsgBuf << endl;
	LocalFree(lpMsgBuf);
}

void send_packet(int p_id, void* p) {
	int p_size = reinterpret_cast<unsigned char*>(p)[0];
	int p_type = reinterpret_cast<unsigned char*>(p)[1];
	cout << "To client [ " << p_id << "] : ";//디버깅용 (나중에 삭제해야함)
	cout << "Packet [" << p_type << "]\n";

	EX_OVER* s_over = new EX_OVER; //로컬 변수로 절때 하지말것 send계속 사용할것이니
	s_over->m_op = OP_SEND;
	memset(&s_over->m_over, 0, sizeof(s_over->m_over));
	memcpy(s_over->m_packetbuf, p, p_size);
	s_over->m_wsabuf[0].buf = reinterpret_cast<CHAR*>(s_over->m_packetbuf);
	s_over->m_wsabuf[0].len = p_size;
	auto ret = WSASend(players[p_id].m_socket, s_over->m_wsabuf, 1, NULL, 0, &s_over->m_over, 0);
	if (0 != ret) {
		auto err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("Error in SendPacket: ", err_no);
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
	auto ret = WSARecv(players[s_id].m_socket, players[s_id].m_recv_over.m_wsabuf, 1, NULL, &r_flag, &players[s_id].m_recv_over.m_over, NULL);

	if (0 != ret) {
		auto err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("Error in SendPacket: ", err_no);
	}
}

int get_new_player_id(SOCKET socketSocket_d) {
	for (int i = SERVER_ID + 1; i < MAX_USER; ++i) {
		lock_guard<mutex> lg{ players[i].m_slock };
		if (PLST_FREE == players[i].m_state)
		{
			players[i].m_state = PLST_CONNECTED;
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
	send_packet(c_id, &p);
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
		lock_guard<mutex>(pl.m_slock);
		if(PLST_INGAME == pl.m_state)
		{
			send_move_packet(pl.id, p_id);
		}
	}
}

void proccess_packet(int p_id, unsigned char* p_buf) {

	switch (p_buf[1]) {
	case C2S_LOGIN: {
		c2s_login* packet = reinterpret_cast<c2s_login*>(p_buf);
		lock_guard<mutex> gl2{ players[p_id].m_slock };
		strcpy_s(players[p_id].m_name, packet->name);
		send_login_ok_packet(p_id);
		players[p_id].m_state = PLST_INGAME;

		for(auto& pl : players)
		{
			if(p_id != pl.id)
			{
				lock_guard<mutex> lg{ pl.m_slock };
				if(PLST_INGAME == pl.m_state)
				{
					send_add_player(pl.id, p_id);
					send_add_player(p_id, pl.id);
				}
			}
		}
	}

				  break;
	case C2S_MOVE: {
		c2s_move* packet = reinterpret_cast<c2s_move*>(p_buf);
		do_move(p_id, packet->dr);
	}

				 break;
	default:
		cout << "Unknown Packet Type from Client[" << p_id << "] Packet Type [" << p_buf[1] << "]" << endl;
		while (true);
	}
}



void disconnect(int p_id) {
	{
		lock_guard<mutex> gl(players[p_id].m_slock);
		closesocket(players[p_id].m_socket);
		players[p_id].m_state = PLST_FREE;
	}
	for (auto& pl : players) {
		lock_guard<mutex> gl2{ pl.m_slock };
		if(PLST_INGAME == pl.m_state)
		{
			send_remove_player(pl.id, p_id);
		}
	}
}

void worker(HANDLE h_iocp, SOCKET listenSocket) {

	while (true) {
		DWORD num_bytes;
		ULONG_PTR ikey;
		WSAOVERLAPPED* over;

		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &ikey, &over, INFINITE);


		int key = static_cast<int>(ikey);
		if (FALSE == ret) {
			if (SERVER_ID == key) {
				display_error("GQCS:", WSAGetLastError());
				exit(-1);
			} else {
				display_error("GQCS:", WSAGetLastError());
				disconnect(key);
			}

			if (0 == num_bytes) {
				disconnect(key);
				continue;
			}
		}


		EX_OVER* ex_over = reinterpret_cast<EX_OVER*> (over);

		switch (ex_over->m_op) {
		case OP_RECV: {
			// 패킷 조립 및 처리
			unsigned char* packet_ptr = ex_over->m_packetbuf;
			int num_data = num_bytes + players[key].m_prev_size;
			int packet_size = packet_ptr[0];

			while (num_data >= packet_size) {
				proccess_packet(key, packet_ptr);
				num_data -= packet_size;
				packet_ptr += packet_size;
				if (0 >= num_data) break;
				packet_size = packet_ptr[0];
			}
			players[key].m_prev_size = num_data;
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
				//players[c_id] = SESSION{};
				players[c_id].id = c_id;
				players[c_id].m_name[0] = 0;
				players[c_id].m_recv_over.m_op = OP_RECV;
				players[c_id].m_socket = ex_over->m_csocket;
				players[c_id].m_prev_size = 0;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(players[c_id].m_socket), h_iocp, c_id, 0);
				//주위에 누가 있는지 알려줘야함
				for (auto& pl : players) {
					if (c_id == pl.id) continue;
					send_add_player(c_id, pl.id);
					send_add_player(pl.id, c_id);
				}


				do_recv(c_id);
			} else {
				closesocket(ex_over->m_csocket);
			}

			//나중에 하나로 함수만드셈 2번쓰니까
			memset(&ex_over->m_over, 0, sizeof(ex_over->m_over));
			auto c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			ex_over->m_csocket = c_socket;
			AcceptEx(listenSocket, c_socket, ex_over->m_packetbuf, 0, 32, 32, NULL, &ex_over->m_over);
		}
					  break;
		}
	}
}

int main() {
	for (int i = 0; i < players.size(); ++i)
	{
		auto& pl = players[i];
		pl.id = i;
		pl.m_state = PLST_FREE;
	}

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	HANDLE h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);

	wcout.imbue(locale("korean"));

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
	AcceptEx(listenSocket, c_socket, accept_over.m_packetbuf, 0, 32, 32, NULL, &accept_over.m_over);

	vector<thread> worker_threads;
	for (int i = 0; i < 4; ++i) {
		worker_threads.emplace_back(worker, h_iocp, listenSocket);
	}

	for (auto& th : worker_threads) {
		th.join();
	}

	closesocket(listenSocket);
	WSACleanup();
}