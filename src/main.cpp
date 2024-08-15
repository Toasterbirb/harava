#include "Memory.hpp"
#include "Types.hpp"

#include <clipp.h>
#include <iostream>
#include <sstream>

struct options
{
	i32 pid{};
	bool no_pause = false;
};

std::vector<std::string> tokenize_string(const std::string& line, const char separator)
{
	std::vector<std::string> tokens;

	std::istringstream line_stream(line);
	std::string token;

	while (std::getline(line_stream, token, separator))
		tokens.push_back(token);

	return tokens;
}


int main(int argc, char** argv)
{
	bool show_help = false;
	options opts;

	auto cli = (
		clipp::option("--help", "-h").set(show_help) % "display help",
		(clipp::option("--pid", "-p") & clipp::number("PID").set(opts.pid)) % "PID of the process to inspect",
		clipp::option("--no-pause").set(opts.no_pause) % "don't pause the process while inspecting its memory"
	);

	if (!clipp::parse(argc, argv, cli))
	{
		std::cout << "arg parsing error!\ncheck --help for help\n";
		return 1;
	}

	if (show_help)
	{
		const auto fmt = clipp::doc_formatting{}.doc_column(40);
		std::cout << clipp::make_man_page(cli, "harava", fmt);
		return 0;
	}

	harava::memory process_memory(opts.pid);

	std::vector<harava::result> results;
	bool first_search = true;

	while (true)
	{
		std::cout << " > ";

		std::string command;
		std::cin >> command;

		if (command == "list")
		{
			u32 index{};
			for (const harava::result& result : results)
			{
				std::cout << std::dec << "[" << index++ << "] ";
				result.print_info();
				std::cout << '\n';
			}


			continue;
		}

		if (command == "set")
		{
			i32 index;

			std::cout << "index: ";
			std::cin >> index;

			std::cout << "new value: ";
			std::string new_value;
			std::cin >> new_value;

			const i32 new_value_int = std::stoi(new_value);
			const i64 new_value_long = std::stol(new_value);
			const f32 new_value_float = std::stof(new_value);
			const f64 new_value_double = std::stold(new_value);


			process_memory.set(results.at(index), new_value_int, new_value_long, new_value_float, new_value_double);

			continue;
		}

		if (first_search)
		{
			const i32 value_int = std::stoi(command);
			const i64 value_long = std::stol(command);
			const f32 value_float = std::stof(command);
			const f64 value_double = std::stold(command);
			results = process_memory.search(value_int, value_long, value_float, value_double);
			first_search = false;
		}
		else
		{
			const i32 value_int = std::stoi(command);
			const i64 value_long = std::stol(command);
			const f32 value_float = std::stof(command);
			const f64 value_double = std::stold(command);

			results = process_memory.refine_search(value_int, value_long, value_float, value_double, results);
		}

		std::cout << "results: " << std::dec << results.size() << '\n';
	}

	return 0;
}
