#pragma once
#include <mutex>
#include <vector>

#include "StrcutPool.h"
class Player;
struct Session;
struct BufOver;
struct BufOverManager;

/// <summary>
/// Create()���� BufOver ����
/// BufOver->Recycle() ȣ���ϸ� ��
/// </summary>
struct BufOverManager {
#define INITAL_MANAGED_EXOVER_COUNT 2
private:
	StructPool<BufOver> managedExOvers;
	StructPool<std::vector<unsigned char>> sendingDataQueue;
	std::vector<unsigned char> sendingData;
	std::mutex sendingDataLock;
	std::mutex remainBufLock;
	Session* session;
	Player* player;
	int remainBufSize;
	int id;
	
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
	void AddSendingData(void* p);

	/// <summary>
	/// �����ص� �����͸� ��� �ʱ�ȭ�մϴ�.
	/// </summary>
	void ClearSendingData();

	/// <summary>
	/// �����ص� �����͸� send�� ȣ���Ͽ� id�� �ش��ϴ� �÷��̾ �����ϴ�.
	/// </summary>
	void SendAddedData();

	BufOver* Get();

	/// <summary>
	/// ���� ȣ���ϴ� �Լ� �ƴ� BufOver::Recycle����Ұ�
	/// </summary>
	/// <param name="usableGroup"></param>
	void Recycle(BufOver* usableGroup);

private:
	std::vector<unsigned char>* GetSendingDataQueue();

	void RecycleSendingDataQueue(std::vector<unsigned char>* recycleQueue);

	void AddSendTimer();

	void DebugProcessPacket(unsigned char* packet);
};