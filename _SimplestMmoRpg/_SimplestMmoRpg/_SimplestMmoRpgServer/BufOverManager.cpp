#include "BufOverManager.h"

#include "OverlappedStruct.h"
#include "Player.h"
#include "SocketUtil.h"
#include "TimerQueueManager.h"

size_t BufOverManager::EX_OVER_SIZE_INCREMENT = 2;
std::array<std::list<BufOver*>, THREAD_COUNT> BufOverManager::managedExOvers;
std::array<std::mutex, THREAD_COUNT> BufOverManager::managedExOversLock;

BufOverManager::BufOverManager(Player* player) : session(player->GetSession()),
                                                 player(player),
                                                 id(player->GetId()) {
}

BufOverManager::~BufOverManager() {
}

bool BufOverManager::HasSendData() {
	std::lock_guard<std::mutex> lock(sendingDataLock);
	return !sendingData.empty();
}

void BufOverManager::AddSendingData(void* p, int threadIdx) {
	const auto packetSize = static_cast<size_t>(static_cast<unsigned char*>(p)[0]);
	//std::cout << "AddSendingData: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;

	sendingDataLock.lock();
	const auto prevSize = sendingData.size();
	const auto totalSendPacketSize = prevSize + packetSize;
	sendingData.resize(totalSendPacketSize);
	memcpy(&sendingData[prevSize], p, packetSize);
	if (prevSize == 0) {
		sendingDataLock.unlock();
		AddSendTimer(threadIdx);
		return;
	}
	sendingDataLock.unlock();
	if(lastSendTime + std::chrono::milliseconds(25) < std::chrono::system_clock::now()){
		//TimerQueueManager::Post(id, threadIdx, [this](int size, int threadIdx2) {
		//	SendAddedData(threadIdx2);
		//	});
		SendAddedData(threadIdx);
	}
}

void BufOverManager::ClearSendingData() {
	std::lock_guard<std::mutex> lock(sendingDataLock);
	sendingData.clear();
}

BufOver* BufOverManager::Get(int threadIdx) {
	managedExOversLock[threadIdx].lock();
	auto& exOverList = managedExOvers[threadIdx];
	if(exOverList.empty()){
		managedExOversLock[threadIdx].unlock();
		auto bufOver = new BufOver;
		bufOver->SetManager(this);
		return bufOver;
	}
	auto back = exOverList.back();
	exOverList.pop_back();
	managedExOversLock[threadIdx].unlock();
	back->InitOver();
	return back;
}

void BufOverManager::Recycle(BufOver* usableGroup, int threadIdx) {
	managedExOversLock[threadIdx].lock();
	usableGroup->InitOver();
	managedExOvers[threadIdx].push_back(usableGroup);
	managedExOversLock[threadIdx].unlock();

	if(0>--globalRecycleRemainCount){
		size_t maxSize = 0;
		size_t minSize = 1000000000000;
		size_t maxIndex, minIndex;
		for (size_t i = 0; i < THREAD_COUNT; i++) {
			managedExOversLock[i].lock();
			auto size = managedExOvers[i].size();
			if (size > maxSize) {
				maxSize = size;
				maxIndex = i;
			}
			if (size < minSize) {
				minSize = size;
				minIndex = i;
			}
		}
		auto diff = (maxSize - minSize) * 0.6f;
		for (size_t i = 0; i < diff; i++) {
			auto maxFront = managedExOvers[maxIndex].front();
			managedExOvers[minIndex].push_back(maxFront);
			managedExOvers[maxIndex].pop_front();
		}

		for (size_t i = 0; i < THREAD_COUNT; i++) {
			managedExOversLock[i].unlock();
		}
		globalRecycleRemainCount = GLOBAL_RECYCLE_REMAIN_COUNT;
	}
}

//std::vector<unsigned char>* BufOverManager::GetSendingDataQueue() {
//	std::lock_guard<std::mutex> lock(sendingDataQueueLock); //TODO 앞뒤 리스트 방식으로 바꾸기
//	auto size = sendingDataQueue.size();
//	if (0 < size) {
//		const auto result = sendingDataQueue[size - 1];
//		sendingDataQueue.pop_back();
//		return result;
//	}
//	return new std::vector<unsigned char>();
//}
//
//void BufOverManager::RecycleSendingDataQueue(std::vector<unsigned char>* recycleQueue) {
//	std::lock_guard<std::mutex> lock(sendingDataQueueLock);
//	sendingDataQueue.push_back(recycleQueue);
//}

void BufOverManager::AddSendTimer(int threadIdx) {
	TimerQueueManager::Add(id, 12, threadIdx, [&]() {
		                       return HasSendData();
	                       }, [this](int size, int threadIdx2) {
		                       SendAddedData(threadIdx2);
	                       });
}

void BufOverManager::DebugProcessPacket(unsigned char* buf) {
	switch (buf[1]) {
	case SC_STAT_CHANGE:
	case S2C_CHAT:
	case S2C_REMOVE_PLAYER:
	case S2C_ADD_PLAYER:
	case S2C_LOGIN_OK:
	case S2C_MOVE_PLAYER: {
		break;
	}
	default: {
		static std::mutex coutLock;
		{
			std::lock_guard<std::mutex> lock(coutLock);
			std::cout << "Unknown Packet Type from Client[" << id;
			std::cout << "] Packet Type [" << +buf[1] << "]\n";
		}
		_ASSERT(false);
	}
	}
}

void EmptyFunction(int size, int threadIdx) {

}

void BufOverManager::SendAddedData(int threadIdx) {
	
	//std::cout << "Send!!!!: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
	
	auto exOver = Get(threadIdx);
	sendingDataLock.lock();
	auto totalSendDataSize = sendingData.size();
	if(totalSendDataSize <= 0){
		sendingDataLock.unlock();
		return;
	}
	exOver->packetBuf.resize(totalSendDataSize);
	memcpy(&exOver->packetBuf[0], &sendingData[0], totalSendDataSize);
	sendingData.clear();
	sendingDataLock.unlock();
	
	//exOver->callback = [=](int size){ if(size != totalSendDataSize){
	//	std::cout << "뭐꼬" << std::endl;
	//	_ASSERT(false);
	//}};
	exOver->callback = EmptyFunction;
	exOver->wsabuf[0].buf = reinterpret_cast<CHAR*>(&exOver->packetBuf[0]);
	exOver->wsabuf[0].len = totalSendDataSize;

	std::vector<unsigned char> debugPacket;
	debugPacket.resize(totalSendDataSize);
	memcpy(&debugPacket[0], &exOver->packetBuf[0], totalSendDataSize);
	
	int ret;
	{
		std::lock_guard <std::mutex> lock{ session->socketLock };
		ret = WSASend(session->socket, exOver->wsabuf, 1, NULL, 0, &exOver->over, NULL);// TODO GetQueuedCompletionStatus에서 얼마 보냈는지 확인해서 안갔으면 더 보내기
		//std::cout << "totalSendDataSize["<<id<<"]: "<<totalSendDataSize << std::endl;
	}
	if (0 != ret) {
		auto err = WSAGetLastError();
		if (WSA_IO_PENDING != err) {
			PrintSocketError("WSASend : ", WSAGetLastError());
			player->Disconnect(threadIdx);
			return;
		}
	}

	lastSendTime = std::chrono::system_clock::now();

	//auto debugPacketPtr = &debugPacket[0];
	//for (unsigned char recvPacketSize = debugPacket[0];
	//	0 < totalSendDataSize;
	//	recvPacketSize = debugPacketPtr[0]) {
	//	DebugProcessPacket(debugPacketPtr);
	//	totalSendDataSize -= recvPacketSize;
	//	debugPacketPtr += recvPacketSize;
	//}
	
	//auto& copiedSendingData = *GetSendingDataQueue();
	//copiedSendingData.resize(totalSendDataSize);
	//memcpy(&copiedSendingData[0], &sendingData[0], totalSendDataSize);
	//sendingData.clear();
	//sendingDataLock.unlock();

	//auto sendDataBegin = &copiedSendingData[0];

	//while (0 < totalSendDataSize) {
	//	const auto sendDataSize = static_cast<int>(totalSendDataSize);
	//	//auto sendDataSize = min(MAX_BUFFER, (int)totalSendDataSize);
	//	auto exOver = Get();
	//	exOver->callback = EmptyFunction;
	//	exOver->packetBuf.resize(sendDataSize);
	//	memcpy(&exOver->packetBuf[0], sendDataBegin, sendDataSize);
	//	exOver->wsabuf[0].buf = reinterpret_cast<CHAR*>(&exOver->packetBuf[0]);
	//	exOver->wsabuf[0].len = sendDataSize;
	//	int ret;
	//	{
	//		std::lock_guard <std::mutex> lock{ session->socketLock };
	//		ret = WSASend(session->socket, exOver->wsabuf, 1, NULL, 0, &exOver->over, NULL);// TODO GetQueuedCompletionStatus에서 얼마 보냈는지 확인해서 안갔으면 더 보내기
	//		//std::cout << "totalSendDataSize["<<id<<"]: "<<totalSendDataSize << std::endl;
	//	}
	//	if (0 != ret) {
	//		auto err = WSAGetLastError();
	//		if (WSA_IO_PENDING != err) {
	//			PrintSocketError("WSASend : ", WSAGetLastError());
	//			player->Disconnect();
	//			return;
	//		}
	//	}
	//	totalSendDataSize -= sendDataSize;
	//	sendDataBegin += sendDataSize;
	//}
	//RecycleSendingDataQueue(&copiedSendingData);
}