#pragma once
#include <cstring>
#include <functional>
#include <winsock2.h>
#include <ws2def.h>

#include "protocol.h"

struct BufOverManager;
typedef std::function<void(int, int)> iocpCallback; // recvBytes, threadIndex

struct MiniOver {
	WSAOVERLAPPED	over; // 클래스 생성자에서 초기화하니 값이 원래대로 돌아온다??
	iocpCallback callback;
	virtual void Recycle(int threadIdx) {}
};
struct AcceptOver : public MiniOver {
	SOCKET			cSocket;					// OP_ACCEPT에서만 사용
	WSABUF			wsabuf[1];
};
struct BufOverBase : public MiniOver {
	WSABUF			wsabuf[1];
public:
	BufOverBase() {
		InitOver();
	}
	virtual void InitOver() {
		memset(&over, 0, sizeof(over));
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
	BufOver(BufOverManager* manager) : BufOverBase(), manager(manager) { }
	void InitOver() {
		BufOverBase::InitOver();
	}

	void Recycle(int threadIdx) override;
	friend BufOverManager;
private:
	void SetManager(BufOverManager* manager);

	BufOverManager* GetManager() const {
		return manager;
	}
};
struct RecvOver : public BufOverBase {
	unsigned char packetBuf[RECV_MAX_BUFFER];
	void Recycle(int threadIdx) override {}
};