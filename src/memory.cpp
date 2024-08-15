#include "Memory.hpp"

#include <cassert>
#include <cmath>
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

	type_bundle::type_bundle(const std::string& value)
	:_int(std::stoi(value)), _long(std::stol(value)), _float(std::stof(value)), _double(std::stold(value))
	{}

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

	std::vector<result> memory::search(const type_bundle value)
	{
		std::vector<result> results;

		for (memory_region& region : regions)
		{
			std::vector<u8> bytes = read_region(mem_path, region.start, region.end);

			// go through the bytes one by one

			u64 region_result_count{};

			for (size_t i = 0; i < bytes.size() - sizeof(double); ++i)
			{
				const auto handle_result = [&](auto a, auto b, datatype type)
				{
					if (a != b)
						return;

					result r;

					if (type == datatype::INT || type == datatype::FLOAT)
						r.value = { bytes.at(i), bytes.at(i + 1), bytes.at(i + 2), bytes.at(i + 3) };
					else if (type == datatype::LONG || type == datatype::DOUBLE)
						r.value = { bytes.at(i), bytes.at(i + 1), bytes.at(i + 2), bytes.at(i + 3),
								bytes.at(i + 4), bytes.at(i + 5), bytes.at(i + 6), bytes.at(i + 7)};
					else
						throw "unimplemented type";

					r.location = i;
					r.region_id = region.id;
					r.type = type;
					results.push_back(r);
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
		}

		return results;
	}

	std::vector<result> memory::refine_search(const type_bundle new_value, const std::vector<result>& old_results)
	{
		std::vector<result> new_results;

		std::fstream mem(mem_path, std::ios::in | std::ios::binary);
		if (!mem.is_open())
			throw "can't open " + mem_path;

		for (result result : old_results)
		{
			mem.seekg(result.location + get_region(result.region_id).start, std::ios::beg);

			const auto check_value = [&mem, &new_results, &result]<typename T>(const T new_value)
			{
				T value{};
				mem.read((char*)&value, sizeof(T));

				if (new_value == value)
					new_results.push_back(result);
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

	void memory::set(result& result, const type_bundle value)
	{
		// result.value = new_value;

		std::fstream mem(mem_path, std::ios::out | std::ios::binary);
		if (!mem.is_open())
			throw "can't open " + mem_path;

		mem.seekg(result.location + get_region(result.region_id).start, std::ios::beg);

		switch (result.type)
		{
			case datatype::INT:
				mem.write((char*)&value._int, sizeof(i32));
				break;

			case datatype::LONG:
				mem.write((char*)&value._long, sizeof(i64));
				break;

			case datatype::FLOAT:
				mem.write((char*)&value._float, sizeof(f32));
				break;

			case datatype::DOUBLE:
				mem.write((char*)&value._double, sizeof(f64));
				break;
		}
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
