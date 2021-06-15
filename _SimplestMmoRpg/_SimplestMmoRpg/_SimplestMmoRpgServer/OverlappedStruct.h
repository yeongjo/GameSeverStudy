#pragma once
#include <cstring>
#include <functional>
#include <winsock2.h>
#include <ws2def.h>

#include "protocol.h"

struct BufOverManager;
typedef std::function<void(int)> iocpCallback;

struct MiniOver {
	WSAOVERLAPPED	over; // Ŭ���� �����ڿ��� �ʱ�ȭ�ϴ� ���� ������� ���ƿ´�??
	iocpCallback callback;
	virtual void Recycle() {}
};
struct AcceptOver : public MiniOver {
	SOCKET			cSocket;					// OP_ACCEPT������ ���
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