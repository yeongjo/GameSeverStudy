#include "TimerQueueManager.h"

#include "Actor.h"

HANDLE TimerQueueManager::hIocp;
std::array<TimerQueue, TIMER_QUEUE_COUNT> TimerQueueManager::timerQueues;
std::atomic_int TimerQueueManager::timerQueueIdx = 0;

void TimerQueue::remove_all(const int playerId) {
	auto size = this->c.size();
	auto object = playerId;
	for (size_t i = 0; i < size;){
		if (this->c[i].object == object){
			auto begin = this->c.begin();
			this->c.erase(begin + i);
			--size;
			this->size = size;
			continue;
		}
		++i;
	}
}

void TimerQueueManager::Add(TimerEvent& event) {
	if(event.checkCondition == nullptr || !event.checkCondition()){
		// 처음 이벤트 호출하는거라 checkCondition이 false면 넣는다
		auto& timerQueue = timerQueues[timerQueueIdx];
		std::lock_guard<std::mutex> lock(timerQueue.GetMutex());
		timerQueue.push(event);
		NextIndex();
	}
}

void TimerQueueManager::RemoveAll(int playerId) {
	auto& timerQueue = timerQueues[timerQueueIdx];
	std::lock_guard<std::mutex> lock(timerQueue.GetMutex());
	timerQueue.remove_all(playerId);
	NextIndex();
}

void TimerQueueManager::Add(int obj, int delayMs, TimerEventCheckCondition checkCondition, iocpCallback callback) {
	using namespace std::chrono;
	TimerEvent ev;
	ev.checkCondition = checkCondition;
	ev.callback = callback;
	ev.object = obj;
	ev.startTime = system_clock::now() + milliseconds(delayMs);
	Add(ev);
}

void TimerQueueManager::Do() {
	using namespace std::chrono;
	for (;;) {
		auto now = system_clock::now();
		for (size_t i = 0; i < TIMER_QUEUE_COUNT; i++) {
			auto& timerQueue = timerQueues[i];
			auto isEmpty = timerQueue.empty();
			if(isEmpty){
				continue;
			}
			timerQueue.GetMutex().lock();
			if (now < timerQueue.top().startTime) {
				timerQueue.GetMutex().unlock();
				continue;
			}
			TimerEvent ev = timerQueue.top();
			timerQueue.pop();
			timerQueue.GetMutex().unlock();
			auto actor = Actor::Get(ev.object);
			if (!actor->IsActive() ||
				(ev.checkCondition != nullptr && !ev.checkCondition())) {
				continue;
			}

			auto over = actor->GetOver();
			over->callback = ev.callback;
			PostQueuedCompletionStatus(hIocp, 1, ev.object, &over->over);
		}
		std::this_thread::sleep_for(10ms);
	}
}

void TimerQueueManager::SetIocpHandle(HANDLE hIocp) {
	TimerQueueManager::hIocp = hIocp;
}

void TimerQueueManager::NextIndex() {
	timerQueueIdx = (1 + timerQueueIdx) % TIMER_QUEUE_COUNT;
}
