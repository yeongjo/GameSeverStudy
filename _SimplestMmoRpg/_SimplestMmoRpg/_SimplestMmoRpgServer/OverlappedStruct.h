#pragma once
#include <cstring>
#include <functional>
#include <winsock2.h>
#include <ws2def.h>

#include "protocol.h"

struct BufOverManager;
typedef std::function<void(int)> iocpCallback;

struct MiniOver {
	WSAOVERLAPPED	over; // 클래스 생성자에서 초기화하니 값이 원래대로 돌아온다??
	iocpCallback callback;
	virtual void Recycle() {}
};
struct AcceptOver : public MiniOver {
	SOCKET			cSocket;					// OP_ACCEPT에서만 사용
	WSABUF			wsabuf[1];
};
struct BufOver : public MiniOver {
	WSABUF			wsabuf[1];
	std::vector<unsigned char>	packetBuf;
private:
	BufOverManager* manager;
public:
	BufOver() {
	}
	BufOver(BufOverManager* manager) :  manager(manager) {
	}
	void InitOver() {
		packetBuf.resize(SEND_MAX_BUFFER);
		memset(&over, 0, sizeof(over));
	}

	void Recycle() override;
	BufOverManager* GetManager() {
		return manager;
	}
};
struct RecvOver : public BufOver {
	void Recycle() override {}
};