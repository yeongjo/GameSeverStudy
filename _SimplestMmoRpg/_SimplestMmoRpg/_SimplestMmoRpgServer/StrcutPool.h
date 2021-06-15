#pragma once
/// <summary>
/// [멀티스레드 전용]리스트로 풀링함, 앞에서 빼는 것과 뒤에서 넣는 것의 락을 따로 걸어 성능 향상을 노림
/// </summary>
/// <typeparam name="T"></typeparam>
template<class T>
class StructPool {
	struct Element {
		T* element;
		T* next;
	};
	Element* start;
	mutex startLock;
	Element* end;
	mutex endLock;
	atomic_int size;
public:
	T* Get() {
		lock_guard<mutex> lock(startLock);
		auto returnObj = start;
		start = start->next; // start가 null 되지않게 새로운 오브젝트를 계속 만들어주어야한다.
		--size;
		if (size <= 1) { // end 하나는 남겨두고 증가시킴
			CreatePoolObjects();
		}
		return returnObj;
	}

	void Recycle(T* obj) {
		lock_guard<mutex> lock(endLock);
		end->next = obj;
		end = obj;
		end->next = nullptr;
		++size;
	}
protected:
	StructPool() : start(nullptr) {
		auto newObj = new Element;
		newObj->element = new T;
		end = newObj;
		end->next = nullptr;
		newObj = new Element;
		newObj->element = new T;
		start = newObj;
		start->next = end;
	}
private:
	void CreatePoolObjects() { // startLock걸고 호출되어야하며, 원소가 무조건 한개 이상 있어야 함
		for (size_t i = 0; i < 2; i++) {
			auto newObj = new Element;
			newObj->element = new T;
			newObj->next = start;
			start = newObj;
		}
	}
};