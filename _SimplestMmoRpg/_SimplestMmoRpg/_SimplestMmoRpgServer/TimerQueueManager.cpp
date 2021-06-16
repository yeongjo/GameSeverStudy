#include "TimerQueueManager.h"

#include <utility>

#include "Actor.h"

HANDLE TimerQueueManager::hIocp;
std::array<TimerQueue, TIMER_QUEUE_COUNT> TimerQueueManager::timerQueues;
std::array<std::chrono::time_point<std::chrono::system_clock>, TIMER_QUEUE_COUNT> TimerQueueManager::startTime;

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

//void TimerQueueManager::RemoveAll(int playerId) {
//	for (size_t i = 0; i < TIMER_QUEUE_COUNT; i++) {
//		auto& timerQueue = timerQueues[i];
//		std::lock_guard<std::mutex> lock(timerQueue.timerLock);
//		timerQueue.remove_all(playerId);
//	}
//}

void TimerQueueManager::Add(int obj, int delayMs, int threadIdx, TimerEventCheckCondition checkCondition, iocpCallback callback) {
	using namespace std::chrono;
	TimerEvent ev;
	ev.checkCondition = std::move(checkCondition);
	ev.callback = std::move(callback);
	ev.object = obj;
	auto actor = Actor::Get(obj);
	ev.bufOver = actor->GetOver(threadIdx);
	ev.startTime = system_clock::now() + milliseconds(delayMs);
	ev.actorTimerId = actor->GetTimerId();
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
		auto now = system_clock::now();
		for (size_t i = 0; i < TIMER_QUEUE_COUNT; i++) {
			if (startTime[i] < now) {
				auto& timerQueue = timerQueues[i];
				timerQueue.timerLock.lock();
				if (timerQueue.empty() || now < timerQueue.top().startTime) {
					timerQueue.timerLock.unlock();
					std::this_thread::sleep_for(INVERSE_THREAD_TIME);
					continue;
				}
				TimerEvent ev = timerQueue.top();
				timerQueue.pop();
				timerQueue.timerLock.unlock();
				const auto actor = Actor::Get(ev.object);
				if (!actor->isActive||
					ev.actorTimerId < actor->GetTimerId()) {
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
