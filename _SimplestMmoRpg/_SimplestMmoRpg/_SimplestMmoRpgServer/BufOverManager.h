#pragma once
#include <mutex>
#include <vector>

#include "protocol.h"
class Player;
struct Session;
struct BufOver;
struct BufOverManager;

constexpr auto GLOBAL_RECYCLE_REMAIN_COUNT = 30;

/// <summary>
/// Create()으로 BufOver 쓰고
/// BufOver->Recycle() 호출하면 됨
/// </summary>
struct BufOverManager {
#define INITAL_MANAGED_EXOVER_COUNT 2
private:
	static std::array<std::list<BufOver*>, THREAD_COUNT> managedExOvers;
	static std::array<std::mutex, THREAD_COUNT> managedExOversLock;
	//StructPool<std::vector<unsigned char>> sendingDataQueue;
	std::vector<unsigned char> sendingData;
	std::mutex sendingDataLock;
	std::mutex remainBufLock;
	Session* session;
	Player* player;
	int remainBufSize;
	int id;
	int globalRecycleRemainCount = GLOBAL_RECYCLE_REMAIN_COUNT;
	std::chrono::system_clock::time_point lastSendTime;
	static size_t EX_OVER_SIZE_INCREMENT;

public:
	BufOverManager(Player* player);

	virtual ~BufOverManager();

	/// <summary>
	/// 보낼 데이터가 남아있으면 true 반환
	/// 내부에서 lock 사용함
	/// </summary>
	/// <returns></returns>
	bool HasSendData();

	/// <summary>
	/// 보낼 데이터를 큐에 쌓아둡니다.
	/// </summary>
	/// <param name="p"></param>
	void AddSendingData(void* p, int threadIdx);

	/// <summary>
	/// 저장해둔 데이터를 모두 초기화합니다.
	/// </summary>
	void ClearSendingData();

	/// <summary>
	/// 저장해둔 데이터를 send를 호출하여 id에 해당하는 플레이어에 보냅니다.
	/// </summary>
	void SendAddedData(int threadIdx);

	BufOver* Get(int threadIdx);

	/// <summary>
	/// 직접 호출하는 함수 아님 BufOver::Recycle사용할것
	/// </summary>
	/// <param name="usableGroup"></param>
	void Recycle(BufOver* usableGroup, int threadIdx);

private:
	std::vector<unsigned char>* GetSendingDataQueue();

	void RecycleSendingDataQueue(std::vector<unsigned char>* recycleQueue);

	void AddSendTimer(int threadIdx);

	void DebugProcessPacket(unsigned char* packet);
};