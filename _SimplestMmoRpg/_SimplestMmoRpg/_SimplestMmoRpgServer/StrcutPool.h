#pragma once
#include <mutex>
/// <summary>
/// [멀티스레드 전용]리스트로 풀링함, 앞에서 빼는 것과 뒤에서 넣는 것의 락을 따로 걸어 성능 향상을 노림
/// </summary>
/// <typeparam name="T"></typeparam>
template<class T>
class StructPool {
	struct Element {
		T element;
		Element* next;
	};
	Element* start;
	std::mutex startLock;
	Element* end;
	std::mutex endLock;
	std::atomic_int size = 2;
public:
	T* Get() {
		--size;
		std::lock_guard<std::mutex> lock(startLock);
		if (size <= 0) { // end 하나는 남겨두고 증가시킴
			CreatePoolObjects();
		}
		auto returnObj = start;
		start = start->next; // start가 null 되지않게 새로운 오브젝트를 계속 만들어주어야한다.
		return &returnObj->element;
	}

	void Recycle(T* obj) {
		Element* element = reinterpret_cast<Element*>(obj);
		++size;
		std::lock_guard<std::mutex> lock(endLock);
		element->next = nullptr;
		end->next = element;
		end = element;
	}

	StructPool() : start(nullptr) {
		auto newObj = new Element;
		end = newObj;
		end->next = nullptr;
		newObj = new Element;
		start = newObj;
		start->next = end;
	}
private:
	void CreatePoolObjects() { // startLock걸고 호출되어야하며, 원소가 무조건 한개 이상 있어야 함
		for (size_t i = 0; i < 1; i++) {
			auto newObj = new Element;
			newObj->next = start;
			start = newObj;
		}
		++size;
	}
};