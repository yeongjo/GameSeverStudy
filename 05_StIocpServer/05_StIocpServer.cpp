#include <iostream>
#include <unordered_map>
using namespace std;
#include <WS2tcpip.h>
#include <MSWSock.h> // AcceptEX 사용시 필요
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")

#define MAX_BUFFER        1024
#define SERVER_PORT        3500
constexpr ULONG_PTR SERVER_ID = 0;
constexpr int MAX_USER = 10;

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

void CALLBACK recv_callback(DWORD Error, DWORD dataBytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags) {
	SOCKET client_s = reinterpret_cast<SESSION*>(overlapped)->socket; // 구조체 시작지점이 LPWSAOVERLAPPED이라 가능한 일. 꼼수

	if (dataBytes == 0) {
		closesocket(players[client_s].socket);
		players.erase(client_s);
		return;
	}  // 클라이언트가 closesocket을 했을 경우
	cout << "From client : " << players[client_s].messageBuffer << " (" << dataBytes << ") bytes)\n";
	players[client_s].dataBuffer.len = dataBytes;
	memset(&(players[client_s].overlapped), 0, sizeof(WSAOVERLAPPED)); // 재사용하기위해 0으로 초기화
	//players[client_s].overlapped.hEvent = (HANDLE)client_s; // 안쓰기로함
	WSASend(client_s, &(players[client_s].dataBuffer), 1, NULL, 0, overlapped, send_callback);
}

void CALLBACK send_callback(DWORD Error, DWORD dataBytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags) {
	DWORD flags = 0;

	SOCKET client_s = reinterpret_cast<SESSION*>(overlapped)->socket;

	if (dataBytes == 0) {
		closesocket(players[client_s].socket);
		players.erase(client_s);
		return;
	}  // 클라이언트가 closesocket을 했을 경우

	// WSASend(응답에 대한)의 콜백일 경우

	cout << "TRACE - Send message : " << players[client_s].messageBuffer << " (" << dataBytes << " bytes)\n";
	memset(&(players[client_s].overlapped), 0, sizeof(WSAOVERLAPPED));
	players[client_s].dataBuffer.len = MAX_BUFFER;
	//players[client_s].overlapped.hEvent = (HANDLE)client_s; // 안쓰기로함
	WSARecv(client_s, &players[client_s].dataBuffer, 1, 0, &flags, overlapped, recv_callback);
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
		if (i == players.size()) 
			return i;
	}
}


void processPacket(ULONG_PTR p_id, unsigned char* p_buf)
{
	switch(p_buf[1])
	{
	case C2S_LOGIN:
		break;
	case C2S_MOVE:
		break;
	default:
		cout << "Unknown Packet type from client[" << p_id << "] Packet type[" << (int)p_buf[1] << endl;
		while (true);
		break;
	}
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
	AcceptEx(listenSocket, c_socket, accept_over.m_packetbuf, 0, 16, 16, NULL, &accept_over.m_over);

	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);

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
				players[c_id] = SESSION{};
				players[c_id].id = c_id;
				players[c_id].m_name[0] = 0;
				players[c_id].m_recv_over.m_op = OP_RECV;
				players[c_id].m_recv_over.m_wasBuffer[0].len = MAX_BUFFER;
				players[c_id].m_recv_over.m_wasBuffer[0].buf = reinterpret_cast<char*>(&players[c_id].m_recv_over.m_packetbuf);
				players[c_id].m_socket = c_socket;
				players[c_id].m_prev_size = 0;
				do_recv(c_id);

				memset(&accept_over.m_over, 0, sizeof(accept_over.m_over));
				SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
				//AcceptEX
				AcceptEx(listenSocket, c_socket, accept_over.m_packetbuf, 0, 16, 16, NULL, &accept_over.m_over);
				break;
			}
		}


	}
	closesocket(listenSocket);
	WSACleanup();
}

