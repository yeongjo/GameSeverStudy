#include <iostream>
#include <WS2tcpip.h>
#pragma comment(lib, "WS2_32.lib")
using namespace std;
constexpr int SERVER_PORT = 3500;
const char* SERVER_IP = "127.0.0.1"; // 자기 자신의 주소는 항상 127.0.0.1
constexpr int BUF_SIZE = 1024;

int main() {
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);
	SOCKET server = WSASocket(AF_INET, SOCK_STREAM, 0, 0, 0, 0);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	//inet_pton(AF_INET, INADDR_ANY, &server_addr.sin_addr);// 아래줄이 대체함
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(server, reinterpret_cast<const sockaddr*>(&server_addr), sizeof(server_addr));
	listen(server, SOMAXCONN); // 1대 1통신에는 max는 과도하지만 그냥 씀
	
	//WSAConnect(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr), 0, 0, 0, 0);

	SOCKADDR_IN client_addr;
	int client_size = sizeof(client_addr);
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(SERVER_PORT);
	WSAAccept(server, reinterpret_cast<sockaddr*>(&client_addr), &client_size, 0, 0);
	while (true)
	{
		// recv
		char r_mess[BUF_SIZE];
		WSABUF r_wsabuf[1]; // 받을 버퍼
		r_wsabuf[0].buf = r_mess;
		r_wsabuf[0].len = BUF_SIZE;
		DWORD bytes_recved;
		DWORD recv_flag = 0;
		WSARecv(server, r_wsabuf, 1, &bytes_recved, &recv_flag, NULL, NULL);
		cout << "Client send: " << r_mess << endl;
		
		// send
		WSABUF s_wsabuf[1]; // 보낼 버퍼
		s_wsabuf[0].buf = r_mess;
		s_wsabuf[0].len = bytes_recved;
		DWORD bytes_sent; // 보내진 양
		WSASend(server, s_wsabuf, 1, &bytes_sent, 0, NULL, NULL);

		
	}
	closesocket(server);

}

//
//int main() {
//	WSADATA WSAData;
//	WSAStartup(MAKEWORD(2, 0), &WSAData);
//	SOCKET serverSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
//	SOCKADDR_IN serverAddr;
//	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
//	serverAddr.sin_family = AF_INET;
//	serverAddr.sin_port = htons(SERVER_PORT);
//	inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
//	connect(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
//	while (true) {
//		char messageBuffer[BUF_SIZE + 1];
//		cout << "Enter Message: ";
//		cin.getline(messageBuffer, BUF_SIZE);
//		int bufferLen = static_cast<int>(strlen(messageBuffer));
//		if (bufferLen == 0) break;
//		int sendBytes = send(serverSocket, messageBuffer, bufferLen + 1, 0);
//		cout << "Sent : " << messageBuffer << "(" << sendBytes << " bytes)\n";
//		int receiveBytes = recv(serverSocket, messageBuffer, BUF_SIZE, 0);
//		cout << "Received : " << messageBuffer << " (" << receiveBytes << " bytes)\n";
//	}
//	closesocket(serverSocket);
//	WSACleanup();
//}

//#include <iostream>
//#include <WS2tcpip.h>
//using namespace std;
//#pragma comment(lib, "Ws2_32.lib")
//#define MAX_BUFFER 1024
//#define SERVER_PORT 3500
//
//int main() {
//	WSADATA WSAData;
//	WSAStartup(MAKEWORD(2, 0), &WSAData);
//	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
//	SOCKADDR_IN serverAddr;
//	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
//	serverAddr.sin_family = AF_INET;
//	serverAddr.sin_port = htons(SERVER_PORT);
//	serverAddr.sin_addr.s_addr = INADDR_ANY;
//	::bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
//	listen(listenSocket, 5);
//	while (true) {
//		SOCKADDR_IN client_addr;
//		int addr_size = sizeof(client_addr);
//		SOCKET client_socket = accept(listenSocket, (sockaddr*)&client_addr, &addr_size);
//		while (true) {
//			char messageBuffer[MAX_BUFFER + 1];
//			int receiveBytes = recv(client_socket, messageBuffer, MAX_BUFFER, 0);
//			if (receiveBytes > 0) {
//				messageBuffer[receiveBytes] = 0;
//				cout << "TRACE - Receive message : " << messageBuffer << "(" << receiveBytes << " bytes)\n";
//			} else break;
//			int sendBytes = send(client_socket, messageBuffer, receiveBytes, 0);
//			if (sendBytes > 0) printf("TRACE - Send message : %s (%d bytes)\n", messageBuffer, sendBytes);
//		}
//		closesocket(client_socket);
//	}
//	closesocket(listenSocket);
//	WSACleanup();
//}