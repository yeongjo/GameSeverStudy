#pragma once
#include <chrono>
#include <functional>
#include <mutex>
#include <unordered_map>

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
	MiniOver* bufOver;
	char buffer[MESSAGE_MAX_BUFFER];
	bool hasBuffer;
	int actorTimerId; // 액터와 비교해서 액터 숫자가 높으면 큐가 끝나도 폐기한다

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
	std::mutex timerLock;
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

struct TimerQueueRemainTime {
	TimerQueue* timerQueue;
	std::chrono::time_point<std::chrono::system_clock> startTime;
	
	TimerQueueRemainTime(TimerQueue* timerQueue, std::chrono::time_point<std::chrono::system_clock> startTime):timerQueue(timerQueue), startTime(startTime){
		
	}
};

constexpr int TIMER_QUEUE_COUNT = THREAD_COUNT;
constexpr auto THREAD_WAITTIME = std::chrono::milliseconds(10) / TIMER_QUEUE_COUNT;

class TimerQueueManager {
	static std::array<TimerQueue, TIMER_QUEUE_COUNT> timerQueues;
	static std::array<std::chrono::time_point<std::chrono::system_clock>, TIMER_QUEUE_COUNT> startTime;
	static HANDLE hIocp;

public:
	static void Add(TimerEvent& event, int threadIdx);

	//static void RemoveAll(int playerId); // TODO 다 지우는거 대체할방법 생각하기

	static void Add(int obj, int delayMs, int threadIdx, TimerEventCheckCondition checkCondition, iocpCallback callback);

	static void Post(int obj, int threadIdx, iocpCallback callback);

	static void Do();

	static void SetIocpHandle(HANDLE hIocp);
};
