#include <iostream>
#include <conio.h>
#include <WS2tcpip.h>
#pragma comment(lib, "WS2_32.lib")
using namespace std;

constexpr int SERVER_PORT = 3500;
const char* SERVER_IP = "127.0.0.1"; // 자기 자신의 주소는 항상 127.0.0.1
constexpr int RECV_BUF_SIZE = 2;
constexpr int SEND_BUF_SIZE = 1;
constexpr int CONSOLE_START_Y = 1;
#define MAPSIZE 8

char map[MAPSIZE][MAPSIZE];
char playerPosX = 4;
char playerPosY = 4;

void display_error(const char* msg, int err_no) {
	WCHAR* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	cout << msg;
	wcout << lpMsgBuf << endl;
	LocalFree(lpMsgBuf);
}

int clamp(int value, int min, int max)
{
	return value < min ? min : value > max ? max : value;
}

void gotoxy(int x, int y)
{
	COORD Pos;
	Pos.X = x;
	Pos.Y = y;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), Pos);

}

void drawMap() {
	gotoxy(0, CONSOLE_START_Y);
	for (size_t i = 0; i < MAPSIZE; i++)
	{
		for (size_t j = 0; j < MAPSIZE; j++)
		{
			map[i][j] = '+';
		}
	}

	map[playerPosY][playerPosX] = 'A';

	for (size_t i = 0; i < MAPSIZE; i++)
	{
		for (size_t j = 0; j < MAPSIZE; j++)
		{
			cout << map[i][j];
		}
		cout << endl;
	}
}

int main()
{
	wcout.imbue(locale("korean"));
	char serverIp[64];
	cout << "서버 주소를 입력해주세요: ";
	cin >> serverIp;
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);
	SOCKET server = WSASocket(AF_INET, SOCK_STREAM, 0, 0, 0, 0);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
	WSAConnect(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr), 0, 0, 0, 0);
	
	while(true)
	{
		drawMap();

		// send
		char mess[SEND_BUF_SIZE];
		mess[0] = static_cast<char>(_getch());
		WSABUF s_wsabuf[1]; // 보낼 버퍼
		s_wsabuf[0].buf = mess;
		s_wsabuf[0].len = SEND_BUF_SIZE;
		DWORD bytes_sent;
		WSASend(server, s_wsabuf, 1, &bytes_sent, 0, NULL, NULL);

		// recv
		char r_mess[RECV_BUF_SIZE];
		WSABUF r_wsabuf[1]; // 보낼 버퍼
		r_wsabuf[0].buf = r_mess;
		r_wsabuf[0].len = RECV_BUF_SIZE;
		DWORD bytes_recv;
		DWORD recv_flag = 0;
		int ret = WSARecv(server, r_wsabuf, 1, &bytes_recv, &recv_flag, NULL, NULL);
		if (SOCKET_ERROR == ret)
		{
			display_error("recv_error: ", WSAGetLastError());
			exit(-1);
		}
		playerPosX = r_mess[0];
		playerPosY = r_mess[1];
	}
}