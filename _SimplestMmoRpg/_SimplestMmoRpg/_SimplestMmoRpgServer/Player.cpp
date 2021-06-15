#include "Player.h"

#include <iostream>

#include "Sector.h"
#include "SocketUtil.h"
#include "StringUtil.h"

Player* Player::Create(int id) {
	return new Player(id);
}

Player* Player::Get(int id) {
	_ASSERT(0 < id && id <= PLAYER_ID_END);
	return reinterpret_cast<Player*>(Actor::Get(id));
}

int Player::GetNewId(SOCKET socket) {
	for (int i = 1; i <= PLAYER_ID_END; ++i){
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
	wname = s2ws(name);
	int tx = x;
	int ty = y;
	DB::LoginQuery(wname, wname, hp, level, exp, tx, ty);
	x = tx;
	y = ty;
}

void Player::Disconnect(int threadIdx) {
	if (session.state == PLST_FREE){
		return;
	}
	session.state = PLST_FREE;
	closesocket(session.socket);
	session.bufOverManager.ClearSendingData();
	RemoveFromAll(threadIdx);
}

void Player::Attack(int threadIdx) {
	viewSetLock.lock();
	int size = viewSet.size();
	attackViewList.resize(size);
	std::copy(viewSet.begin(), viewSet.end(), attackViewList.begin());
	viewSetLock.unlock();
	for (auto i = 0; i < size;){
		auto actor = Actor::Get(attackViewList[i]);
		if (actor->IsMonster()){
			if (abs(x - actor->GetX()) + abs(y - actor->GetY()) <= 1){
				actor->TakeDamage(id, threadIdx);
			}
		}
		++i;
	}
}

void Player::Die(int threadIdx) {
	//Actor::Die();
	//SendRemoveActor(id, id); // 삭제는 안하고 위치 옮기고 경험치 반 HP 회복해서 시작위치로
	SetPos(initX, initY, threadIdx);
	hp = maxHp;
	exp = exp >> 1;
	SendStatChange(threadIdx);
}

void Player::AddToViewSet(int otherId, int threadIdx) {
	viewSetLock.lock();
	if (0 == viewSet.count(id)){
		viewSet.insert(otherId);
		viewSetLock.unlock();
		SendAddActor(otherId, threadIdx);
		return;
	}
	viewSetLock.unlock();
}

void Player::RemoveFromViewSet(int otherId, int threadIdx) {
	std::lock_guard<std::mutex> lock(viewSetLock);
	RemoveFromViewSetWithoutLock(otherId, threadIdx);
}

void Player::RemoveFromViewSetWithoutLock(int otherId, int threadIdx) {
	if (0 != viewSet.count(otherId)) {
		viewSet.erase(otherId);
		SendRemoveActor(otherId, threadIdx);
	}
}

void Player::SendLoginOk(int threadIdx) {
	sc_packet_login_ok p;
	p.size = sizeof(p);
	p.type = SC_LOGIN_OK;
	p.id = id;
	p.x = GetX();
	p.y = GetY();
	p.HP = GetHp();
	p.LEVEL = GetLevel();
	p.EXP = GetExp();
	session.bufOverManager.AddSendingData(&p, threadIdx);
}

void Player::SendChat(int senderId, const char* mess, int threadIdx) {
	sc_packet_chat p;
	p.id = senderId;
	p.size = sizeof(p);
	p.type = SC_CHAT;
	strcpy_s(p.message, mess);
	session.bufOverManager.AddSendingData(&p, threadIdx);
}

void Player::SendMove(int p_id, int threadIdx) {
	sc_packet_position p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = SC_POSITION;
	auto actor = Actor::Get(p_id);
	p.x = actor->GetX();
	p.y = actor->GetY();
	p.move_time = actor->GetMoveTime();
	session.bufOverManager.AddSendingData(&p, threadIdx);
}

void Player::SendAddActor(int addedId, int threadIdx) {
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
	session.bufOverManager.AddSendingData(&p, threadIdx);
}

void Player::SendChangedStat(int statChangedId, int hp, int level, int exp, int threadIdx) {
	sc_packet_stat_change p;
	p.id = statChangedId;
	p.size = sizeof(p);
	p.type = SC_STAT_CHANGE;
	p.HP = hp;
	p.LEVEL = level;
	p.EXP = exp;
	session.bufOverManager.AddSendingData(&p, threadIdx);
}

Player::Player(int id) : Actor(id), session(this) {
	actors[id] = this;
	session.state = PLST_FREE;
	session.recvOver.wsabuf[0].buf =
		reinterpret_cast<char*>(&session.recvOver.packetBuf[0]);
	session.recvOver.wsabuf[0].len = RECV_MAX_BUFFER;
	session.recvOver.callback = [this](int bufSize, int threadIdx) {
		auto exOver = session.recvOver;
		unsigned char* recvPacketPtr;
		auto totalRecvBufSize = static_cast<unsigned char>(bufSize);
		if (session.recvedBufSize > 0){
			// 남아있는게 있으면 남아있던 한패킷만 처리
			const unsigned char prevPacketSize = session.recvedBuf[0];
			const unsigned char splitBufSize = prevPacketSize - session.recvedBufSize;
			session.recvedBuf.resize(prevPacketSize);
			memcpy(1 + &session.recvedBuf.back(), &exOver.packetBuf[0], splitBufSize);
			recvPacketPtr = &session.recvedBuf[0];
			ProcessPacket(recvPacketPtr, threadIdx);
			recvPacketPtr = &exOver.packetBuf[0] + splitBufSize;
			totalRecvBufSize -= splitBufSize;
		}
		else{
			recvPacketPtr = &exOver.packetBuf[0];
		}

		for (unsigned char recvPacketSize = recvPacketPtr[0];
		     0 < totalRecvBufSize;
		     recvPacketSize = recvPacketPtr[0]){
			ProcessPacket(recvPacketPtr, threadIdx);
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

void Player::SetPos(int x, int y, int threadIdx) {
	auto prevX = this->x;
	auto prevY = this->y;
	this->x = x;
	this->y = y;

	SendMove(id, threadIdx);

	Sector::Move(id, prevX, prevY, x, y);

	//std::lock_guard<std::mutex> lock(oldNewViewListLock);
	auto&& new_vl = Sector::GetIdFromOverlappedSector(id);

	CopyViewSetToOldViewList();

	for (auto otherId : new_vl) {
		if (oldViewList.end() == std::find(oldViewList.begin(), oldViewList.end(), otherId)) {
			//1. 새로 시야에 들어오는 플레이어
			AddToViewSet(otherId, threadIdx);
			Actor::Get(otherId)->AddToViewSet(id, threadIdx);
		} else {
			//2. 기존 시야에도 있고 새 시야에도 있는 경우
			if (!Actor::Get(otherId)->IsNpc()) {
				Player::Get(otherId)->SendMove(id, threadIdx);
			} else {
#ifdef PLAYERLOG
				{
					lock_guard<mutex> coutLock{ coutMutex };
					cout << "플레이어[" << id << "]이 " << actor->x << "," << actor->y << " 움직여서 npc[" << pl << "]가 갱신" << endl;
				}
#endif
				auto actor = Actor::Get(otherId);
				auto tx = actor->GetX();
				auto ty = actor->GetY();
				if(x==tx&&y==ty){
					actor->OnNearActorWithPlayerMove(id, threadIdx);
				}
			}
		}
	}
	for (auto otherId : oldViewList) {
		if (new_vl.end() == std::find(new_vl.begin(), new_vl.end(), otherId)) {
			// 기존 시야에 있었는데 새 시야에 없는 경우
			RemoveFromViewSetWithoutLock(otherId, threadIdx);
			Actor::Get(otherId)->RemoveFromViewSet(id, threadIdx);
		}
	}
}

void Player::SendStatChange(int threadIdx) {
	DB::UpdateStat(wname, hp, level, exp, x, y);
	SendChangedStat(id, hp, level, exp, threadIdx);
	Actor::SendStatChange(threadIdx);
}

bool Player::TakeDamage(int attackerId, int threadIdx) {
	hp -= damage;
	if (hp < 0) {
		hp = 0;
		SendStatChange(threadIdx);
		return true;
	}
	if(hp == maxHp - 1){
		AddHealTimer(threadIdx);
	}
	SendStatChange(threadIdx);
	return false;
}

void Player::SetExp(int exp, int threadIdx) {
	auto level = GetLevel();
	auto levelMinusOne = level - 1;
	auto requireExp = levelMinusOne * 100 + 100;
	if (requireExp < exp){
		SetLevel(level + 1);
		exp -= requireExp;
	}
	this->exp = exp;
	SendStatChange(threadIdx);
}

void Player::SetLevel(int level) { this->level = level; }

void Player::SetHp(int hp) { this->hp = hp; }

void Player::ProcessPacket(unsigned char* buf, int threadIdx) {
	switch (buf[1]) {
	case CS_LOGIN: {
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(buf);
		if(packet->player_id[0] == '\0'){
			std::cout << "You need to type id not a black" << std::endl;
			SendChat(id, "You need to type id not a black", threadIdx);
			session.bufOverManager.SendAddedData(threadIdx);
			Disconnect(threadIdx);
			return;
		}
		session.state = PLST_INGAME;
		{
			// 위치 이름 초기화
			std::lock_guard<std::mutex> lock{ session.socketLock };
			strcpy_s(name, packet->player_id);

			//TRUNCATE TABLE user_data로 db 행 다날릴수있음
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
		SendLoginOk(threadIdx);

		auto& selected_sector = Sector::GetIdFromOverlappedSector(id);

		viewSetLock.lock();
		for (auto otherId : selected_sector) {
			viewSet.insert(otherId);
		}
		viewSetLock.unlock();
		for (auto otherId : selected_sector) {
			auto other = Actor::Get(otherId);
			other->AddToViewSet(id, threadIdx);
			SendAddActor(otherId, threadIdx);
			if (!other->IsNpc()) {
				Get(otherId)->SendAddActor(id, threadIdx);
			}
		}
		break;
	}
	case CS_MOVE: {
		auto packet = reinterpret_cast<cs_packet_move*>(buf);
		SetMoveTime(packet->move_time);
		Move(static_cast<DIRECTION>(packet->direction), threadIdx);
		break;
	}
	case CS_ATTACK: {
		auto* packet = reinterpret_cast<cs_packet_attack*>(buf);
		Attack(threadIdx);
		break;
	}
	case CS_CHAT: {
		auto* packet = reinterpret_cast<cs_packet_chat*>(buf);
		std::lock_guard<std::mutex> lock(viewSetLock);
		for (auto viewId : viewSet) {
			if (!Actor::Get(viewId)->IsNpc()) {
				Get(viewId)->SendChat(id, packet->message, threadIdx);
			}
		}
		break;
	}
	default: {
		std::cout << "Unknown Packet Type from Client[" << id;
		std::cout << "] Packet Type [" << +buf[1] << "]\n";
		while (true);
	}
	}
}

void Player::AddHealTimer(int threadIdx) {
	TimerQueueManager::Add(id, 5000, threadIdx, []() {return true; }, [this](int, int threadIdx2) {
		if(GetHp() < maxHp){
			SetHp(GetHp() + 1);
			SendStatChange(threadIdx2);
			if (GetHp() < maxHp) {
				AddHealTimer(threadIdx2);
			}
		}
	});
}

void Player::SendRemoveActor(int removeTargetId, int threadIdx) {
	sc_packet_remove_object p;
	p.id = removeTargetId;
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	session.bufOverManager.AddSendingData(&p, threadIdx);
}

void Player::CallRecv() {
	auto& recvOver = session.recvOver;
	memset(&recvOver.over, 0, sizeof(recvOver.over));
	DWORD r_flag = 0;
	int ret = WSARecv(session.socket, recvOver.wsabuf, 1, NULL, &r_flag, &recvOver.over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			PrintSocketError("WSARecv : ", WSAGetLastError());
	}
}