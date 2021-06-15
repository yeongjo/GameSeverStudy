#include "TimerQueueManager.h"

#include "Actor.h"

HANDLE TimerQueueManager::hIocp;
TimerQueue TimerQueueManager::timerQueue;
std::mutex TimerQueueManager::timerLock;

void TimerQueue::remove_all(const int playerId) {
	auto size = this->c.size();
	auto object = playerId;
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

void TimerQueueManager::Add(TimerEvent event) {
	std::lock_guard<std::mutex> lock(timerLock);
	timerQueue.push(event);
}

void TimerQueueManager::RemoveAll(int playerId) {
	std::lock_guard<std::mutex> lock(timerLock);
	timerQueue.remove_all(playerId);
}

void TimerQueueManager::Add(int obj, int delayMs, TimerEventCheckCondition checkCondition, iocpCallback callback,
                            const char* buffer, int targetId) {
	using namespace std::chrono;
	TimerEvent ev;
	ev.checkCondition = checkCondition;
	ev.callback = callback;
	ev.object = obj;
	ev.startTime = system_clock::now() + milliseconds(delayMs);
	ev.targetId = targetId;
	if (nullptr != buffer){
		memcpy(ev.buffer, buffer, sizeof(char) * (strlen(buffer) + 1));
		ev.hasBuffer = true;
	}
	else{
		ev.hasBuffer = false;
	}
	Add(ev);
}

void TimerQueueManager::Do() {
	using namespace std::chrono;
	for (;;) {
		timerLock.lock();
		if (false == timerQueue.empty() && timerQueue.top().startTime < system_clock::now()) {
			TimerEvent ev = timerQueue.top();
			timerQueue.pop();
			timerLock.unlock();
			auto actor = Actor::Get(ev.object);
			if (!actor->IsActive() ||
				(ev.checkCondition != nullptr && !ev.checkCondition())) {
				continue;
			}

			auto over = actor->GetOver();
			over->callback = ev.callback;
			PostQueuedCompletionStatus(hIocp, 1, ev.object, &over->over);
		} else {
			timerLock.unlock();
			std::this_thread::sleep_for(10ms);
		}
	}
}

void TimerQueueManager::SetIocpHandle(HANDLE hIocp) {
	TimerQueueManager::hIocp = hIocp;
}
