#pragma once

#include <chrono>
#include <string>

namespace harava
{
	class scope_timer
	{
	public:
		scope_timer(const std::string& message);
		~scope_timer();

	private:
		std::string message;
		std::chrono::time_point<std::chrono::steady_clock> start, stop;
	};
}
