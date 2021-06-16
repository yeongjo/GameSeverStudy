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
	return sendBufSize;
}

void BufOverManager::AddSendingData(void* p, int threadIdx) {
	const auto packetSize = static_cast<size_t>(static_cast<unsigned char*>(p)[0]);
	//std::cout << "AddSendingData: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
	
	{
		auto& sendBuffer = sendBufs[threadIdx];
		std::lock_guard<std::mutex> lock(sendBuffer.dataLock);
		sendBufSize += packetSize;
		auto prevSize = sendBuffer.sendingBuf.size();
		sendBuffer.sendingBuf.resize(prevSize +packetSize);
		memcpy(&sendBuffer.sendingBuf[prevSize], p, packetSize);
	}
	AddSendTimer(threadIdx);
}

void BufOverManager::ClearSendingData() {
	for (size_t i = 0; i < THREAD_COUNT; i++){
		std::lock_guard<std::mutex> lock(sendBufs[i].dataLock);
		sendBufs[i].sendingBuf.clear();
	}
	sendBufSize = 0;
}

BufOver* BufOverManager::Get(int threadIdx) {
	std::lock_guard<std::mutex> lock(managedExOversLock[threadIdx]);
	auto& exOverList = managedExOvers[threadIdx];
	if (exOverList.empty()){
		auto bufOver = new BufOver;
		bufOver->InitOver();
		bufOver->packetBuf.resize(SEND_MAX_BUFFER);
		bufOver->SetManager(this);
		return bufOver;
	}
	auto pop = exOverList.front();
	exOverList.pop_front();
	pop->InitOver();
	return pop;
}

void BufOverManager::Recycle(BufOver* usableGroup, int threadIdx) {
	usableGroup->InitOver();
	{
		std::lock_guard<std::mutex> lock(managedExOversLock[threadIdx]);
		managedExOvers[threadIdx].push_back(usableGroup);
	}

	if (0 > --globalRecycleRemainCount){
		globalRecycleRemainCount = GLOBAL_RECYCLE_REMAIN_COUNT;
		size_t maxSize = 0;
		size_t minSize = 1000000000000;
		size_t maxIndex, minIndex;
		for (size_t i = 0; i < THREAD_COUNT; i++){
			managedExOversLock[i].lock();
			auto size = managedExOvers[i].size();
			if (size > maxSize){
				maxSize = size;
				maxIndex = i;
			}
			if (size < minSize){
				minSize = size;
				minIndex = i;
			}
		}
		auto diff = (maxSize - minSize) * 0.6f;
		for (size_t i = 0; i < diff; i++){
			auto maxFront = managedExOvers[maxIndex].front();
			managedExOvers[minIndex].push_back(maxFront);
			managedExOvers[maxIndex].pop_front();
		}

		for (size_t i = 0; i < THREAD_COUNT; i++){
			managedExOversLock[i].unlock();
		}
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
	switch (buf[1]){
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

void PrintPacket(int debugIndex, unsigned char* debugPacketPtr) {
	std::cout << debugIndex << ": [" << +debugPacketPtr[0] << ",";
	const auto packetSize = debugPacketPtr[0];
	for (unsigned char i = 0; i < packetSize; i++) {
		std::cout << +debugPacketPtr[i] << ",";
	}
	std::cout << "]\n";
}

void BufOverManager::SendAddedData(int threadIdx) {

	//std::cout << "Send!!!!: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
	if (0 == sendBufSize){
		return;
	}
	auto exOver = Get(threadIdx);
	exOver->packetBuf.clear();
	auto bufPtr = 0;
	for (size_t i = 0; i < THREAD_COUNT; i++){
		int size;
		{
			std::lock_guard<std::mutex> lock(sendBufs[i].dataLock);
			size = sendBufs[i].sendingBuf.size();
			if (size == 0) {
				continue;
			}
			exOver->packetBuf.insert(exOver->packetBuf.end(), sendBufs[i].sendingBuf.begin(), sendBufs[i].sendingBuf.end());
			sendBufs[i].sendingBuf.clear();
		}
		bufPtr += size;
	}
	auto size = exOver->packetBuf.size();
	if(size == 0){
		return;
	}
	sendBufSize -= size;

	//exOver->callback = [=](int size){ if(size != totalSendDataSize){
	//	std::cout << "뭐꼬" << std::endl;
	//	_ASSERT(false);
	//}};
	exOver->callback = EmptyFunction;
	exOver->wsabuf[0].buf = reinterpret_cast<CHAR*>(&exOver->packetBuf[0]);
	exOver->wsabuf[0].len = size;

#ifdef PACKETLOG
	std::vector<unsigned char> debugPacket(exOver->packetBuf.begin(), exOver->packetBuf.end());
#endif

	int ret;
	{
		std::lock_guard<std::mutex> lock{session->socketLock};
		ret = WSASend(session->socket, exOver->wsabuf, 1, nullptr, 0, &exOver->over, nullptr);
		// TODO GetQueuedCompletionStatus에서 얼마 보냈는지 확인해서 안갔으면 더 보내기
		//std::cout << "totalSendDataSize["<<id<<"]: "<<totalSendDataSize << std::endl;
	}
	if (0 != ret){
		auto err = WSAGetLastError();
		if (WSA_IO_PENDING != err){
			PrintSocketError("WSASend : ", WSAGetLastError());
			player->Disconnect(threadIdx);
		}
	}

#ifdef PACKETLOG
	auto debugSize = size;
	auto debugIndex = 0;
	auto debugPacketPtr = &debugPacket[0];
	for (unsigned char recvPacketSize = debugPacket[0];
		0 < debugSize;
		recvPacketSize = debugPacketPtr[0]) {
		if (recvPacketSize == 0) {
			PrintPacket(debugIndex, debugPacketPtr);
			_ASSERT(false);
		}
		DebugProcessPacket(debugPacketPtr);
		debugSize -= recvPacketSize;
		debugPacketPtr += recvPacketSize;
		debugIndex += recvPacketSize;
	}
#endif
}
