#pragma once
#include <mutex>
/// <summary>
/// [��Ƽ������ ����]����Ʈ�� Ǯ����, �տ��� ���� �Ͱ� �ڿ��� �ִ� ���� ���� ���� �ɾ� ���� ����� �븲
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
		if (size <= 0) { // end �ϳ��� ���ܵΰ� ������Ŵ
			CreatePoolObjects();
		}
		auto returnObj = start;
		start = start->next; // start�� null �����ʰ� ���ο� ������Ʈ�� ��� ������־���Ѵ�.
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
	void CreatePoolObjects() { // startLock�ɰ� ȣ��Ǿ���ϸ�, ���Ұ� ������ �Ѱ� �̻� �־�� ��
		for (size_t i = 0; i < 1; i++) {
			auto newObj = new Element;
			newObj->next = start;
			start = newObj;
		}
		++size;
	}
};