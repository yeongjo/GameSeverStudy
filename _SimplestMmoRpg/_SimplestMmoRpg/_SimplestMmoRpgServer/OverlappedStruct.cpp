#include "OverlappedStruct.h"

#include <crtdbg.h>

#include "BufOverManager.h"

void BufOver::Recycle() {
	_ASSERT(manager != nullptr && "manager�� null�Դϴ�");
	manager->Recycle(this);
}
