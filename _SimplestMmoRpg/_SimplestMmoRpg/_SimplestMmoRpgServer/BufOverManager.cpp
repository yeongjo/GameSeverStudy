#include "BufOverManager.h"

#include "OverlappedStruct.h"
#include "Player.h"
#include "SocketUtil.h"
#include "TimerQueueManager.h"

size_t BufOverManager::EX_OVER_SIZE_INCREMENT = 2;

BufOverManager::BufOverManager(Player* player) : managedExOvers(INITAL_MANAGED_EXOVER_COUNT), session(player->GetSession()), player(player),
                                                id(player->GetId()) {
	auto size = INITAL_MANAGED_EXOVER_COUNT;
	for (size_t i = 0; i < size; ++i){
		managedExOvers[i] = new BufOver(this);
		managedExOvers[i]->InitOver();
	}
}

BufOverManager::~BufOverManager() {
	for (auto queue : sendingDataQueue){
		delete queue;
	}
	sendingDataQueue.clear();
}

bool BufOverManager::HasSendData() {
	return remainBufSize;
	//std::lock_guard<std::mutex> lock(sendingDataLock);
	//return !sendingData.empty();
}

void BufOverManager::AddSendingData(void* p) {
	const auto packetSize = static_cast<size_t>(static_cast<unsigned char*>(p)[0]);
	//std::cout << "AddSendingData: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
	std::lock_guard<std::mutex> lock(sendingDataLock);
	if (remainBufSize == 0) {
		AddSendTimer();
	}
	remainBufSize += packetSize;
	const auto prevSize = sendingData.size();
	const auto totalSendPacketSize = prevSize + packetSize;
	sendingData.resize(totalSendPacketSize);
	memcpy(&sendingData[prevSize], p, packetSize);
}

void BufOverManager::ClearSendingData() {
	std::lock_guard<std::mutex> lock(sendingDataLock);
	sendingData.clear();
	remainBufSize = 0;
}

BufOver* BufOverManager::Get() {
	BufOver* result;
	managedExOversLock.lock();
	size_t size = managedExOvers.size();
	if (size == 0) {
		managedExOvers.resize(size + EX_OVER_SIZE_INCREMENT);
		for (size_t i = size; i < size + EX_OVER_SIZE_INCREMENT; ++i) {
			managedExOvers[i] = new BufOver(this);
			managedExOvers[i]->InitOver();
		}
		managedExOversLock.unlock();
		auto newOver = new BufOver(this);
		newOver->InitOver();
		return newOver;
	}
	result = managedExOvers[size - 1];
	managedExOvers.pop_back(); // �˾��ϴ� ������� ����ȭ ����
	managedExOversLock.unlock();
	return result;
}

void BufOverManager::Recycle(BufOver* usableGroup) {
	usableGroup->InitOver();
	managedExOversLock.lock();
	managedExOvers.push_back(usableGroup);
	managedExOversLock.unlock();
}

std::vector<unsigned char>* BufOverManager::GetSendingDataQueue() {
	std::lock_guard<std::mutex> lock(sendingDataQueueLock); //TODO �յ� ����Ʈ ������� �ٲٱ�
	auto size = sendingDataQueue.size();
	if (0 < size) {
		const auto result = sendingDataQueue[size - 1];
		sendingDataQueue.pop_back();
		return result;
	}
	return new std::vector<unsigned char>();
}

void BufOverManager::RecycleSendingDataQueue(std::vector<unsigned char>* recycleQueue) {
	std::lock_guard<std::mutex> lock(sendingDataQueueLock);
	sendingDataQueue.push_back(recycleQueue);
}

void BufOverManager::AddSendTimer() {
	TimerQueueManager::Add(id, 32, [&]() {
		                       return HasSendData();
	                       }, [this](int size) {
		                       SendAddedData();
	                       });
}

void EmptyFunction(int size) {

}

void BufOverManager::SendAddedData() {
	sendingDataLock.lock();
	auto totalSendDataSize = sendingData.size();
	if (totalSendDataSize == 0) {
		sendingDataLock.unlock();
		return;
	}
	//std::cout << "Send!!!!: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;

	remainBufSize -= totalSendDataSize;
	if (remainBufSize > 0) {
		AddSendTimer();
	}

	auto& copiedSendingData = *GetSendingDataQueue();
	copiedSendingData.resize(totalSendDataSize);
	memcpy(&copiedSendingData[0], &sendingData[0], totalSendDataSize);
	sendingData.clear();
	sendingDataLock.unlock();

	auto sendDataBegin = &copiedSendingData[0];

	while (0 < totalSendDataSize) {
		const auto sendDataSize = static_cast<int>(totalSendDataSize);
		//auto sendDataSize = min(MAX_BUFFER, (int)totalSendDataSize);
		auto exOver = Get();
		exOver->callback = EmptyFunction;
		exOver->packetBuf.resize(sendDataSize);
		memcpy(&exOver->packetBuf[0], sendDataBegin, sendDataSize);
		exOver->wsabuf[0].buf = reinterpret_cast<CHAR*>(&exOver->packetBuf[0]);
		exOver->wsabuf[0].len = sendDataSize;
		int ret;
		{
			std::lock_guard <std::mutex> lock{ session->socketLock };
			ret = WSASend(session->socket, exOver->wsabuf, 1, NULL, 0, &exOver->over, NULL);// TODO GetQueuedCompletionStatus���� �� ���´��� Ȯ���ؼ� �Ȱ����� �� ������
			//std::cout << "totalSendDataSize["<<id<<"]: "<<totalSendDataSize << std::endl;
		}
		if (0 != ret) {
			auto err = WSAGetLastError();
			if (WSA_IO_PENDING != err) {
				PrintSocketError("WSASend : ", WSAGetLastError());
				player->Disconnect();
				return;
			}
		}
		totalSendDataSize -= sendDataSize;
		sendDataBegin += sendDataSize;
	}
	RecycleSendingDataQueue(&copiedSendingData);
}