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
	/// socketLock���� ����ְ� ���
	/// </summary>
	SOCKET socket;
	std::mutex  socketLock;

	/// <summary>
	/// ��Ŷ �Ѱ����� ���� ����� ��
	/// recvedBufLock���� ����ְ� ���
	/// </summary>
	std::vector<unsigned char> recvedBuf; // ��Ŷ �Ѱ����� ���� ����� ��
	std::mutex recvedBufLock;
	std::atomic<unsigned char> recvedBufSize;

	Session(Player* player);
};

inline Session::Session(Player* player): bufOverManager(player) {
}
