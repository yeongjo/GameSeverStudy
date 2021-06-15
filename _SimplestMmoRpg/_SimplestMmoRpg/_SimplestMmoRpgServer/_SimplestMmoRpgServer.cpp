// =============== 최적화 ===============
// BufOverManager::Get(), BufOverManager::Recycle을 이용한 BufOver 재활용
// BufOverManager::AddSendingData()으로 패킷을 저장했다
// 별도의 스레드에서 BufOverManager::SendAddedData() 호출로 패킷 한꺼번에 보냄으로 WSASend호출 최소화
// CanSee와 Array를 활용한 섹터처리로 시야내 플레이어에게만 패킷보냄
// ===========================================


// =============== 몬스터 이동 ===============
// 시야에 들어왔을때 루아 스크립트에서 무슨일을 할지 결정한다.
// (1칸 남을때까지 다가간다, 플레이어 위치를 전달받으면서 멈출지 말지 bool로 스크립트에서 결정한다.) 장애물 회피여부에 따라 함수도 나눔, 혹은 wakeup상태일때 매 루프마다 루아의 함수 호출되게 해줘도 좋을듯. 근데 다른 플레이어의 정보는 다른곳에서 넘겨받아서 글로벌 변수로 가지고 잇어야하네
// 
// 플레이어에게 다가오면서 주변 플레이어들 위치와의 관계를 스크립트에서 검사하게 해준다.
// 배열로 넘기면 더 좋다
// ===========================================


// =================== 전투 ==================
// 전투 메시지 출력해야함 - sendChat로 전송해야하나 로그로 출력해야하나?
// 다른 NPC가 있는 곳으로 이동하면 때린다. sc_packet_stat_change패킷 전송
// ===========================================


// =================== 스킬 ==================
// 방향성 스킬, 버프스킬
// ===========================================


// =================== 맵 json 로드 ==================
// 클라 서버가 같은 맵 데이터를 가지고 있어야함
// 몬스터 배치 포함. 죽은지 일정시간 지나면 재생성 되어야함
//===========================================


// =============== 장애물 회피 이동 ===========
// c++에서 A* 사용하여 이동
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

constexpr int MONSTER_AGRO_RANGE = 3; // 몬스터가 어그로 끌리는 범위
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
				cout << "서버키가 Key로 넘어옴" << endl;
				exit(-1);
			}
			Player::Get(key)->Disconnect();
		}
		if ((key != SERVER_ID) && (0 == recvBufSize)) {
			Player::Get(key)->Disconnect();
			continue;
		}
		auto over = reinterpret_cast<MiniOver*>(reinterpret_cast<char*>(recvOver)-sizeof(void*)); // vtable 로 인해 포인터 바이트수 만큼 뺌

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
		cout << "DB 연결 실패로 다시 시도: "<< i+1<<"회\n";
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
			cout << "인원 수 꽉참" << endl;
		}
		CallAccept(accept_over, listenSocket);
	};
	CallAccept(accept_over, listenSocket);

	cout << "서버 열림" << endl;

	thread timer_thread(TimerQueueManager::Do);
	vector <thread> worker_threads;
	for (int i = 0; i < 4; ++i)
		worker_threads.emplace_back(Worker, hIocp);
	for (auto& th : worker_threads)
		th.join();
	closesocket(listenSocket);
	WSACleanup();
}
