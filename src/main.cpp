#include "Memory.hpp"
#include "Options.hpp"
#include "ScopeTimer.hpp"
#include "Types.hpp"

#include <clipp.h>
#include <iostream>
#include <sstream>
#include <thread>

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
		<< "=                    find values that haven't changed since last scan\n"
		<< "!                    find values that have changed since last scan\n"
		<< "= [value]            find matching values"
		<< "> [value]            find values higher than the given value\n"
		<< "< [value]            find values lower than the given value\n"
		<< "quit                 exit the program\n";
}

int main(int argc, char** argv)
{
	using namespace std::chrono_literals;

	bool show_help = false;
	harava::options opts;

	auto cli = (
		clipp::option("--help", "-h").set(show_help) % "display help",
		(clipp::option("--pid", "-p") & clipp::number("PID").set(opts.pid)) % "PID of the process to inspect",
		(clipp::option("--memory", "-m") & clipp::number("GB").set(opts.memory_limit)) % "set the maximum memory usage in gigabytes",
		clipp::option("--skip-volatile").set(opts.skip_volatile) % "during the initial search scan each region twice and skip values that change between the two scans",
		clipp::option("--skip-zeroes").set(opts.skip_zeroes) % "skip zeroes during the initial search to lower the memory usage (only really works for comparison searches)",
		clipp::option("--skip-null-regions").set(opts.skip_null_regions) % "skip memory regions that are full of zeroes during the initial search"
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

	std::unique_ptr<harava::memory> process_memory = std::make_unique<harava::memory>(opts.pid);

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

				std::cout << " | " << std::dec;

				switch (result.type)
				{
					case harava::datatype::INT:
						std::cout << process_memory->get_result_value<i32>(result);
						break;

					case harava::datatype::LONG:
						std::cout << process_memory->get_result_value<i64>(result);
						break;

					case harava::datatype::FLOAT:
						std::cout << process_memory->get_result_value<f32>(result);
						break;

					case harava::datatype::DOUBLE:
						std::cout << process_memory->get_result_value<f64>(result);
						break;
				}

				std::cout << '\n';
			}

			continue;
		}

		if (is_cmd("reset", 1))
		{
			results.clear();
			first_search = true;

			process_memory.reset();
			process_memory = std::make_unique<harava::memory>(opts.pid);

			continue;
		}

		if (is_cmd("set", 3))
		{
			i32 index = std::stoi(tokens.at(1));
			const std::string& new_value = tokens.at(2);

			harava::type_bundle value(new_value);
			process_memory->set(results.at(index), value);

			continue;
		}

		if (is_cmd("setall", 2))
		{
			for (harava::result& r : results)
				process_memory->set(r, tokens.at(1));

			continue;
		}

		if (!first_search && is_cmd("repeat", 3))
		{
			char comparison = tokens.at(1).at(0);
			i32 count = std::stoi(tokens.at(2));

			if (count < 1)
				count = 1;

			for (i32 i = 0; i < count; ++i)
			{
				switch (comparison)
				{
					case '!':
						results = process_memory->refine_search_changed(results);
						break;

					case '=':
						results = process_memory->refine_search_unchanced(results);
						break;

					default:
						break;
				}

				std::cout << "results: " << results.size() << '\n';
				std::this_thread::sleep_for(0.5s);
			}

			continue;
		}

		if (!first_search && is_cmd("!", 1))
		{
			harava::scope_timer timer("scan duration: ");
			results = process_memory->refine_search_changed(results);
			std::cout << "results: " << results.size() << '\n';
			continue;
		}

		if (!first_search && is_cmd("=", 1))
		{
			harava::scope_timer timer("scan duration: ");
			results = process_memory->refine_search_unchanced(results);
			std::cout << "results: " << results.size() << '\n';
			continue;
		}

		if (is_cmd("=", 2) || is_cmd("<", 2) || is_cmd(">", 2))
		{
			harava::scope_timer timer("scan duration: ");

			harava::type_bundle value(tokens[1]);
			if (first_search)
			{
				results = process_memory->search(opts, value, tokens.at(0).at(0));
				first_search = false;
			}
			else
			{
				results = process_memory->refine_search(value, results, tokens.at(0).at(0));
			}

			std::cout << "results: " << results.size() << '\n';

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
