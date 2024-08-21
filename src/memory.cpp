#include "Memory.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <execution>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>

constexpr u64 gigabyte = 1'000'000'000;

namespace harava
{
	static u16 memory_region_count = 0;

	memory_region::memory_region(const std::string& range_str)
	{
		size_t line_pos = range_str.find('-');

		std::stringstream hex_ss;
		hex_ss << std::hex << range_str.substr(0, line_pos) << " " << range_str.substr(line_pos + 1, range_str.size() - line_pos - 1);
		hex_ss >> start >> end;
	}

	type_bundle::type_bundle(const std::string& value)
	:_float(std::stof(value)), _double(std::stold(value))
	{
		try
		{
			_int = std::stoi(value);
		}
		catch (const std::exception& e)
		{
			_int = 2'147'483'646;
		}

		try
		{
			_long = std::stol(value);
		}
		catch (const std::exception& e)
		{
			_long = 9223372036854775806;
		}
	}

	void result::print_info() const
	{
		std::cout << std::right << std::hex << std::setw(5) << location << " | ";

		switch(type)
		{
			case datatype::INT:
				std::cout << "i32";
				break;

			case datatype::LONG:
				std::cout << "i64";
				break;

			case datatype::FLOAT:
				std::cout << "f32";
				break;

			case datatype::DOUBLE:
				std::cout << "f64";
				break;
		}
	}

	bool result::compare_bytes(const std::vector<u8>& bytes) const
	{
		const u8 type_size = static_cast<u8>(type) & 0x0F;

		bool match = true;
		for (u8 i = 0; i < type_size; ++i)
		{
			if (value.bytes[i] != bytes[i + location])
			{
				match = false;
				break;
			}
		}

		return match;
	}

	u64 results::total_size() const
	{
		return int_results.size() * sizeof(i32)
			+ long_results.size() * sizeof(i64)
			+ float_results.size() * sizeof(f32)
			+ double_results.size() * sizeof(f64);
	}

	u64 results::count() const
	{
		return int_results.size()
			+ long_results.size()
			+ float_results.size()
			+ double_results.size();
	}

	result& results::at(const u64 index)
	{
		auto vecs = result_vecs();

		u64 total_elements{0};
		std::vector<result>* target_vec = nullptr;

		for (auto& [index, vec] : vecs)
		{
			if (total_elements + vec->size() > index)
			{
				target_vec = vec;
				break;
			}

			total_elements += vec->size();
		}

		assert(target_vec);
		return target_vec->at(index - total_elements);
	}

	void results::clear()
	{
		int_results.clear();
		long_results.clear();
		float_results.clear();
		double_results.clear();
	}

	std::array<std::pair<u8, std::vector<result>*>, 4> results::result_vecs()
	{
		return {
			std::make_pair( 0, &int_results ),
			{ 1, &long_results },
			{ 2, &float_results },
			{ 3, &double_results }
		};
	}

	memory::memory(const i32 pid, const options opts)
	:pid(pid), proc_path("/proc/" + std::to_string(pid)), mem_path(proc_path + "/mem")
	{
		// Find suitable memory regions
		const std::string maps_path = proc_path + "/maps";
		std::ifstream maps(maps_path);

		if (!maps.is_open())
		{
			std::cout << "can't open " << maps_path << '\n';
			exit(1);
		}

		const std::regex lib_regex("^.*\\.so$");
		const std::regex lib_versioned_regex("^.*\\.so\\.[.0-9]*$");

		std::string line;
		while (std::getline(maps, line))
		{
			std::string range, perms, offset, ids, inode_id, file_path;

			std::stringstream ss;
			ss << line;
			ss >> range >> perms >> offset >> ids >> inode_id >> file_path;

			if (opts.stack_scan && file_path != "[stack]")
				continue;

			// Skip memory regions that are not writable
			if (!perms.starts_with("rw"))
				continue;

			// Skip memory regions that are for external libraries
			if (file_path.starts_with("/lib")
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

			this->regions[memory_region_count] = memory_region( range );
			++memory_region_count;
		}

		if (regions.empty())
		{
			std::cout << "no suitable memory regions could be found\n";
			exit(1);
		}

		std::cout << "found " << regions.size() << " suitable regions\n";
	}

	results memory::search(const options opts, const filter filter, const type_bundle value, const comparison comparison)
	{
		results aggragate_results;
		std::mutex result_mutex;
		// results.reserve(200'000);
		bool cancel_search = false;

		using namespace std::chrono_literals;

		std::for_each(std::execution::par_unseq, regions.begin(), regions.end(), [&](auto&& memory_region)
		{
			if (cancel_search)
				return;

			const auto& [region_id, region] = memory_region;

			std::ifstream mem(mem_path, std::ios::in | std::ios::binary);
			std::vector<u8> bytes = read_region(mem, region.start, region.end);

			if (opts.skip_null_regions && std::all_of(std::execution::par_unseq, bytes.begin(), bytes.end(), [](const u8 byte) { return byte == 0; }))
			{
				std::cout << '0' << std::flush;
				return;
			}

			// go through the bytes one by one

			results region_result;

			for (size_t i = 0; i < bytes.size() - sizeof(double) && !cancel_search; ++i)
			{
				type_union res_value;
				memcpy(res_value.bytes, &bytes[i], sizeof(f64));

				if (opts.skip_zeroes && res_value._long == 0)
					continue;

				result r;
				r.value._long = res_value._long; // copy 8 bytes
				r.location = i;
				r.region_id = region_id;

				if (filter.enable_i32 && cmp<i32>(value._int, res_value._int, comparison))
				{
					r.type = datatype::INT;
					region_result.int_results.emplace_back(r);
				}

				if (filter.enable_f32 && cmp<f32>(value._float, res_value._float, comparison))
				{
					r.type = datatype::FLOAT;
					region_result.float_results.emplace_back(r);
				}

				if (filter.enable_i64 && cmp<i64>(value._long, res_value._long, comparison))
				{
					r.type = datatype::LONG;
					region_result.long_results.emplace_back(r);
				}

				if (filter.enable_f64 && cmp<f64>(value._double, res_value._double, comparison))
				{
					r.type = datatype::DOUBLE;
					region_result.double_results.emplace_back(r);
				}

				if (!cancel_search && aggragate_results.total_size() > opts.memory_limit * gigabyte)
				{
					std::scoped_lock mem_lock;
					std::cout << "\nmemory limit of " << opts.memory_limit << "GB has been reached\n"
						<< "stopping the search\n";
					cancel_search = true;
				}
			}

			std::lock_guard<std::mutex> guard(result_mutex);
			aggragate_results.int_results.insert(aggragate_results.int_results.end(),
					region_result.int_results.begin(), region_result.int_results.end());

			aggragate_results.long_results.insert(aggragate_results.long_results.end(),
					region_result.long_results.begin(), region_result.long_results.end());

			aggragate_results.float_results.insert(aggragate_results.float_results.end(),
					region_result.float_results.begin(), region_result.float_results.end());

			aggragate_results.double_results.insert(aggragate_results.double_results.end(),
					region_result.double_results.begin(), region_result.double_results.end());

			std::cout << '.' << std::flush;
		});

		std::cout << '\n';

		return aggragate_results;
	}

	results memory::refine_search(const type_bundle new_value, results& old_results, const comparison comparison)
	{
		results new_results;
		std::unordered_map<u16, region_snapshot> region_cache = snapshot_regions(old_results);

		std::cout << "processing bytes" << std::endl;

		std::future<void> int_res_future = std::async(std::launch::async, [&]()
		{
			for (result result : old_results.int_results)
			{
				type_as_bytes<i32> v;
				memcpy(v.bytes, &region_cache.at(result.region_id).bytes[result.location], sizeof(i32));
				if (cmp<i32>(new_value._int, v.type, comparison))
				{
					memcpy(result.value.bytes, &region_cache.at(result.region_id).bytes[result.location], 0x0F & static_cast<u8>(result.type));
					new_results.int_results.emplace_back(result);
				}
			}
		});

		std::future<void> long_res_future = std::async(std::launch::async, [&]()
		{
			for (result result : old_results.long_results)
			{
				type_as_bytes<i64> v;
				memcpy(v.bytes, &region_cache.at(result.region_id).bytes[result.location], sizeof(i64));
				if (cmp<i64>(new_value._long, v.type, comparison))
				{
					memcpy(result.value.bytes, &region_cache.at(result.region_id).bytes[result.location], 0x0F & static_cast<u8>(result.type));
					new_results.long_results.emplace_back(result);
				}
			}
		});

		std::future<void> float_res_future = std::async(std::launch::async, [&]()
		{
			for (result result : old_results.float_results)
			{
				type_as_bytes<f32> v;
				memcpy(v.bytes, &region_cache.at(result.region_id).bytes[result.location], sizeof(f32));
				if (cmp<f32>(new_value._float, v.type, comparison))
				{
					memcpy(result.value.bytes, &region_cache.at(result.region_id).bytes[result.location], 0x0F & static_cast<u8>(result.type));
					new_results.float_results.emplace_back(result);
				}
			}
		});

		std::future<void> double_res_future = std::async(std::launch::async, [&]()
		{
			for (result result : old_results.double_results)
			{
				type_as_bytes<f64> v;
				memcpy(v.bytes, &region_cache.at(result.region_id).bytes[result.location], sizeof(f64));
				if (cmp<f64>(new_value._double, v.type, comparison))
				{
					memcpy(result.value.bytes, &region_cache.at(result.region_id).bytes[result.location], 0x0F & static_cast<u8>(result.type));
					new_results.double_results.emplace_back(result);
				}
			}
		});

		int_res_future.wait();
		long_res_future.wait();
		float_res_future.wait();
		double_res_future.wait();

		return new_results;
	}

	results memory::refine_search_change(results& old_results, const bool expected_result)
	{
		// expected_result == true (value unchanged)
		// expected_result == false (value changed)

		std::unordered_map<u16, region_snapshot> region_cache = snapshot_regions(old_results);
		results new_results;

		const auto old_res_vec_ptrs = old_results.result_vecs();
		auto new_res_vec_ptrs = new_results.result_vecs();

		std::for_each(std::execution::par_unseq, old_res_vec_ptrs.begin(), old_res_vec_ptrs.end(),
			[&](const std::pair<u8, std::vector<result>*> res_vec)
			{
				const auto& [vec_index, vec] = res_vec;
				for (result r : *vec)
				{
					if (r.compare_bytes(region_cache.at(r.region_id).bytes) == expected_result)
						new_res_vec_ptrs.at(vec_index).second->emplace_back(r);
				}
			});

		return new_results;
	}

	void memory::set(result& result, const type_bundle value)
	{
		// result.value = new_value;

		std::fstream mem(mem_path, std::ios::out | std::ios::binary);
		if (!mem.is_open()) [[unlikely]]
		{
			std::cout << "can't open " << mem_path << '\n';
			return;
		}

		mem.seekg(result.location + regions.at(result.region_id).start, std::ios::beg);

		switch (result.type)
		{
			case datatype::INT:
			{
				mem.write((char*)&value._int, sizeof(i32));
				result.value._int = value._int;
				break;
			}

			case datatype::LONG:
			{
				mem.write((char*)&value._long, sizeof(i64));
				result.value._long = value._long;
				break;
			}

			case datatype::FLOAT:
			{
				mem.write((char*)&value._float, sizeof(f32));
				result.value._float = value._float;
				break;
			}

			case datatype::DOUBLE:
			{
				mem.write((char*)&value._double, sizeof(f64));
				result.value._double = value._double;
				break;
			}
		}
	}

	u64 memory::region_count() const
	{
		return regions.size();
	}

	std::vector<u8> memory::read_region(std::ifstream& file, const size_t start, const size_t end)
	{
		std::vector<u8> bytes;
		bytes.resize(end - start);

		file.seekg(start, std::ios::beg);
		file.read((char*)&bytes.data()[0], bytes.size());

		return bytes;
	}

	std::unordered_map<u16, memory::region_snapshot> memory::snapshot_regions(results& results)
	{
		std::unordered_map<u16, region_snapshot> region_cache;

		std::cout << "taking a memory snapshot\n" << std::flush;
		{
			std::mutex cache_mutex;

			const auto result_vecs = results.result_vecs();
			for (const auto& [index, vec_ptr] : result_vecs)
			{
				std::for_each(std::execution::par_unseq, vec_ptr->begin(), vec_ptr->end(),
					[&](result result)
					{
						if (region_cache.contains(result.region_id)) [[likely]]
							return;

						std::ifstream mem(mem_path, std::ios::in | std::ios::binary);
						if (!mem.is_open()) [[unlikely]]
						{
							std::cout << "can't open " << mem_path << '\n';
							exit(1);
						}

						memory_region* region = &regions.at(result.region_id);

						region_snapshot snapshot;
						snapshot.bytes = read_region(mem, region->start, region->end);
						snapshot.region = region;

						std::lock_guard<std::mutex> lock(cache_mutex);
						region_cache[result.region_id] = snapshot;
						std::cout << '.' << std::flush;
					});
			}
			std::cout << '\n';
		}

		return region_cache;
	}

	void memory::trim_region_range(const result result)
	{
		// update the end point of the region so that during the next
		// snapshot less bytes can be read from the memory file
		regions.at(result.region_id).end = regions.at(result.region_id).start + result.location + sizeof(f64);
	}
}
