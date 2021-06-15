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
/// Create()���� BufOver ����
/// BufOver->Recycle() ȣ���ϸ� ��
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
	/// ���� �����Ͱ� ���������� true ��ȯ
	/// ���ο��� lock �����
	/// </summary>
	/// <returns></returns>
	bool HasSendData();

	/// <summary>
	/// ���� �����͸� ť�� �׾ƵӴϴ�.
	/// </summary>
	/// <param name="p"></param>
	void AddSendingData(void* p, int threadIdx);

	/// <summary>
	/// �����ص� �����͸� ��� �ʱ�ȭ�մϴ�.
	/// </summary>
	void ClearSendingData();

	/// <summary>
	/// �����ص� �����͸� send�� ȣ���Ͽ� id�� �ش��ϴ� �÷��̾ �����ϴ�.
	/// </summary>
	void SendAddedData(int threadIdx);

	BufOver* Get(int threadIdx);

	/// <summary>
	/// ���� ȣ���ϴ� �Լ� �ƴ� BufOver::Recycle����Ұ�
	/// </summary>
	/// <param name="usableGroup"></param>
	void Recycle(BufOver* usableGroup, int threadIdx);

private:
	std::vector<unsigned char>* GetSendingDataQueue();

	void RecycleSendingDataQueue(std::vector<unsigned char>* recycleQueue);

	void AddSendTimer(int threadIdx);

	void DebugProcessPacket(unsigned char* packet);
};