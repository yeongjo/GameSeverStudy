// =============== ����ȭ ===============
// BufOverManager::Get(), BufOverManager::Recycle�� �̿��� BufOver ��Ȱ��
// BufOverManager::AddSendingData()���� ��Ŷ�� �����ߴ�
// ������ �����忡�� BufOverManager::SendAddedData() ȣ��� ��Ŷ �Ѳ����� �������� WSASendȣ�� �ּ�ȭ
// CanSee�� Array�� Ȱ���� ����ó���� �þ߳� �÷��̾�Ը� ��Ŷ����
// ===========================================


// =============== ���� �̵� ===============
// �þ߿� �������� ��� ��ũ��Ʈ���� �������� ���� �����Ѵ�.
// (1ĭ ���������� �ٰ�����, �÷��̾� ��ġ�� ���޹����鼭 ������ ���� bool�� ��ũ��Ʈ���� �����Ѵ�.) ��ֹ� ȸ�ǿ��ο� ���� �Լ��� ����, Ȥ�� wakeup�����϶� �� �������� ����� �Լ� ȣ��ǰ� ���൵ ������. �ٵ� �ٸ� �÷��̾��� ������ �ٸ������� �Ѱܹ޾Ƽ� �۷ι� ������ ������ �վ���ϳ�
// 
// �÷��̾�� �ٰ����鼭 �ֺ� �÷��̾�� ��ġ���� ���踦 ��ũ��Ʈ���� �˻��ϰ� ���ش�.
// �迭�� �ѱ�� �� ����
// ===========================================


// =================== ���� ==================
// ���� �޽��� ����ؾ��� - sendChat�� �����ؾ��ϳ� �α׷� ����ؾ��ϳ�?
// �ٸ� NPC�� �ִ� ������ �̵��ϸ� ������. sc_packet_stat_change��Ŷ ����
// ===========================================


// =================== ��ų ==================
// ���⼺ ��ų, ������ų
// ===========================================


// =================== �� json �ε� ==================
// Ŭ�� ������ ���� �� �����͸� ������ �־����
// ���� ��ġ ����. ������ �����ð� ������ ����� �Ǿ����
//===========================================


// =============== ��ֹ� ȸ�� �̵� ===========
// c++���� A* ����Ͽ� �̵�
// ===========================================


#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <vector>
#include <WS2tcpip.h>
#include <MSWSock.h>

//#define PLAYERLOG
//#define NPCLOG
//#define PLAYER_NOT_RANDOM_SPAWN
//#define DISPLAYLOG
//#define DBLOG
#include "Db.h"
#include "LuaUtil.h"
#include "Npc.h"
#include "PathFindHelper.h"
#include "Player.h"
#include "protocol.h"
#include "Session.h"
#include "SocketUtil.h"
#include "TimerQueueManager.h"
#include "WorldManager.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")

#include "Actor.cpp"
#include "BufOverManager.cpp"
#include "Image.cpp"
#include "LuaUtil.cpp"
#include "Monster.cpp"
#include "NonPlayer.cpp"
#include "Npc.cpp"
#include "OverlappedStruct.cpp"
#include "PathFindHelper.cpp"
#include "Player.cpp"
#include "Sector.cpp"
#include "TimerQueueManager.cpp"
#include "WorldManager.cpp"
#include "Db.cpp"

using namespace std;

constexpr int MONSTER_AGRO_RANGE = 3; // ���Ͱ� ��׷� ������ ����
constexpr int SERVER_ID = 0;

HANDLE hIocp;

std::string random_string(std::size_t length) {
	const std::string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	std::random_device random_device;
	std::mt19937 generator(random_device());
	std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

	std::string random_string;

	for (std::size_t i = 0; i < length; ++i) {
		random_string += CHARACTERS[distribution(generator)];
	}

	return random_string;
}

void Worker(HANDLE hIocp) {
	while (true) {
		DWORD recvBufSize;
		ULONG_PTR recvKey;
		WSAOVERLAPPED* recvOver;

		BOOL ret = GetQueuedCompletionStatus(hIocp, &recvBufSize,
			&recvKey, &recvOver, INFINITE);

		int key = static_cast<int>(recvKey);
		if (FALSE == ret) {
			PrintSocketError("GQCS : ", WSAGetLastError());
			if (SERVER_ID == key) {
				cout << "����Ű�� Key�� �Ѿ��" << endl;
				exit(-1);
			}
			Player::Get(key)->Disconnect();
		}
		if ((key != SERVER_ID) && (0 == recvBufSize)) {
			Player::Get(key)->Disconnect();
			continue;
		}
		auto over = reinterpret_cast<MiniOver*>(reinterpret_cast<char*>(recvOver)-sizeof(void*)); // vtable �� ���� ������ ����Ʈ�� ��ŭ ��

		over->callback(recvBufSize);
		over->Recycle();
	}
}

void CallAccept(AcceptOver& over, SOCKET listenSocket) {
	memset(&over.over, 0, sizeof(over.over));
	const SOCKET cSocket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	over.cSocket = cSocket;
	const auto ret = AcceptEx(listenSocket, cSocket,
	                          &over.wsabuf[0], 0, 32, 32, nullptr, &over.over);
	if (FALSE == ret) {
		const int err_num = WSAGetLastError();
		if (err_num != WSA_IO_PENDING)
			PrintSocketError("AcceptEx Error", err_num);
	}
}

int main() {
	wcout.imbue(locale("korean"));
	for (size_t i = 0; i < 5; i++) {
		if (DB::Create()) {
			break;
		}
		cout << "DB ���� ���з� �ٽ� �õ�: "<< i+1<<"ȸ\n";
	}
	
	WorldManager::Get()->Generate();
	WorldManager::Get()->Load();
	int i = SERVER_ID + 1;
	for (; i <= PLAYER_ID_END; ++i) {
		Player::Create(i);
	}
	for (; i <= NPC_ID_END; i++) {
		Npc::Create(i)->Init();
	}
	for (; i <= MONSTER_ID_END; i++) {
		WorldManager::Get()->GetMonster(i);
	}

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listenSocket), hIocp, SERVER_ID, 0);
	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	::bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
	listen(listenSocket, SOMAXCONN);

	TimerQueueManager::SetIocpHandle(hIocp);
	
	AcceptOver accept_over;

	accept_over.callback = [&](int) {
		int acceptId = Player::GetNewId(accept_over.cSocket);
		if (-1 != acceptId) {
			auto session = Player::Get(acceptId)->GetSession();
			CreateIoCompletionPort(
				reinterpret_cast<HANDLE>(session->socket), hIocp, acceptId, 0);
			Player::Get(acceptId)->CallRecv();
		} else {
			closesocket(accept_over.cSocket);
			cout << "�ο� �� ����" << endl;
		}
		CallAccept(accept_over, listenSocket);
	};
	CallAccept(accept_over, listenSocket);

	cout << "���� ����" << endl;

	thread timer_thread(TimerQueueManager::Do);
	vector <thread> worker_threads;
	for (int i = 0; i < 4; ++i)
		worker_threads.emplace_back(Worker, hIocp);
	for (auto& th : worker_threads)
		th.join();
	closesocket(listenSocket);
	WSACleanup();
}
