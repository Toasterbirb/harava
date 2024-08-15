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
				std::cout << std::dec << "[" << index++ << "] " << std::hex << result.location << '\n';

			continue;
		}

		if (command == "set")
		{
			i32 index, new_value;

			std::cout << "index: ";
			std::cin >> index;

			std::cout << "new value: ";
			std::cin >> new_value;

			process_memory.set(results.at(index), new_value);

			continue;
		}

		if (first_search)
		{
			const i32 value = std::stoi(command);
			results = process_memory.search(value);
			first_search = false;
		}
		else
		{
			const i32 value = std::stoi(command);
			results = process_memory.refine_search(value, results);
		}

		std::cout << "results: " << std::dec << results.size() << '\n';
	}

	return 0;
}
