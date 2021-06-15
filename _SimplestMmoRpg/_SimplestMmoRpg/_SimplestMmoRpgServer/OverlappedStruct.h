#pragma once
#include <cstring>
#include <functional>
#include <winsock2.h>
#include <ws2def.h>

#include "protocol.h"

struct BufOverManager;
typedef std::function<void(int, int)> iocpCallback; // recvBytes, threadIndex

struct MiniOver {
	WSAOVERLAPPED	over; // Ŭ���� �����ڿ��� �ʱ�ȭ�ϴ� ���� ������� ���ƿ´�??
	iocpCallback callback;
	virtual void Recycle(int threadIdx) {}
};
struct AcceptOver : public MiniOver {
	SOCKET			cSocket;					// OP_ACCEPT������ ���
	WSABUF			wsabuf[1];
};
struct BufOverBase : public MiniOver {
	WSABUF			wsabuf[1];
private:
	BufOverManager* manager;
public:
	BufOverBase() {
		InitOver();
	}
	BufOverBase(BufOverManager* manager) : manager(manager) {
		InitOver();
	}
	virtual void InitOver() {
		memset(&over, 0, sizeof(over));
	}

	void Recycle(int threadIdx) override {}

	friend BufOverManager;
private:
	void SetManager(BufOverManager* manager) {
		this->manager = manager;
	}

	BufOverManager* GetManager() {
		return manager;
	}
};
struct BufOver : public BufOverBase {
	WSABUF			wsabuf[1];
	std::vector<unsigned char>	packetBuf;
	int threadIdx;
private:
	BufOverManager* manager;
public:
	BufOver() : BufOverBase(){ }
	BufOver(BufOverManager* manager) : BufOverBase(manager) { }
	void InitOver() {
		BufOverBase::InitOver();
		packetBuf.resize(SEND_MAX_BUFFER);
	}

	void Recycle(int threadIdx) override;

	friend BufOverManager;
private:
	void SetManager(BufOverManager* manager) {
		this->manager = manager;
	}

	BufOverManager* GetManager() {
		return manager;
	}
};
struct RecvOver : public BufOverBase {
	unsigned char packetBuf[RECV_MAX_BUFFER];
	void Recycle(int threadIdx) override {}
};