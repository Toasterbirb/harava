#include "Memory.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace harava
{
	memory_region::memory_region(const std::string& range_str)
	{
		size_t line_pos = range_str.find('-');

		std::stringstream hex_ss;
		hex_ss << std::hex << range_str.substr(0, line_pos) << " " << range_str.substr(line_pos + 1, range_str.size() - line_pos - 1);
		hex_ss >> start >> end;

		id = start ^ (end << 1);
	}

	memory::memory(const i32 pid)
	:pid(pid), proc_path("/proc/" + std::to_string(pid)), mem_path(proc_path + "/mem")
	{
		// Find suitable memory regions
		const std::string maps_path = proc_path + "/maps";
		std::ifstream maps(maps_path);

		std::string line;
		while (std::getline(maps, line))
		{
			std::string range, perms, offset, ids, inode_id, file_path;

			std::stringstream ss;
			ss << line;
			ss >> range >> perms >> offset >> ids >> inode_id >> file_path;

			// Skip memory regions that are not writable
			if (!perms.starts_with("rw"))
				continue;

			// // Skip memory regions that are for external libraries
			if (file_path.starts_with("/lib64") || file_path.starts_with("/usr/lib"))
				continue;

			regions.emplace_back(range);
		}

		if (regions.empty())
			throw "no suitable memory regions could be found";

		std::cout << "found " << regions.size() << " suitable regions\n";

		for (const memory_region region : regions)
			std::cout << std::hex << region.start << " -> " << region.end << '\n';
	}

	std::vector<result> memory::search(const i32 value)
	{
		std::vector<result> results;

		for (memory_region& region : regions)
		{
			std::vector<u8> bytes = read_region(mem_path, region.start, region.end);

			// go through the bytes one by one

			u64 region_result_count{};

			for (size_t i = 0; i < bytes.size() - sizeof(int); ++i)
			{
				const i32 cur_value = bytes_to_int(bytes.data(), i);

				if (cur_value == value)
				{
					result r;
					r.value = value;
					r.location = i;
					r.region_id = region.id;
					results.push_back(r);
					++region_result_count;
				}
			}

			if (region_result_count == 0)
				region.ignore = true;
		}

		return results;
	}

	std::vector<result> memory::refine_search(const i32 new_value, const std::vector<result>& old_results)
	{
		std::vector<result> new_results;

		std::fstream mem(mem_path, std::ios::in | std::ios::binary);
		if (!mem.is_open())
			throw "can't open " + mem_path;

		for (result result : old_results)
		{
			mem.seekg(result.location + get_region(result.region_id).start, std::ios::beg);

			i32 value{};
			mem.read((char*)&value, sizeof(int));

			if (value == new_value)
			{
				result.value = new_value;
				new_results.push_back(result);
			}
		}

		return new_results;
	}

	void memory::set(result& result, const i32 new_value)
	{
		result.value = new_value;

		std::fstream mem(mem_path, std::ios::out | std::ios::binary);
		if (!mem.is_open())
			throw "can't open " + mem_path;

		mem.seekg(result.location + get_region(result.region_id).start, std::ios::beg);
		mem.write((char*)&new_value, sizeof(int));
	}

	i32 memory::bytes_to_int(const u8* bytes, size_t location)
	{
		i32 value = 0;

		for (u8 i = 0; i < 4; ++i)
			value += bytes[location + i] << i * 8;

		return value;
	}

	memory_region& memory::get_region(const u64 id)
	{
		for (memory_region& region : regions)
			if (region.id == id)
				return region;

		throw "no region could be found with the given id";
	}

	std::vector<u8> memory::read_region(const std::string& path, const size_t start, const size_t end)
	{
		std::vector<u8> bytes;
		bytes.resize(end - start);

		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			std::cerr << "can't open " << mem_path << '\n';
			return {};
		}

		file.seekg(start, std::ios::beg);
		file.read((char*)&bytes.data()[0], bytes.size());

		return bytes;
	}
}
