#include <iostream>
#include <unordered_map>
#include <WS2tcpip.h>
#include <MSWSock.h> // AcceptEX 사용시 필요
#include "protocol.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")

using namespace std;

constexpr ULONG_PTR SERVER_ID = 0;

enum OP_TYPE {
	OP_RECV, OP_SEND, OP_ACCEPT
};

struct EX_OVER {
	WSAOVERLAPPED m_over; // 구조체 맨 앞에 있는 값의 주소가 구조체의 주소를 사용할때 사용된다.
	WSABUF m_wasBuffer[1];
	unsigned char m_packetbuf[MAX_BUFFER]; // 패킷사이즈가 -로 오면 패킷이 가지를 않는다.
	OP_TYPE m_op;
};

struct SESSION {
	SOCKET m_socket;
	int id;
	EX_OVER m_recv_over;
	DWORD m_prev_size;
	char m_name[200];
	short x, y;
};

unordered_map <int, SESSION> players;

void display_error(const char* msg, int err_no) {
	WCHAR* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	std::cout << msg;
	std::wcout << lpMsgBuf << std::endl;
	LocalFree(lpMsgBuf);
}

void send_packet(int p_id, void *p) {
	int p_size = static_cast<unsigned char*>(p)[0];
	int p_type = static_cast<unsigned char*>(p)[1];
	cout << "To client [" << p_id << "] : ";
	cout << "Pakcet [" << p_type << "]\n";
	EX_OVER* s_over = new EX_OVER;
	s_over->m_op = OP_SEND;
	memset(&s_over->m_over, 0, sizeof(s_over->m_over));
	memcpy(s_over->m_packetbuf, p, p_size);
	s_over->m_wasBuffer[0].buf = reinterpret_cast<char*>(&s_over->m_packetbuf);
	s_over->m_wasBuffer[0].len = p_size;
	WSASend(players[p_id].m_socket, s_over->m_wasBuffer, 1, NULL, 0, &s_over->m_over, 0);
}

void do_recv(int key) {
	players[key].m_recv_over.m_wasBuffer[0].buf = reinterpret_cast<char*>(&players[key].m_recv_over.m_packetbuf) + players[key].m_prev_size;
	players[key].m_recv_over.m_wasBuffer[0].len = MAX_BUFFER - players[key].m_prev_size;
	DWORD r_flag = 0;
	memset(&players[key].m_recv_over.m_over, 0, sizeof(players[key].m_recv_over.m_over));
	WSARecv(players[key].m_socket, players[key].m_recv_over.m_wasBuffer, 1, NULL, &r_flag, &players[key].m_recv_over.m_over, NULL);
}


int get_new_player_id() {
	for (int i = SERVER_ID + 1; i < MAX_USER; ++i) // 0은 서버의 id여서 쓰면 안된다.
	{
		if (i == players.size() + 1) 
			return i;
	}
	return -1;
}


void send_login_ok_packet(int p_id)
{
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

void send_move_packet(int p_id)
{
	s2c_move_player p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_MOVE_PLAYER;
	p.x = players[p_id].x;
	p.y = players[p_id].y;
	send_packet(p_id, &p);
}

void do_move(int p_id, DIRECTION direction)
{
	auto& x = players[p_id].x;
	auto& y = players[p_id].y;
	switch(direction)
	{
	case D_N:
		y > 0 ? y-- : y;
		break;
	case D_S:
		y < WORLD_Y_SIZE ? y++ : y;
		break;
	case D_E:
		x < WORLD_X_SIZE ? x++ : x;
		break;
	case D_W:
		x > 0 ? x-- : x;
		break;
	}
}

void processPacket(int p_id, unsigned char* p_buf)
{
	switch(p_buf[1])
	{
	case C2S_LOGIN:
		{
			c2s_login* packet = reinterpret_cast<c2s_login*>(p_buf);
			strcpy_s(players[p_id].m_name, packet->name);
			send_login_ok_packet(p_id);
			break;
		}
	case C2S_MOVE:
		{
			c2s_move* packet = (c2s_move*)p_buf;
			do_move(p_id, packet->direction);
			break;
		}
	default:
		{
			cout << "Unknown Packet type from client[" << p_id << "] Packet type[" << (int)p_buf[1] << endl;
			while (true);
			break;
		}
	}
}

void disconnect(int p_id) {
	closesocket(players[p_id].m_socket);
	players.erase(p_id);
}

int main() {
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
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
	//AcceptEX
	AcceptEx(listenSocket, c_socket, accept_over.m_packetbuf, 0, 32, 32, NULL, &accept_over.m_over);

	while (true) {
		DWORD num_bytes;
		ULONG_PTR ikey;
		WSAOVERLAPPED* over;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &ikey, &over, INFINITE);
		int key = static_cast<int>(ikey);

		if (FALSE == ret) {
			if(SERVER_ID == key)
			{
				display_error("Server GQCS: ", WSAGetLastError());
				exit(-1);
			}else
			{
				display_error("GQCS: ", WSAGetLastError());
				disconnect(key);
			}
		}

		EX_OVER* ex_over = reinterpret_cast<EX_OVER*>(over);

		switch (ex_over->m_op) {
		case OP_RECV:
			{
				//패킷 조립 및 처리
				unsigned char* packet_ptr= ex_over->m_packetbuf;
				int num_data = num_bytes + players[key].m_prev_size;
				int packet_size = packet_ptr[0];

				while (num_data >= packet_size) // 다 받았는지에 대한 처리
				{
					processPacket(key, packet_ptr);
					num_data -= packet_size;
					packet_ptr += packet_size;
					if (0 >= num_data)
						break;
					packet_size = packet_ptr[0];
				}
				players[key].m_prev_size = num_data;
				if(0 != num_data)
				{
					memcpy(ex_over->m_packetbuf, packet_ptr, num_data);
				}
				do_recv(key);
				break;
			}
		case OP_SEND:
			{
				delete ex_over;
				break;
			}
		case OP_ACCEPT:
			{
				int  c_id = get_new_player_id();
				if(c_id != -1)
				{
					players[c_id] = SESSION{};
					players[c_id].id = c_id;
					players[c_id].m_name[0] = 0;
					players[c_id].m_recv_over.m_op = OP_RECV;
					players[c_id].m_recv_over.m_wasBuffer[0].len = MAX_BUFFER;
					players[c_id].m_recv_over.m_wasBuffer[0].buf = reinterpret_cast<char*>(&players[c_id].m_recv_over.m_packetbuf);
					players[c_id].m_socket = c_socket;
					players[c_id].m_prev_size = 0;
					CreateIoCompletionPort(reinterpret_cast<HANDLE>(c_socket), h_iocp, c_id, 0);
					do_recv(c_id);

					memset(&accept_over.m_over, 0, sizeof(accept_over.m_over));
					SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
					//AcceptEX
					AcceptEx(listenSocket, c_socket, accept_over.m_packetbuf, 0, 32, 32, NULL, &accept_over.m_over);
				}else
				{
					closesocket(c_socket);
				}
				break;
			}
		}
	}
	closesocket(listenSocket);
	WSACleanup();
}

