#pragma once

#include "Types.hpp"

#include <fstream>
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

	struct type_bundle
	{
		type_bundle(const std::string& value);
		const i32 _int;
		const i64 _long;
		const f32 _float;
		const f64 _double;
	};

	enum class datatype
	{
		INT, LONG, FLOAT, DOUBLE
	};

	struct result
	{
		u8 value[8];
		size_t location;
		u64 region_id;
		datatype type;
		void print_info() const;
	};

	class memory
	{
	public:
		memory(const i32 pid);
		std::vector<result> search(const type_bundle value);
		std::vector<result> refine_search(const type_bundle new_value, const std::vector<result>& old_results);
		void set(result& result, const type_bundle value);

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

		memory_region& get_region(const u64 id);

		// read a range of bytes from a file
		std::vector<u8> read_region(std::ifstream& file, const size_t start, const size_t end);

		const i32 pid;
		const std::string proc_path;
		const std::string mem_path;

		std::vector<memory_region> regions;
	};
}
