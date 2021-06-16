#include "OverlappedStruct.h"

#include <crtdbg.h>

#include "BufOverManager.h"

void BufOver::Recycle(int threadIdx) {
	_ASSERT(manager != nullptr && "manager�� null�Դϴ�");
	manager->Recycle(this, threadIdx);
}

void BufOver::SetManager(BufOverManager* manager) {
	this->manager = manager;
}
