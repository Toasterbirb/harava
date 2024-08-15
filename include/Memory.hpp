#pragma once

#include "Types.hpp"

#include <string>
#include <vector>

namespace harava
{
	struct memory_region
	{
		memory_region(const std::string& range_str);
		size_t start, end;
		u64 id;

		// if no results are from this region, ignore it
		bool ignore = false;
	};

	struct result
	{
		i32 value;
		size_t location;
		u64 region_id;
	};

	class memory
	{
	public:
		memory(const i32 pid);
		std::vector<result> search(const i32 value);
		std::vector<result> refine_search(const i32 new_value, const std::vector<result>& old_results);
		void set(result& result, const i32 new_value);

	private:
		i32 bytes_to_int(const u8* bytes, size_t location);
		memory_region& get_region(const u64 id);

		// read a range of bytes from a file
		std::vector<u8> read_region(const std::string& path, const size_t start, const size_t end);

		const i32 pid;
		const std::string proc_path;
		const std::string mem_path;

		std::vector<memory_region> regions;
	};
}
