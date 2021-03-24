#include <iostream>
#include <conio.h>
#include <map>
#include <mutex>
#include <thread>
#include <WS2tcpip.h>
#include <fcntl.h>
#pragma comment(lib, "WS2_32.lib")
using namespace std;

#pragma pack(1)

class PlayerInfo {
public:
	int id = 0;
	int posX = 0;
	int posY = 0;
};

class RecvPacket {
public:
	unsigned char playerCnt;
	PlayerInfo playerInfos[10];
};

constexpr int SERVER_PORT = 3500;
const char* SERVER_IP = "127.0.0.1"; // 자기 자신의 주소는 항상 127.0.0.1
constexpr int RECV_BUF_SIZE = sizeof(PlayerInfo);
constexpr int SEND_BUF_SIZE = 1;
constexpr int CONSOLE_START_Y = 1;
#define MAPSIZE 8

char chessBoard[MAPSIZE][MAPSIZE];
int myId;
SOCKET server;
int logLine = 9;
mutex value_mutex;
std::map<unsigned char, PlayerInfo> playerInfos;
u_long nonBlockingMode = 1;

void display_error(const char* msg, int err_no) {
	WCHAR* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, err_no, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPTSTR)&lpMsgBuf, 0, NULL);
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

void initChessBoard() {
	for (size_t i = 0; i < MAPSIZE; i++)
	{
		for (size_t j = 0; j < MAPSIZE; j++)
		{
			chessBoard[i][j] = '+';
		}
	}
}

void drawMap() {
	value_mutex.lock();
	gotoxy(0, CONSOLE_START_Y);
	for (size_t i = 0; i < MAPSIZE; i++)
	{
		for (size_t j = 0; j < MAPSIZE; j++)
		{
			cout << chessBoard[i][j];
		}
		cout << endl;
	}
	value_mutex.unlock();

}

void drawPlayers() {
	// 플레이어들 위치 그림
	auto lter = playerInfos.begin();
	while (lter != playerInfos.end()) {
		auto info = lter->second;
		chessBoard[info.posY][info.posX] = myId == lter->first ? 'M' : 'A';
		lter++;
	}
}

void recvUpdatePlayers() {
	// recv
	RecvPacket recvPacket;
	WSABUF r_wsabuf[1]; // 받을 버퍼
	r_wsabuf[0].buf = reinterpret_cast<char*>(&recvPacket.playerCnt);
	r_wsabuf[0].len = sizeof(RecvPacket::playerCnt);
	DWORD bytes_recv;
	DWORD recv_flag = 0;
	int ret = WSARecv(server, r_wsabuf, 1, &bytes_recv, &recv_flag, NULL, NULL);
	if (SOCKET_ERROR == ret)
	{
		//display_error("recv_error: ", WSAGetLastError());
		//exit(-1);
		drawPlayers();
		return;
	}

	PlayerInfo r_mess[10];
	r_wsabuf[0].buf = (char*)r_mess;
	r_wsabuf[0].len = recvPacket.playerCnt * sizeof(PlayerInfo);
	ret = WSARecv(server, r_wsabuf, 1, &bytes_recv, &recv_flag, NULL, NULL);
	if (SOCKET_ERROR == ret)
	{
		//display_error("recv_error: ", WSAGetLastError());
		//exit(-1);
		drawPlayers();
		return;
	}
	
	value_mutex.lock();
	//gotoxy(0, logLine++);
	//cout << "받은 바이트 크기(" << bytes_recv << ") 플레이어수("<< (unsigned)recvPacket.playerCnt<<")\n";
	value_mutex.unlock();

	for (int i = 0; i < recvPacket.playerCnt; i++){
		playerInfos[r_mess[i].id].id = r_mess[i].id;
		playerInfos[r_mess[i].id].posX = r_mess[i].posX;
		playerInfos[r_mess[i].id].posY = r_mess[i].posY;
		//cout << "받은 데이터 id(" << (unsigned)r_mess[i].id << ") posX("<< (unsigned)r_mess[i].posX << ") posY("<< (unsigned)r_mess[i].posY<<")\n";
	}

	drawPlayers();
}



unsigned int recvUpdatePlayersLoop(void* arg) {
	while(true){
		recvUpdatePlayers();
	}
	return 0;
}

int key_value;

void sendKeyInput() {
	// send
	if (!_kbhit()){
		return;
	}
	key_value = _getch();  // 키를 하나 입력 받는다.
	if (key_value == 0x00 || key_value == 0xE0) {
		// 확장키의 경우 키를 하나더 입력 받는다.
		key_value = _getch();
		cout << key_value << endl;
	}
	char mess[SEND_BUF_SIZE];
	mess[0] = static_cast<char>(key_value);
	key_value = -1;
	WSABUF s_wsabuf[1]; // 보낼 버퍼
	s_wsabuf[0].buf = mess;
	s_wsabuf[0].len = SEND_BUF_SIZE;
	DWORD bytes_sent;
	int ret = WSASend(server, s_wsabuf, 1, &bytes_sent, 0, NULL, NULL);
	if (ret == SOCKET_ERROR) {
		//display_error("Send error: ", WSAGetLastError());
		return;
	}
	value_mutex.lock();
	gotoxy(0, logLine++);
	//cout << "보낸 데이터 내용(" << (int)mess[0] << ") 바이트크기(" << bytes_sent <<")\n";
	value_mutex.unlock();
}

int main()
{
	wcout.imbue(locale("korean"));
	char serverIp[64];
	cout << "서버 주소를 입력해주세요(아무 한글자나 입력하면 본인컴): ";
	cin >> serverIp;
	if(strlen(serverIp) < 2){
		strcpy_s(serverIp, "127.0.0.1");
	}
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);
	server = WSASocket(AF_INET, SOCK_STREAM, 0, 0, 0, 0);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
	WSAConnect(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr), 0, 0, 0, 0);

	//auto flag = fcntl(sock_fd, F_GETFL, 0);

	// recv my id
	WSABUF r_wsabuf[1]; // 보낼 버퍼
	r_wsabuf[0].buf = reinterpret_cast<char*>(&myId);
	r_wsabuf[0].len = sizeof(myId);
	DWORD bytes_recv;
	DWORD recv_flag = 0;
	int ret = WSARecv(server, r_wsabuf, 1, &bytes_recv, &recv_flag, NULL, NULL);
	if (SOCKET_ERROR == ret)
	{
		display_error("201	recv_error: ", WSAGetLastError());
		exit(-1);
	}
	gotoxy(0, logLine++);
	cout << "받은 ID(" << (unsigned)myId << ")  받은바이트크기(" << bytes_recv << ")\n";
	

	initChessBoard();
	drawMap();

	ioctlsocket(server, FIONBIO, &nonBlockingMode);
	//unsigned int threadId;
	//thread sendKeyInputThread(sendKeyInputLoop);
	//_beginthreadex(NULL, 0, recvUpdatePlayersLoop, NULL, 0, &threadId);
	while(true)
	{
		recvUpdatePlayers();
		
		drawMap();

		Sleep(60);

		sendKeyInput();
		
		initChessBoard();
	}

	//sendKeyInputThread.detach();
}
