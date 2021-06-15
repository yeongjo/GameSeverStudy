#include "Player.h"

#include <iostream>

#include "Sector.h"
#include "SocketUtil.h"

Player* Player::Create(int id) {
	return new Player(id);
}

Player* Player::Get(int id) {
	_ASSERT(0 < id && id < NPC_ID_START);
	return reinterpret_cast<Player*>(Actor::Get(id));
}

int Player::GetNewId(SOCKET socket) {
	for (int i = 1; i <= MAX_PLAYER; ++i){
		auto actor = Get(i);
		auto session = actor->GetSession();
		if (PLST_FREE == session->state){
			session->state = PLST_CONNECTED;
			std::lock_guard<std::mutex> lg{session->socketLock};
			session->socket = socket;
			actor->name[0] = 0;
			return i;
		}
	}
	return -1;
}

void Player::Init() {
	Actor::Init();
	hp = maxHp;
	level = 1;
	exp = 0;
	damage = 1;
}

void Player::Disconnect() {
	auto actor = Get(id);
	if (session.state == PLST_FREE){
		return;
	}
	session.state = PLST_FREE;
	closesocket(session.socket);

	{
		std::lock_guard<std::mutex> lock(session.recvedBufLock);
		session.recvedBuf.clear();
	}
	session.bufOverManager.ClearSendingData();
	actor->RemoveFromAll();
}

void Player::Attack() {
	viewSetLock.lock();
	int size = viewSet.size();
	attackViewList.resize(size);
	std::copy(viewSet.begin(), viewSet.end(), attackViewList.begin());
	viewSetLock.unlock();
	for (auto i = 0; i < size;){
		auto actor = Actor::Get(attackViewList[i]);
		if (actor->IsMonster()){
			if (abs(x - actor->GetX()) + abs(y - actor->GetY()) <= 1){
				actor->TakeDamage(id);
			}
		}
		++i;
	}
}

void Player::Die() {
	//Actor::Die();
	//SendRemoveActor(id, id); // 삭제는 안하고 위치 옮기고 경험치 반 HP 회복해서 시작위치로
	SetPos(initX, initY);
	hp = maxHp;
	exp = exp >> 1;
	SendStatChange();
}

void Player::AddToViewSet(int otherId) {
	viewSetLock.lock();
	if (0 == viewSet.count(id)){
		viewSet.insert(otherId);
		viewSetLock.unlock();
		SendAddActor(otherId);
		return;
	}
	viewSetLock.unlock();
}

void Player::RemoveFromViewSet(int otherId) {
	viewSetLock.lock();
	if (0 != viewSet.count(otherId)){
		viewSet.erase(otherId);
		viewSetLock.unlock();
		SendRemoveActor(otherId);
		return;
	}
	viewSetLock.unlock();
}

void Player::SendLoginOk() {
	sc_packet_login_ok p;
	p.size = sizeof(p);
	p.type = SC_LOGIN_OK;
	p.id = id;
	p.x = GetX();
	p.y = GetY();
	p.HP = GetHp();
	p.LEVEL = GetLevel();
	p.EXP = GetExp();
	session.bufOverManager.AddSendingData(&p);
}

void Player::SendChat(int senderId, const char* mess) {
	sc_packet_chat p;
	p.id = senderId;
	p.size = sizeof(p);
	p.type = SC_CHAT;
	strcpy_s(p.message, mess);
	session.bufOverManager.AddSendingData(&p);
}

void Player::SendMove(int p_id) {
	sc_packet_position p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = SC_POSITION;
	auto actor = Actor::Get(p_id);
	p.x = actor->GetX();
	p.y = actor->GetY();
	p.move_time = actor->GetMoveTime();
	session.bufOverManager.AddSendingData(&p);
}

void Player::SendAddActor(int addedId) {
	sc_packet_add_object p;
	p.id = addedId;
	p.size = sizeof(p);
	p.type = SC_ADD_OBJECT;
	auto actor = Actor::Get(addedId);
	p.x = actor->GetX();
	p.y = actor->GetY();
	p.obj_class = 1;
	p.HP = 1;
	p.LEVEL = 1;
	p.EXP = 1;
	strcpy_s(p.name, actor->GetName().c_str());
	session.bufOverManager.AddSendingData(&p);
}

void Player::SendChangedStat(int statChangedId, int hp, int level, int exp) {
	sc_packet_stat_change p;
	p.id = statChangedId;
	p.size = sizeof(p);
	p.type = SC_STAT_CHANGE;
	p.HP = hp;
	p.LEVEL = level;
	p.EXP = exp;
	session.bufOverManager.AddSendingData(&p);
}

MiniOver* Player::GetOver() {
	return session.bufOverManager.Get();
}
Player::Player(int id) : Actor(id), session(this) {
	actors[id] = this;
	session.state = PLST_FREE;
	session.recvOver.callback = [this](int bufSize) {
		auto exOver = session.recvOver;
		unsigned char* recvPacketPtr;
		auto totalRecvBufSize = static_cast<unsigned char>(bufSize);
		if (session.recvedBufSize > 0){
			// 남아있는게 있으면 남아있던 한패킷만 처리
			const unsigned char prevPacketSize = session.recvedBuf[0];
			const unsigned char splitBufSize = prevPacketSize - session.recvedBufSize;
			session.recvedBuf.resize(prevPacketSize);
			memcpy(1 + &session.recvedBuf.back(), exOver.packetBuf, splitBufSize);
			recvPacketPtr = &session.recvedBuf[0];
			ProcessPacket(recvPacketPtr);
			recvPacketPtr = exOver.packetBuf + splitBufSize;
			totalRecvBufSize -= splitBufSize;
		}
		else{
			recvPacketPtr = exOver.packetBuf;
		}

		for (unsigned char recvPacketSize = recvPacketPtr[0];
		     0 < totalRecvBufSize;
		     recvPacketSize = recvPacketPtr[0]){
			ProcessPacket(recvPacketPtr);
			totalRecvBufSize -= recvPacketSize;
			recvPacketPtr += recvPacketSize;
		}
		{
			std::lock_guard<std::mutex> lock(session.recvedBufLock);
			if (0 < totalRecvBufSize){
				session.recvedBuf.resize(totalRecvBufSize);
				session.recvedBufSize = totalRecvBufSize;
				memcpy(&session.recvedBuf[0], recvPacketPtr, totalRecvBufSize);
			}
			else{
				session.recvedBuf.clear();
				session.recvedBufSize = 0;
			}
		}
		CallRecv();
	};
}

void Player::SetPos(int x, int y) {
	auto prevX = this->x;
	auto prevY = this->y;
	this->x = x;
	this->y = y;

	SendMove(id);

	SECTOR::Move(id, prevX, prevY, x, y);

	auto&& new_vl = SECTOR::GetIdFromOverlappedSector(id);

	std::lock_guard<std::mutex> lock(oldNewViewListLock);
	CopyViewSetToOldViewList();

	for (auto otherId : new_vl) {
		if (oldViewList.end() == std::find(oldViewList.begin(), oldViewList.end(), otherId)) {
			//1. 새로 시야에 들어오는 플레이어
			AddToViewSet(otherId);
			Actor::Get(otherId)->AddToViewSet(id);
		} else {
			//2. 기존 시야에도 있고 새 시야에도 있는 경우
			if (!Actor::Get(otherId)->IsNpc()) {
				Get(otherId)->SendMove(id);
			} else {
#ifdef PLAYERLOG
				{
					lock_guard<mutex> coutLock{ coutMutex };
					cout << "플레이어[" << id << "]이 " << actor->x << "," << actor->y << " 움직여서 npc[" << pl << "]가 갱신" << endl;
				}
#endif
				Actor::Get(otherId)->OnNearActorWithPlayerMove(id);
			}
		}
	}
	for (auto otherId : oldViewList) {
		if (new_vl.end() == std::find(new_vl.begin(), new_vl.end(), otherId)) {
			// 기존 시야에 있었는데 새 시야에 없는 경우
			RemoveFromViewSet(otherId);
			Actor::Get(otherId)->RemoveFromViewSet(id);
		}
	}
}

void Player::SendStatChange() {
	SendChangedStat(id, hp, level, exp);
	Actor::SendStatChange();
}

bool Player::TakeDamage(int attackerId) {
	hp -= damage;
	if (hp < 0) {
		hp = 0;
		SendStatChange();
		return true;
	}
	SendStatChange();
	return false;
}

void Player::ProcessPacket(unsigned char* buf) {
	switch (buf[1]) {
	case CS_LOGIN: {
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(buf);
		session.state = PLST_INGAME;
		{
			// 위치 이름 초기화
			std::lock_guard<std::mutex> lock{ session.socketLock };
			strcpy_s(name, packet->player_id);

			x = rand() % WORLD_WIDTH;
			y = rand() % WORLD_HEIGHT;
#ifdef PLAYER_NOT_RANDOM_SPAWN
			x = 0;
			y = 0;
#endif
			Init();
		}
		isActive = true;
		InitSector();
		SendLoginOk();

		auto& selected_sector = SECTOR::GetIdFromOverlappedSector(id);

		viewSetLock.lock();
		for (auto otherId : selected_sector) {
			viewSet.insert(otherId);
		}
		viewSetLock.unlock();
		for (auto otherId : selected_sector) {
			auto other = Get(otherId);
			other->AddToViewSet(id);
			SendAddActor(otherId);
			if (!other->IsNpc()) {
				Get(otherId)->SendAddActor(id);
			}
		}
		break;
	}
	case CS_MOVE: {
		auto packet = reinterpret_cast<cs_packet_move*>(buf);
		SetMoveTime(packet->move_time);
		Move(static_cast<DIRECTION>(packet->direction));
		break;
	}
	case CS_ATTACK: {
		auto* packet = reinterpret_cast<cs_packet_attack*>(buf);
		Attack();
		break;
	}
	case CS_CHAT: {
		auto* packet = reinterpret_cast<cs_packet_chat*>(buf);
		std::lock_guard<std::mutex> lock(viewSetLock);
		for (auto viewId : viewSet) {
			if (!Actor::Get(viewId)->IsNpc()) {
				Get(viewId)->SendChat(id, packet->message);
			}
		}
		break;
	}
	default: {
		std::cout << "Unknown Packet Type from Client[" << id;
		std::cout << "] Packet Type [" << +buf[1] << "]";
		while (true);
	}
	}
}

void Player::SendRemoveActor(int removeTargetId) {
	sc_packet_remove_object p;
	p.id = removeTargetId;
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	session.bufOverManager.AddSendingData(&p);
}

void Player::CallRecv() {
	auto& recvOver = session.recvOver;
	recvOver.wsabuf[0].buf =
		reinterpret_cast<char*>(recvOver.packetBuf);
	recvOver.wsabuf[0].len = MAX_BUFFER;
	memset(&recvOver.over, 0, sizeof(recvOver.over));
	DWORD r_flag = 0;
	int ret = WSARecv(session.socket, recvOver.wsabuf, 1, NULL, &r_flag, &recvOver.over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			display_error("WSARecv : ", WSAGetLastError());
	}
}