#include "OverlappedStruct.h"

#include <crtdbg.h>

#include "BufOverManager.h"

void BufOver::Recycle(int threadIdx) {
	_ASSERT(manager != nullptr && "manager가 null입니다");
	manager->Recycle(this, threadIdx);
}

void BufOver::SetManager(BufOverManager* manager) {
	this->manager = manager;
}
