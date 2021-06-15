#pragma once
#include <chrono>
#include <functional>
#include <mutex>

#include "removable_priority_queue.h"
#include "OverlappedStruct.h"
#include "protocol.h"
typedef std::function<bool()> TimerEventCheckCondition;

struct TimerEvent {
	int object;
	/// <summary>
	/// false면 타이머에서 빠진다.
	/// </summary>
	TimerEventCheckCondition checkCondition;
	iocpCallback callback;
	std::chrono::system_clock::time_point startTime;
	int targetId;
	char buffer[MESSAGE_MAX_BUFFER];
	bool hasBuffer;

	constexpr bool operator<(const TimerEvent& L) const;

	constexpr bool operator==(const TimerEvent& L) const;
};

constexpr bool TimerEvent::operator<(const TimerEvent& L) const {
	return (startTime > L.startTime);
}

constexpr bool TimerEvent::operator==(const TimerEvent& L) const {
	return (object == L.object);
}

class TimerQueue : public removable_priority_queue<TimerEvent> {
public:
	// TODO Add할때 id 리턴받고 그거 바탕으로 지워야할듯 근데 당장은 쓸일없음
	//bool remove(const int playerId, const EEventType eventType) {
	//	auto size = this->c.size();
	//	for (size_t i = 0; i < size;) {
	//		if (this->c[i].object == playerId && this->c[i].eventType == eventType) {
	//			auto begin = this->c.begin();
	//			this->c.erase(begin + i);
	//			--size;
	//			return true;
	//		}
	//		++i;
	//	}
	//	return false;
	//}
	void remove_all(const int playerId);
};

class TimerQueueManager {
	/// <summary>
	/// timerLock으로 잠궈주고 사용
	/// </summary>
	static TimerQueue timerQueue;
	static std::mutex timerLock;
	static HANDLE hIocp;

public:
	static void Add(TimerEvent event);

	static void RemoveAll(int playerId);

	static void Add(int obj, int delayMs, TimerEventCheckCondition checkCondition, iocpCallback callback,
	                const char* buffer = 0, int targetId = 0);

	static void Do();

	static void SetIocpHandle(HANDLE hIocp);
};
