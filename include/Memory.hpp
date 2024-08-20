#pragma once

#include "Filter.hpp"
#include "Options.hpp"
#include "Types.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace harava
{
	struct memory_region
	{
		memory_region() = default;
		memory_region(const std::string& range_str);
		size_t start, end;
	};

	struct type_bundle
	{
		type_bundle(const std::string& value);
		i32 _int;
		i64 _long;
		const f32 _float;
		const f64 _double;
	};

	// the first 4 bits of the datatype indicate the type
	// 1. int
	// 2. long
	// 3. float
	// 4. double
	//
	// and the latter 4 bits indicate the size
	// (big endian)
	enum class datatype : u8
	{
		INT		= 0x04,
		LONG	= 0x18,
		FLOAT	= 0x24,
		DOUBLE	= 0x38
	};

	enum class comparison
	{
		eq, // equal
		lt, // less than
		gt, // greater than
		le, // less than or equal to
		ge  // greater than or equal to
	};

	__attribute__((hot))
	u8 datatype_to_size();

	union type_union
	{
		i32 _int;
		i64 _long;
		f32 _float;
		f64 _double;
		u8 bytes[8];
	};

	struct result
	{
		type_union value;
		u32 location;
		u16 region_id;
		datatype type;

		void print_info() const;

		__attribute__((hot))
		bool compare_bytes(const std::vector<u8>& bytes) const;
	};

	template<typename T>
	__attribute__((hot))
	inline constexpr bool cmp(const T a, const T b, const comparison comparison)
	{
		switch (comparison)
		{
			case comparison::eq:
				return a == b;

			case comparison::lt:
				return a > b;

			case comparison::le:
				return a >= b;

			case comparison::gt:
				return a < b;

			case comparison::ge:
				return a <= b;
		}

		return false;
	}

	class memory
	{
	public:
		memory(const i32 pid, const options opts);

		__attribute__((warn_unused_result))
		std::vector<result> search(const options opts, const filter filter, const type_bundle value, const comparison comparison);

		__attribute__((warn_unused_result))
		std::vector<result> refine_search(const type_bundle new_value, const std::vector<result>& old_results, const comparison comparison);

		__attribute__((warn_unused_result))
		std::vector<result> refine_search_change(const std::vector<result>& old_results, const bool expected_result);

		void set(result& result, const type_bundle value);
		u64 region_count() const;

		template<typename T>
		T get_result_value(const result result)
		{
			std::fstream mem(mem_path, std::ios::in | std::ios::binary);
			if (!mem.is_open())
			{
				std::cout << "can't open " << mem_path;
				exit(1);
			}

			mem.seekg(result.location + regions.at(result.region_id).start, std::ios::beg);

			T value;
			mem.read((char*)&value, sizeof(T));

			return value;
		}

	private:
		static constexpr u8 max_type_size = 8;

		template<typename T>
		union type_as_bytes
		{
			T type;
			u8 bytes[max_type_size] = { 0, 0, 0, 0, 0, 0, 0, 0};
		};

		// read a range of bytes from a file
		std::vector<u8> read_region(std::ifstream& file, const size_t start, const size_t end);

		struct region_snapshot
		{
			memory_region* region;
			std::vector<u8> bytes;
		};

		std::unordered_map<u16, region_snapshot> snapshot_regions(const std::vector<result>& results);
		void trim_region_range(const result result);

		const i32 pid;
		const std::string proc_path;
		const std::string mem_path;

		std::map<u16, memory_region> regions;
	};
}
