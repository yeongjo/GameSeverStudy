#include <iostream>
#include <WS2tcpip.h>
#pragma comment(lib, "WS2_32.lib")
using namespace std;
constexpr int SERVER_PORT = 3500;
const char* SERVER_IP = "127.0.0.1"; // 자기 자신의 주소는 항상 127.0.0.1
constexpr int BUF_SIZE = 1024;

void display_error(const char* msg, int err_no)
{
    WCHAR* lpMsgBuf;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
    cout << msg;
    wcout << lpMsgBuf << endl;
    LocalFree(lpMsgBuf);
}

int main() {
    wcout.imbue(locale("korean"));
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 0), &WSAData);
    SOCKET server = WSASocket(AF_INET, SOCK_STREAM, 0, 0, 0, 0);
    SOCKADDR_IN server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    WSAConnect(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr), 0, 0, 0, 0);
    while (true)
    {

        char mess[BUF_SIZE];
        cout << "Enter Message : ";
        cin.getline(mess, BUF_SIZE - 1);

        // send
        WSABUF s_wsabuf[1]; // 보낼 버퍼
        s_wsabuf[0].buf = mess;
        s_wsabuf[0].len = static_cast<unsigned int>(strlen(mess) + 1);
        DWORD bytes_sent;
        WSASend(server, s_wsabuf, 1, &bytes_sent, 0, NULL, NULL);

        // recv
        char r_mess[BUF_SIZE];
        WSABUF r_wsabuf[1]; // 보낼 버퍼
        r_wsabuf[0].buf = r_mess;
        r_wsabuf[0].len = BUF_SIZE;
        DWORD bytes_recv;
        DWORD recv_flag = 0;
        int ret = WSARecv(server, r_wsabuf, 1, &bytes_recv, &recv_flag, NULL, NULL);
    	if(SOCKET_ERROR == ret)
    	{
            display_error("recv_error: ", WSAGetLastError());
            exit(-1);
    	}

        cout << "Server sent:" << r_mess << endl;
    }
    closesocket(server);
    WSACleanup();
}

//
//int main() {
//   WSADATA WSAData;
//   WSAStartup(MAKEWORD(2, 0), &WSAData);
//   SOCKET serverSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
//   SOCKADDR_IN serverAddr;
//   memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
//   serverAddr.sin_family = AF_INET;
//   serverAddr.sin_port = htons(SERVER_PORT);
//   inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
//   connect(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
//   while (true) {
//      char messageBuffer[BUF_SIZE + 1];
//      cout << "Enter Message: ";
//      cin.getline(messageBuffer, BUF_SIZE);
//      int bufferLen = static_cast<int>(strlen(messageBuffer));
//      if (bufferLen == 0) break;
//      int sendBytes = send(serverSocket, messageBuffer, bufferLen + 1, 0);
//      cout << "Sent : " << messageBuffer << "(" << sendBytes << " bytes)\n";
//      int receiveBytes = recv(serverSocket, messageBuffer, BUF_SIZE, 0);
//      cout << "Received : " << messageBuffer << " (" << receiveBytes << " bytes)\n";
//   }
//   closesocket(serverSocket);
//   WSACleanup();
//}