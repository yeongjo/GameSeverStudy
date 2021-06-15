#pragma once
/// <summary>
/// [��Ƽ������ ����]����Ʈ�� Ǯ����, �տ��� ���� �Ͱ� �ڿ��� �ִ� ���� ���� ���� �ɾ� ���� ����� �븲
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
		start = start->next; // start�� null �����ʰ� ���ο� ������Ʈ�� ��� ������־���Ѵ�.
		--size;
		if (size <= 1) { // end �ϳ��� ���ܵΰ� ������Ŵ
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
	void CreatePoolObjects() { // startLock�ɰ� ȣ��Ǿ���ϸ�, ���Ұ� ������ �Ѱ� �̻� �־�� ��
		for (size_t i = 0; i < 2; i++) {
			auto newObj = new Element;
			newObj->element = new T;
			newObj->next = start;
			start = newObj;
		}
	}
};