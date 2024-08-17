#pragma once

#include "Types.hpp"

namespace harava
{
	struct options
	{
		i32 pid{};
		u64 memory_limit = 8; // limit in gigabytes
		bool skip_volatile = false;
		bool skip_zeroes = false;
		bool skip_null_maps = false;
	};
}
