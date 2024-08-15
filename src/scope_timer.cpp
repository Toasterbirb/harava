#include "ScopeTimer.hpp"

#include <iostream>

namespace harava
{
	scope_timer::scope_timer(const std::string& message)
	:message(message)
	{
		start = std::chrono::steady_clock::now();
	}

	scope_timer::~scope_timer()
	{
		stop = std::chrono::steady_clock::now();
		std::cout << message << std::dec << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start) << '\n';
	}
}
