#include "OverlappedStruct.h"

#include <crtdbg.h>

#include "BufOverManager.h"

void BufOver::Recycle() {
	_ASSERT(manager != nullptr && "manager가 null입니다");
	manager->Recycle(this);
}
