#pragma once

#include "Types.hpp"

#include <array>
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

	enum class datatype
	{
		INT, LONG, FLOAT, DOUBLE
	};

	struct result
	{
		std::array<u8, 8> value;
		size_t location;
		u64 region_id;
		datatype type;
		void print_info() const;
	};

	class memory
	{
	public:
		memory(const i32 pid);
		std::vector<result> search(const i32 value_int, const i64 value_long, const f32 value_float, const f64 value_double);
		std::vector<result> refine_search(const i32 new_value_int, const i64 new_value_long, const f32 new_value_float, const f64 new_value_double, const std::vector<result>& old_results);
		void set(result& result, const i32 new_value_int, const i64 new_value_long, const f32 new_value_float, const f64 new_value_double);

	private:
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
			union value
			{
				T type;
				u8 bytes[8];
			};

			value v;

			for (u8 i = 0; i < size; ++i)
				v.bytes[i] = bytes[location + i];

			return v.type;
		}

		memory_region& get_region(const u64 id);

		// read a range of bytes from a file
		std::vector<u8> read_region(const std::string& path, const size_t start, const size_t end);

		const i32 pid;
		const std::string proc_path;
		const std::string mem_path;

		std::vector<memory_region> regions;
	};
}
