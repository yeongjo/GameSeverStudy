#include "BufOverManager.h"

#include "OverlappedStruct.h"
#include "Player.h"
#include "SocketUtil.h"
#include "TimerQueueManager.h"

size_t BufOverManager::EX_OVER_SIZE_INCREMENT = 2;

BufOverManager::BufOverManager(Player* player) : session(player->GetSession()),
                                                 player(player),
                                                 id(player->GetId()) {
}

BufOverManager::~BufOverManager() {
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
	const auto prevSize = remainBufSize;
	remainBufSize += packetSize;
	const auto totalSendPacketSize = remainBufSize;
	sendingData.resize(totalSendPacketSize);
	memcpy(&sendingData[prevSize], p, packetSize);
}

void BufOverManager::ClearSendingData() {
	std::lock_guard<std::mutex> lock(sendingDataLock);
	sendingData.clear();
	remainBufSize = 0;
}

BufOver* BufOverManager::Get() {
	auto exover = managedExOvers.Get();
	exover->SetManager(this);
	return exover; // TODO WSAOVERLAPPED	over 초기화 잘되는지 확인하기
}

void BufOverManager::Recycle(BufOver* usableGroup) {
	usableGroup->InitOver();
	managedExOvers.Recycle(usableGroup);
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

void BufOverManager::AddSendTimer() {
	TimerQueueManager::Add(id, 12, [&]() {
		                       return HasSendData();
	                       }, [this](int size) {
		                       SendAddedData();
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

void EmptyFunction(int size) {

}

void BufOverManager::SendAddedData() {
	auto exOver = Get();
	sendingDataLock.lock();
	auto totalSendDataSize = remainBufSize;
	if (totalSendDataSize == 0) {
		sendingDataLock.unlock();
		exOver->Recycle();
		return;
	}
	//std::cout << "Send!!!!: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;

	remainBufSize -= totalSendDataSize;
	if (remainBufSize > 0) {
		AddSendTimer();
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
			player->Disconnect();
			return;
		}
	}

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