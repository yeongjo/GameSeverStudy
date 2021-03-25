#include <iostream>
#include <map>
#include <vector>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using namespace std;
#pragma pack(1)

constexpr int MAX_BUFFER = 1024;
constexpr int SERVER_PORT = 3500;
constexpr int MAPSIZE = 8;
constexpr int MAX_PLAYER_CNT = 10;
constexpr int KEY_UP = 72;
constexpr int KEY_DOWN = 80;
constexpr int KEY_LEFT = 75;
constexpr int KEY_RIGHT = 77;
constexpr int CONSOLE_START_Y = 1;
constexpr int RECV_BUF_SIZE = 1;

class PlayerInfo {
public:
	int id = 0;
	int posX = 0;
	int posY = 0;
};

class SendPacket {
public:
	unsigned char playerCnt;
	PlayerInfo playerInfos[MAX_PLAYER_CNT];
};

constexpr int SEND_BUF_SIZE = sizeof(PlayerInfo);


struct SOCKETINFO {
	WSAOVERLAPPED recvOverlapped; // 구조체 맨 앞에 있는 값의 주소가 구조체의 주소를 사용할때 사용된다.
	WSAOVERLAPPED sendOverlapped[2]; // 0은 id보내는거에 사용 1은 플레이어 정보 보내는데 사용 나중에 풀링해서 사용하는걸 만들거나 해야할듯
	WSABUF sendDataBuffer;
	WSABUF recvDataBuffer;
	SOCKET socket;
	char messageBuffer[MAX_BUFFER];
	PlayerInfo player;
};

map<SOCKET, SOCKETINFO> clients;
map<LPWSAOVERLAPPED, SOCKETINFO*> clientsFromRecv;
map<LPWSAOVERLAPPED, SOCKETINFO*> clientsFromSend0;
map<LPWSAOVERLAPPED, SOCKETINFO*> clientsFromSend1;
int currentPlayerId = 0;
char chessBoard[MAPSIZE][MAPSIZE];

void CALLBACK recv_callback(DWORD Error, DWORD dataBytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags);
void CALLBACK sendPlayerInfosCallback(DWORD Error, DWORD dataBytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags);

void display_error(const char* msg, int err_no) {
	WCHAR* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, err_no, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPTSTR)&lpMsgBuf, 0, NULL);
	cout << msg;
	wcout << lpMsgBuf << endl;
	LocalFree(lpMsgBuf);
}

int clamp(int value, int min, int max) {
	return value < min ? min : (value > max ? max : value);
}

void gotoxy(int x, int y) {
	COORD Pos;
	Pos.X = x;
	Pos.Y = y;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), Pos);
}

bool getKeyInput(PlayerInfo& playerInfo, int keyCode) {
	switch (keyCode) {
	case KEY_UP: // 위
		playerInfo.posY--;
		break;
	case KEY_DOWN: // 아래
		playerInfo.posY++;
		break;
	case KEY_LEFT: // 왼
		playerInfo.posX--;
		break;
	case KEY_RIGHT: // 오른
		playerInfo.posX++;
		break;
	default:
		return false;
	}
	playerInfo.posX = clamp(playerInfo.posX, 0, MAPSIZE - 1);
	playerInfo.posY = clamp(playerInfo.posY, 0, MAPSIZE - 1);
	return true;
}

void sendEveryPlayersInfo(SOCKETINFO* socketInfo) {
	const auto clientCnt = clients.size();
	if (clientCnt == 0) {
		return;
	}

	auto lter0 = clients.begin();
	for (int i = 0; lter0 != clients.end(); ++lter0, ++i) {
		reinterpret_cast<SendPacket*>(socketInfo->messageBuffer)->playerInfos[i] = lter0->second.player;
	}

	auto lter1 = clients.begin();
	while (lter1 != clients.end()) {
		auto& info = clients[lter1->first];
		clientsFromSend1[&info.sendOverlapped[1]] = &info;

		memcpy(reinterpret_cast<SendPacket*>(info.messageBuffer)->playerInfos, reinterpret_cast<SendPacket*>(socketInfo->messageBuffer)->playerInfos, clientCnt * sizeof(PlayerInfo));
		reinterpret_cast<SendPacket*>(info.messageBuffer)->playerCnt = (unsigned char)clientCnt;

		info.sendDataBuffer.buf = info.messageBuffer;
		info.sendDataBuffer.len = (SEND_BUF_SIZE * clientCnt) + sizeof(SendPacket::playerCnt);
		memset(&(info.sendOverlapped[1]), 0, sizeof(WSAOVERLAPPED)); // 재사용하기위해 0으로 초기화
		WSASend(info.socket, &(info.sendDataBuffer), 1, NULL, 0, &info.sendOverlapped[1], socketInfo == &info ? sendPlayerInfosCallback : 0);
		++lter1;
	}
}

void recvPlayerKeyInput(SOCKETINFO& socketInfo) {
	DWORD flags = 0;
	clientsFromRecv[&socketInfo.recvOverlapped] = &socketInfo;
	memset(&(socketInfo.recvOverlapped), 0, sizeof(WSAOVERLAPPED));
	socketInfo.recvDataBuffer.buf = socketInfo.messageBuffer;
	socketInfo.recvDataBuffer.len = 1;
	auto ret = WSARecv(socketInfo.socket, &socketInfo.recvDataBuffer, 1, 0, &flags, &socketInfo.recvOverlapped, recv_callback);
	if (ret == SOCKET_ERROR) {
		display_error("Send error: ", WSAGetLastError());
	}
}

void CALLBACK recv_callback(DWORD Error, DWORD dataBytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags) {
	auto& socket = clientsFromRecv[overlapped]->socket;
	auto& client = clients[socket];

	if (dataBytes == 0) {
		cout << "recv: dataBytes == 0 Remove socket from list" << endl;
		closesocket(client.socket);
		clients.erase(socket);
		return;
	}  // 클라이언트가 closesocket을 했을 경우
	cout << "From client : " << (int)client.messageBuffer[0] << " (" << dataBytes << ") bytes)\n";

	getKeyInput(client.player, (int)client.messageBuffer[0]);
	sendEveryPlayersInfo(&client);
}

void CALLBACK sendPlayerInfosCallback(DWORD Error, DWORD dataBytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags) {
	auto& socket = clientsFromSend1[overlapped]->socket;
	auto& client = clients[socket];

	if (dataBytes == 0) {
		cout << "send: dataBytes == 0 Remove socket from list" << endl;
		closesocket(client.socket);
		clients.erase(socket);
		return;
	}  // 클라이언트가 closesocket을 했을 경우

	// WSASend(응답에 대한)의 콜백일 경우

	PlayerInfo* sendedInfo = (PlayerInfo*)client.sendDataBuffer.buf;
	cout << "TRACE - Send message : " << (unsigned)sendedInfo->id << ": " << (unsigned)sendedInfo->posX << ": " << (unsigned)sendedInfo->posY << ": " << " (" << dataBytes << " bytes)\n";
	recvPlayerKeyInput(client);
}

void CALLBACK sendPlayerIdCallback(DWORD Error, DWORD dataBytes, LPWSAOVERLAPPED overlapped, DWORD lnFlags) {
	DWORD flags = 0;
	auto& socket = clientsFromSend0[overlapped]->socket;
	auto& client = clients[socket];

	if (dataBytes == 0) {
		cout << "send: dataBytes == 0 Remove socket from list" << endl;
		closesocket(client.socket);
		clients.erase(socket);
		return;
	}  // 클라이언트가 closesocket을 했을 경우

	auto id = *(unsigned short*)client.sendDataBuffer.buf;
	cout << "TRACE - Send message : " << id << ": " << " (" << dataBytes << " bytes)\n";

	sendEveryPlayersInfo(&client);
}

void drawMap() {
	gotoxy(0, CONSOLE_START_Y);
	for (size_t i = 0; i < MAPSIZE; i++) {
		for (size_t j = 0; j < MAPSIZE; j++) {
			chessBoard[i][j] = '+';
		}
	}

	auto iter = clients.begin();
	while (iter != clients.end()) {
		chessBoard[iter->second.player.posX][iter->second.player.posY] = 'A';
	}

	for (size_t i = 0; i < MAPSIZE; i++) {
		for (size_t j = 0; j < MAPSIZE; j++) {
			cout << chessBoard[i][j];
		}
		cout << endl;
	}
}

void sendPlayerId(SOCKETINFO& client) {
	client.sendDataBuffer.len = sizeof(client.player.id);
	client.sendDataBuffer.buf = reinterpret_cast<char*>(&client.player.id);
	memset(&client.sendOverlapped[0], 0, sizeof(WSAOVERLAPPED));
	DWORD flags = 0;

	clientsFromSend0[&client.sendOverlapped[0]] = &client;
	auto result = WSASend(client.socket, &client.sendDataBuffer, 1, NULL, 0, &client.sendOverlapped[0], sendPlayerIdCallback);

	if (result == SOCKET_ERROR) {
		cout << "ERROR!: " << client.player.id << endl;
		display_error("Send error: ", WSAGetLastError());
	}
}

int main() {
	wcout.imbue(locale("english"));
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	::bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
	listen(listenSocket, 5);
	SOCKADDR_IN clientAddr;
	int addrLen = sizeof(SOCKADDR_IN);
	memset(&clientAddr, 0, addrLen);

	while (true) {
		SOCKET clientSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &addrLen);
		clients[clientSocket] = SOCKETINFO{};
		clients[clientSocket].socket = clientSocket;
		clients[clientSocket].player.id = currentPlayerId++;
		sendPlayerId(clients[clientSocket]);
	}
	closesocket(listenSocket);
	WSACleanup();
}

