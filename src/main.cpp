#include "Memory.hpp"
#include "Types.hpp"

#include <chrono>
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

void print_help()
{
	std::cout
		<< "help                 show a list of commands\n"
		<< "list                 list out all results found so far\n"
		<< "set [index] [value]  set a new value for a result\n"
		<< "setall [value]       set a new value for all results\n"
		<< "= [value]            find matching values from the process\n"
		<< "quit                 exit the program\n";
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

		constexpr size_t max_command_size = 64;

		char buffer[max_command_size];
		std::cin.getline(buffer, max_command_size, '\n');
		std::string command = buffer;

		std::vector<std::string> tokens = tokenize_string(command, ' ');

		const auto is_cmd = [&tokens, &command](const std::string& command_name, const size_t arg_count) -> bool
		{
			return command_name == tokens.at(0) && arg_count == tokens.size();
		};

		if (is_cmd("exit", 1) || is_cmd("quit", 1)) [[unlikely]]
			break;

		if (is_cmd("list", 1))
		{
			u32 index{};
			for (const harava::result& result : results)
			{
				std::cout << std::dec << "[" << index++ << "] ";
				result.print_info();

				std::cout << " " << std::dec;

				switch (result.type)
				{
					case harava::datatype::INT:
						std::cout << process_memory.get_result_value<i32>(result);
						break;

					case harava::datatype::LONG:
						std::cout << process_memory.get_result_value<i64>(result);
						break;

					case harava::datatype::FLOAT:
						std::cout << process_memory.get_result_value<f32>(result);
						break;

					case harava::datatype::DOUBLE:
						std::cout << process_memory.get_result_value<f64>(result);
						break;
				}

				std::cout << '\n';
			}

			continue;
		}

		if (is_cmd("set", 3))
		{
			i32 index = std::stoi(tokens.at(1));
			const std::string& new_value = tokens.at(2);

			harava::type_bundle value(new_value);
			process_memory.set(results.at(index), value);

			continue;
		}

		if (is_cmd("setall", 2))
		{
			for (harava::result& r : results)
				process_memory.set(r, tokens.at(1));

			continue;
		}


		if (is_cmd("=", 2))
		{
			std::chrono::time_point scan_start = std::chrono::steady_clock::now();

			harava::type_bundle value(tokens[1]);
			if (first_search)
			{
				results = process_memory.search(value);
				first_search = false;
			}
			else
			{
				results = process_memory.refine_search(value, results);
			}

			std::chrono::time_point scan_end = std::chrono::steady_clock::now();
			std::cout << "scan duration: " << std::dec << std::chrono::duration_cast<std::chrono::milliseconds>(scan_end - scan_start) << "\n"
				<< "results: " << results.size() << '\n';

			continue;
		}

		if (is_cmd("help", 1))
		{
			print_help();
			continue;
		}

		std::cout << "unknown command\n";
	}

	return 0;
}
