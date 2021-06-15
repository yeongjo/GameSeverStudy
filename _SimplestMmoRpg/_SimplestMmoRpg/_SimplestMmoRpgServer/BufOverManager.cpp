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
	std::lock_guard<std::mutex> lock(sendingDataLock);
	return !sendingData.empty();
}

void BufOverManager::AddSendingData(void* p) {
	TimerQueueManager::Add(id, 12, [&]() {
		return HasSendData();
		}, [this](int size) {
			SendAddedData();
		});
	const auto packetSize = static_cast<size_t>(static_cast<unsigned char*>(p)[0]);
	std::lock_guard<std::mutex> lock(sendingDataLock);
	const auto prevSize = sendingData.size();
	const auto totalSendPacketSize = prevSize + packetSize;
	sendingData.resize(totalSendPacketSize);
	memcpy(&sendingData[prevSize], p, packetSize);
}

void BufOverManager::ClearSendingData() {
	std::lock_guard<std::mutex> lock(sendingDataLock);
	sendingData.clear();
}

BufOver* BufOverManager::Get() {
	BufOver* result;
	managedExOversLock.lock();
	size_t size = managedExOvers.size();
	if (size == 0){
		managedExOvers.resize(size + EX_OVER_SIZE_INCREMENT);
		for (size_t i = size; i < size + EX_OVER_SIZE_INCREMENT; ++i){
			managedExOvers[i] = new BufOver(this);
			managedExOvers[i]->InitOver();
		}
		managedExOversLock.unlock();
		auto newOver = new BufOver(this);
		newOver->InitOver();
		return newOver;
	}
	result = managedExOvers[size - 1];
	managedExOvers.pop_back(); // 팝안하는 방식으로 최적화 가능
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
	std::lock_guard<std::mutex> lock(sendingDataQueueLock);
	auto size = sendingDataQueue.size();
	if (0 < size){
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

void BufOverManager::SendAddedData() {
	sendingDataLock.lock();
	auto totalSendDataSize = sendingData.size();
	if (totalSendDataSize == 0) {
		sendingDataLock.unlock();
		return;
	}

	auto& copiedSendingData = *GetSendingDataQueue();
	copiedSendingData.resize(totalSendDataSize);
	memcpy(&copiedSendingData[0], &sendingData[0], totalSendDataSize);
	sendingData.clear();
	sendingDataLock.unlock();
	
	auto sendDataBegin = &copiedSendingData[0];

	while (0 < totalSendDataSize) {
		auto sendDataSize = min(MAX_BUFFER, (int)totalSendDataSize);
		auto exOver = Get();
		exOver->callback = [](int size) {};
		memcpy(exOver->packetBuf, sendDataBegin, sendDataSize);
		exOver->wsabuf[0].buf = reinterpret_cast<CHAR*>(exOver->packetBuf);
		exOver->wsabuf[0].len = sendDataSize;
		int ret;
		{
			std::lock_guard <std::mutex> lock{ session->socketLock };
			ret = WSASend(session->socket, exOver->wsabuf, 1, NULL, 0, &exOver->over, NULL);// TODO GetQueuedCompletionStatus에서 얼마 보냈는지 확인해서 안갔으면 더 보내기
		}
		if (0 != ret) {
			auto err = WSAGetLastError();
			if (WSA_IO_PENDING != err) {
				display_error("WSASend : ", WSAGetLastError());
				player->Disconnect();
				return;
			}
		}
		totalSendDataSize -= sendDataSize;
		sendDataBegin += sendDataSize;
	}
	RecycleSendingDataQueue(&copiedSendingData);
}