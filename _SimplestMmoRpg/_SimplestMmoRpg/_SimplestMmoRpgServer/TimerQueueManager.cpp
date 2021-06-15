#include "TimerQueueManager.h"

#include <utility>

#include "Actor.h"

HANDLE TimerQueueManager::hIocp;
std::array<TimerQueue, TIMER_QUEUE_COUNT> TimerQueueManager::timerQueues;
std::array<std::chrono::time_point<std::chrono::system_clock>, TIMER_QUEUE_COUNT> TimerQueueManager::startTime;
std::unordered_map<int, std::chrono::time_point<std::chrono::system_clock>> TimerQueueManager::ignoreTime;
std::mutex TimerQueueManager::ignoreTimeLock;

void TimerQueue::remove_all(const int playerId) {
	auto size = this->c.size();
	const auto object = playerId;
	for (size_t i = 0; i < size;){
		if (this->c[i].object == object){
			auto begin = this->c.begin();
			this->c.erase(begin + i);
			--size;
			continue;
		}
		++i;
	}
}

void TimerQueueManager::Add(TimerEvent& event, int threadIdx) {
	// 처음 이벤트 호출하는거라 checkCondition이 false면 넣는다
	auto& timerQueue = timerQueues[threadIdx];
	timerQueue.timerLock.lock();
	timerQueue.push(event);
	startTime[threadIdx] = timerQueue.top().startTime;
	timerQueue.timerLock.unlock();

	if (!event.callback) {
		_ASSERT(false);
	}
}

void TimerQueueManager::RemoveAll(int playerId) {
	std::lock_guard<std::mutex> lock(ignoreTimeLock);
	ignoreTime[playerId] = std::chrono::system_clock::now();
}

void TimerQueueManager::Add(int obj, int delayMs, int threadIdx, TimerEventCheckCondition checkCondition, iocpCallback callback) {
	using namespace std::chrono;
	TimerEvent ev;
	ev.checkCondition = std::move(checkCondition);
	ev.callback = std::move(callback);
	ev.object = obj;
	ev.bufOver = Actor::Get(obj)->GetOver(threadIdx);
	ev.startTime = system_clock::now() + milliseconds(delayMs);
	Add(ev, threadIdx);
}

void TimerQueueManager::Post(int obj, int threadIdx, iocpCallback callback) {
	auto over = Actor::Get(obj)->GetOver(threadIdx);
	if(!callback){
		_ASSERT(false);
	}
	over->callback = callback;
	PostQueuedCompletionStatus(hIocp, 1, obj, &over->over);
}

void TimerQueueManager::Do() {
	using namespace std::chrono;
	for (;;) {
		for (size_t i = 0; i < TIMER_QUEUE_COUNT; i++) {
			auto now = system_clock::now();
			if(startTime[i] < now){
				auto& timerQueue = timerQueues[i];
				ignoreTimeLock.lock();
				timerQueue.timerLock.lock();
				if (timerQueue.empty() || now < timerQueue.top().startTime ||
					timerQueue.top().startTime < ignoreTime[timerQueue.top().object]) {
					timerQueue.timerLock.unlock();
					ignoreTimeLock.unlock();
					continue;
				}
				ignoreTimeLock.unlock();
				TimerEvent ev = timerQueue.top();
				timerQueue.pop();
				timerQueue.timerLock.unlock();
				auto actor = Actor::Get(ev.object);
				if (!actor->isActive ||
					(ev.checkCondition != nullptr && !ev.checkCondition())) {
					continue;
					}

				auto over = ev.bufOver;
				over->callback = ev.callback;
				if (!ev.callback) {
					_ASSERT(false);
				}
				PostQueuedCompletionStatus(hIocp, 1, ev.object, &over->over);
			}
		}
	}
}

void TimerQueueManager::SetIocpHandle(HANDLE hIocp) {
	TimerQueueManager::hIocp = hIocp;
}
