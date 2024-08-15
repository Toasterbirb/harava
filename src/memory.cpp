#include "Memory.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace harava
{
	const static auto comparison_func_equal = [](const auto a, const auto b) -> bool
	{
		return a == b;
	};

	const static auto comparison_func_less_than = [](const auto a, const auto b) -> bool
	{
		return a > b;
	};

	const static auto comparison_func_more_than = [](const auto a, const auto b) -> bool
	{
		return a < b;
	};

	static u16 memory_region_count = 0;

	memory_region::memory_region(const std::string& range_str)
	{
		size_t line_pos = range_str.find('-');

		std::stringstream hex_ss;
		hex_ss << std::hex << range_str.substr(0, line_pos) << " " << range_str.substr(line_pos + 1, range_str.size() - line_pos - 1);
		hex_ss >> start >> end;

		id = memory_region_count++;
	}

	type_bundle::type_bundle(const std::string& value)
	:_int(std::stoi(value)), _long(std::stol(value)), _float(std::stof(value)), _double(std::stold(value))
	{}

	u8 datatype_to_size(const datatype type)
	{
		return type == datatype::INT || type == datatype::FLOAT
			? 4
			: 8;
	}

	void result::print_info() const
	{
		std::cout << std::left << std::hex << location << " | ";

		switch(type)
		{
			case datatype::INT:
				std::cout << "int";
				break;

			case datatype::LONG:
				std::cout << "long";
				break;

			case datatype::FLOAT:
				std::cout << "float";
				break;

			case datatype::DOUBLE:
				std::cout << "double";
				break;
		}
	}

	bool result::compare_bytes(const std::vector<u8>& bytes) const
	{
		const u8 type_size = datatype_to_size(type);

		bool match = true;
		for (u8 i = 0; i < type_size; ++i)
		{
			if (value[i] != bytes[i + location])
			{
				match = false;
				break;
			}
		}

		return match;
	}

	memory::memory(const i32 pid)
	:pid(pid), proc_path("/proc/" + std::to_string(pid)), mem_path(proc_path + "/mem")
	{
		// Find suitable memory regions
		const std::string maps_path = proc_path + "/maps";
		std::ifstream maps(maps_path);

		const std::regex lib_regex("^.*\\.so$");
		const std::regex lib_versioned_regex("^.*\\.so\\.[.0-9]*$");

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

			// Skip memory regions that are for external libraries
			if (file_path.starts_with("/lib64")
				|| file_path.starts_with("/usr/lib")
				|| file_path.starts_with("/dev")
				|| file_path.starts_with("/memfd")
				// need to use the full line for some things due to whitespace
				|| line.ends_with(".dll")
				|| line.ends_with("wine64")
				|| line.ends_with("wine64-preloader")
				|| line.ends_with(".drv"))
				continue;

			// Skip library files
			if (std::regex_match(file_path, lib_regex) || std::regex_match(file_path, lib_versioned_regex))
				continue;

			regions.emplace_back(range);
		}

		if (regions.empty())
			throw "no suitable memory regions could be found";

		std::cout << "found " << regions.size() << " suitable regions\n";
	}

	std::vector<result> memory::search(const type_bundle value, const char comparison)
	{
		std::vector<result> results;
		results.reserve(100'000);

		std::ifstream mem(mem_path, std::ios::in | std::ios::binary);

		for (memory_region& region : regions)
		{
			std::vector<u8> bytes = read_region(mem, region.start, region.end);

			// go through the bytes one by one

			u64 region_result_count{};

			for (size_t i = 0; i < bytes.size() - sizeof(double); ++i)
			{
				const auto handle_result = [&](auto a, auto b, datatype type)
				{
					bool comparison_result = false;
					const auto comp = [&](auto f)
					{
						comparison_result = f(a, b);
					};

					// ignore zero when doing less than or greater than comparisons
					// to avoid running out of memory
					//
					// though you'll still probably run out of memory if doing a direct comparison
					// with values that are zero, but probably shouldn't make it impossible to find all
					// values that are zero
					//
					// with the less than / more than comparisons though ignoring zeroes is kind of
					// necessary or else they are unusable even with 32 gigabytes of memory and a bunch
					// of swap
					if (b == 0 && comparison != '=')
						return;

					switch (comparison)
					{
						case '=':
							comp(comparison_func_equal);
							break;

						case '<':
							comp(comparison_func_less_than);
							break;

						case '>':
							comp(comparison_func_more_than);
							break;

						default:
							std::cout << "invalid comparison\n";
							return;
					}

					if (!comparison_result)
						return;

					result r;

					const u8 byte_count = datatype_to_size(type);

					for (u8 j = 0; j < byte_count; ++j)
						r.value[j] = bytes.at(i + j);

					r.location = i;
					r.region_id = region.id;
					r.type = type;
					results.emplace_back(r);
					++region_result_count;
				};

				const i32 cur_value_int = interpret_bytes<i32>(bytes.data(), i, sizeof(i32));
				const i64 cur_value_long = interpret_bytes<i64>(bytes.data(), i, sizeof(i64));
				const f32 cur_value_float = interpret_bytes<f32>(bytes.data(), i, sizeof(f32));
				const f64 cur_value_double = interpret_bytes<f64>(bytes.data(), i, sizeof(f64));

				handle_result(value._int, cur_value_int, datatype::INT);
				handle_result(value._long, cur_value_long, datatype::LONG);
				handle_result(value._float, cur_value_float, datatype::FLOAT);
				handle_result(value._double, cur_value_double, datatype::DOUBLE);
			}

			if (region_result_count == 0)
				region.ignore = true;

			std::cout << '.' << std::flush;
		}
		std::cout << '\n';

		return results;
	}

	std::vector<result> memory::refine_search(const type_bundle new_value, const std::vector<result>& old_results, const char comparison)
	{
		std::vector<result> new_results;
		new_results.reserve(old_results.size() / 4);

		std::unordered_map<u16, region_snapshot> region_cache = snapshot_regions(old_results);

		std::cout << "processing bytes" << std::endl;
		for (result result : old_results)
		{
			const region_snapshot& snapshot = region_cache.at(result.region_id);

			const auto check_value = [&]<typename T>(const T new_value)
			{
				const u64 offset = result.location;

				type_as_bytes<T> v;
				for (u8 i = 0; i < sizeof(T); ++i)
					v.bytes[i] = snapshot.bytes[i + offset];

				memcpy(result.value, v.bytes, max_type_size);

				const auto comp = [&](auto f)
				{
					if (f(new_value, v.type))
						new_results.push_back(result);
				};

				switch (comparison)
				{
					case '=':
						comp(comparison_func_equal);
						break;

					case '<':
						comp(comparison_func_less_than);
						break;

					case '>':
						comp(comparison_func_more_than);
						break;

					default:
						std::cout << "invalid comparison\n";
						return;
				}
			};

			switch (result.type)
			{
				case datatype::INT:
					check_value(new_value._int);
					break;

				case datatype::LONG:
					check_value(new_value._long);
					break;

				case datatype::FLOAT:
					check_value(new_value._float);
					break;

				case datatype::DOUBLE:
					check_value(new_value._double);
					break;
			}
		}

		return new_results;
	}

	std::vector<result> memory::refine_search_changed(const std::vector<result>& old_results)
	{
		std::unordered_map<u16, region_snapshot> region_cache = snapshot_regions(old_results);
		std::vector<result> new_results;

		for (result result : old_results)
		{
			const region_snapshot& snapshot = region_cache.at(result.region_id);
			if (!result.compare_bytes(snapshot.bytes))
				new_results.push_back(result);
		}

		return new_results;
	}

	std::vector<result> memory::refine_search_unchanced(const std::vector<result>& old_results)
	{
		std::unordered_map<u16, region_snapshot> region_cache = snapshot_regions(old_results);
		std::vector<result> new_results;

		for (result result : old_results)
		{
			const region_snapshot& snapshot = region_cache.at(result.region_id);
			if (result.compare_bytes(snapshot.bytes))
				new_results.push_back(result);
		}

		return new_results;
	}

	void memory::set(result& result, const type_bundle value)
	{
		// result.value = new_value;

		std::fstream mem(mem_path, std::ios::out | std::ios::binary);
		if (!mem.is_open()) [[unlikely]]
			throw "can't open " + mem_path;

		mem.seekg(result.location + get_region(result.region_id).start, std::ios::beg);

		switch (result.type)
		{
			case datatype::INT:
			{
				mem.write((char*)&value._int, sizeof(i32));

				type_as_bytes<i32> v;
				v.type = value._int;
				memcpy(result.value, v.bytes, max_type_size);
				break;
			}

			case datatype::LONG:
			{
				mem.write((char*)&value._long, sizeof(i64));

				type_as_bytes<i64> v;
				v.type = value._int;
				memcpy(result.value, v.bytes, max_type_size);
				break;
			}

			case datatype::FLOAT:
			{
				mem.write((char*)&value._float, sizeof(f32));

				type_as_bytes<f32> v;
				v.type = value._int;
				memcpy(result.value, v.bytes, max_type_size);
				break;
			}

			case datatype::DOUBLE:
			{
				mem.write((char*)&value._double, sizeof(f64));

				type_as_bytes<f64> v;
				v.type = value._int;
				memcpy(result.value, v.bytes, max_type_size);
				break;
			}
		}
	}

	u64 memory::region_count() const
	{
		return regions.size();
	}

	memory_region& memory::get_region(const u16 id)
	{
		for (memory_region& region : regions)
			if (region.id == id)
				return region;

		throw "no region could be found with the given id";
	}

	std::vector<u8> memory::read_region(std::ifstream& file, const size_t start, const size_t end)
	{
		std::vector<u8> bytes;
		bytes.resize(end - start);

		file.seekg(start, std::ios::beg);
		file.read((char*)&bytes.data()[0], bytes.size());

		return bytes;
	}

	std::unordered_map<u16, memory::region_snapshot> memory::snapshot_regions(const std::vector<result>& results)
	{
		std::unordered_map<u16, region_snapshot> region_cache;

		std::cout << "taking a memory snapshot\n" << std::flush;
		{
			std::ifstream mem(mem_path, std::ios::in | std::ios::binary);
			if (!mem.is_open()) [[unlikely]]
				throw "can't open " + mem_path;

			for (result result : results)
			{
				if (region_cache.contains(result.region_id)) [[likely]]
					continue;

				memory_region* region = &get_region(result.region_id);

				region_snapshot snapshot;
				snapshot.bytes = read_region(mem, region->start, region->end);
				snapshot.region = region;

				region_cache[result.region_id] = snapshot;
				std::cout << '.' << std::flush;
			}
			std::cout << '\n';
		}

		return region_cache;
	}
}
