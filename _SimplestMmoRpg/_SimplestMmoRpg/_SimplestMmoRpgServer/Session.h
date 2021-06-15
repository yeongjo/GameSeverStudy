#pragma once
#include <atomic>
#include <mutex>
#include <vector>
#include "winsock2.h"

#include "BufOverManager.h"
#include "OverlappedStruct.h"

enum EPlayerState { PLST_FREE, PLST_CONNECTED, PLST_INGAME };

struct Session {
	std::atomic<EPlayerState> state;
	RecvOver recvOver;
	BufOverManager bufOverManager;

	/// <summary>
	/// socketLock으로 잠궈주고 사용
	/// </summary>
	SOCKET socket;
	std::mutex  socketLock;

	/// <summary>
	/// 패킷 한개보다 작은 사이즈가 들어감
	/// recvedBufLock으로 잠궈주고 사용
	/// </summary>
	std::vector<unsigned char> recvedBuf; // 패킷 한개보다 작은 사이즈가 들어감
	std::mutex recvedBufLock;
	std::atomic<unsigned char> recvedBufSize;

	Session(Player* player);
};

inline Session::Session(Player* player): bufOverManager(player) {
}
