#pragma once

#include "Types.hpp"

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace harava
{
	struct memory_region
	{
		memory_region(const std::string& range_str, const bool is_stack);
		size_t start, end;
		u16 id;
		bool is_stack = false;

		// if no results are from this region, ignore it
		bool ignore = false;
	};

	struct type_bundle
	{
		type_bundle(const std::string& value);
		const i32 _int;
		const i64 _long;
		const f32 _float;
		const f64 _double;
	};

	enum class datatype : u8
	{
		INT		= 0,
		LONG	= 1,
		FLOAT	= 2,
		DOUBLE	= 3
	};

	u8 datatype_to_size();


	struct result
	{
		u8 value[8];
		u32 location;
		u16 region_id;
		datatype type;

		void print_info() const;
		bool compare_bytes(const std::vector<u8>& bytes) const;
	};

	class memory
	{
	public:
		memory(const i32 pid);
		std::vector<result> search(const u64 memory_limit, const type_bundle value, const char comparison);
		std::vector<result> refine_search(const type_bundle new_value, const std::vector<result>& old_results, const char comparison);
		std::vector<result> refine_search_changed(const std::vector<result>& old_results);
		std::vector<result> refine_search_unchanced(const std::vector<result>& old_results);
		void set(result& result, const type_bundle value);
		u64 region_count() const;

		template<typename T>
		T get_result_value(const result result)
		{
			std::fstream mem(mem_path, std::ios::in | std::ios::binary);
			if (!mem.is_open())
				throw "can't open " + mem_path;

			mem.seekg(result.location + get_region(result.region_id).start, std::ios::beg);

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

		/**
		 * @brief Interpret bytes as a type
		 *
		 * @param bytes bytes to interpret
		 * @param location location in the byte array
		 * @param size size of the type to interpret
		 * @return value as the given datatype
		 */
		template<typename T>
		T interpret_bytes(const u8* bytes, const size_t location, const u8 size)
		{
			type_as_bytes<T> v;

			for (u8 i = 0; i < size; ++i)
				v.bytes[i] = bytes[location + i];

			return v.type;
		}

		memory_region& get_region(const u16 id);

		// read a range of bytes from a file
		std::vector<u8> read_region(std::ifstream& file, const size_t start, const size_t end);

		struct region_snapshot
		{
			memory_region* region;
			std::vector<u8> bytes;
		};

		std::unordered_map<u16, region_snapshot> snapshot_regions(const std::vector<result>& results);

		const i32 pid;
		const std::string proc_path;
		const std::string mem_path;

		std::vector<memory_region> regions;
	};
}
