#pragma once
#include <cstdint>

constexpr int32_t ceil_const(float num) {
	return (static_cast<float>(static_cast<int32_t>(num)) == num)
		? static_cast<int32_t>(num)
		: static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
}
