#include "Filter.hpp"
#include "Memory.hpp"
#include "Options.hpp"
#include "ScopeTimer.hpp"
#include "Types.hpp"

#include <algorithm>
#include <clipp.h>
#include <execution>
#include <functional>
#include <iostream>
#include <map>
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

	harava::filter filter;

	// map strings to filter options to make arg parsing simpler (and more fun)
	std::map<std::string, bool*> type_filter_mappings = {
		{ "i32", &filter.enable_i32 },
		{ "i64", &filter.enable_i64 },
		{ "f32", &filter.enable_f32 },
		{ "f64", &filter.enable_f64 }
	};

	std::vector<harava::result> results;
	bool first_search = true;
	bool running = true;

	const std::string scan_duration_str = "scan duration: ";
	const std::string do_initial_search_notif_str = "do an initial scan first";
	const auto print_result_count = [&results]() { std::cout << "results: " << results.size() << '\n'; };

	while (running)
	{
		std::cout << " > ";

		constexpr size_t max_command_size = 64;

		char buffer[max_command_size];
		std::cin.getline(buffer, max_command_size, '\n');
		std::string command = buffer;

		std::vector<std::string> tokens = tokenize_string(command, ' ');

		// command format: <comand name, argument description, command description, argument count, function to run>
		const static std::vector<std::tuple<std::string, std::string, std::string, i8, std::function<void()>>> commands = {
			{
				"help",
				"",
				"show help",
				0,
				[&]()
				{
					std::cout << std::left;
					for (const auto&[cmd_name, arg_desc, cmd_desc, arg_count, func] : commands)
					{
						constexpr u8 cmd_name_arg_width = 32;
						if (arg_count == 0)
						{
							std::cout << std::setw(cmd_name_arg_width) << cmd_name << cmd_desc << '\n';
							continue;
						}

						std::cout << std::setw(cmd_name_arg_width) << cmd_name + " " + arg_desc << cmd_desc << '\n';
					}
				}
			},
			{
				"quit",
				"",
				"quit the program",
				0,
				[&]() { running = false; }
			},
			{
				"=",
				"[value]",
				"find matching values",
				1,
				[&]()
				{
					harava::scope_timer timer(scan_duration_str);
					harava::type_bundle value(tokens.at(1));

					results = first_search
						? process_memory->search(opts, filter, value, '=')
						: process_memory->refine_search(value, results, '=');

					first_search = false;
					print_result_count();
				}
			},
			{
				">",
				"[value]",
				"find values higher than the given value",
				1,
				[&]()
				{
					harava::scope_timer timer(scan_duration_str);
					harava::type_bundle value(tokens.at(1));

					results = first_search
						? process_memory->search(opts, filter, value, '>')
						: process_memory->refine_search(value, results, '>');

					first_search = false;
					print_result_count();
				}
			},
			{
				"<",
				"[value]",
				"find values lower than the given value",
				1,
				[&]()
				{
					harava::scope_timer timer(scan_duration_str);
					harava::type_bundle value(tokens.at(1));

					results = first_search
						? process_memory->search(opts, filter, value, '<')
						: process_memory->refine_search(value, results, '<');

					first_search = false;
					print_result_count();
				}
			},
			{
				"=",
				"",
				"find values that have not changed since last scan",
				0,
				[&]
				{
					if (!first_search)
					{
						std::cout << do_initial_search_notif_str << '\n';
						return;
					}

					harava::scope_timer timer("scan duration: ");
					results = process_memory->refine_search_unchanced(results);
					print_result_count();
				}
			},
			{
				"=",
				"",
				"find values that have changed since last scan",
				0,
				[&]
				{
					if (!first_search)
					{
						std::cout << do_initial_search_notif_str << '\n';
						return;
					}

					harava::scope_timer timer("scan duration: ");
					results = process_memory->refine_search_changed(results);
					print_result_count();
				}
			},
			{
				"repeat",
				"[!|=] [count]",
				"repeat a comparison multiple times in a row with a slight delay",
				2,
				[&]
				{
					if (!first_search)
					{
						std::cout << do_initial_search_notif_str << '\n';
						return;
					}

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
								std::cout << "unimplemented repeat comparison\n";
								return;
						}

						std::cout << "results: " << results.size() << '\n';
						std::this_thread::sleep_for(0.5s);
					}
				}
			},
			{
				"list",
				"",
				"list out all results found so far",
				0,
				[&results, &process_memory]()
				{
					for (size_t i = 0; i < results.size(); ++i)
					{
						std::cout << std::dec << "[" << i << "] ";
						results.at(i).print_info();

						std::cout << " | " << std::dec;

						switch (results[i].type)
						{
							case harava::datatype::INT:
								std::cout << process_memory->get_result_value<i32>(results[i]);
								break;

							case harava::datatype::LONG:
								std::cout << process_memory->get_result_value<i64>(results[i]);
								break;

							case harava::datatype::FLOAT:
								std::cout << process_memory->get_result_value<f32>(results[i]);
								break;

							case harava::datatype::DOUBLE:
								std::cout << process_memory->get_result_value<f64>(results[i]);
								break;
						}

						std::cout << '\n';
					}
				}
			},
			{
				"set",
				"[index] [value]",
				"set a new value for a result",
				2,
				[&tokens, &results, &process_memory]()
				{
					i32 index = std::stoi(tokens.at(1));
					const std::string& new_value = tokens.at(2);

					harava::type_bundle value(new_value);
					process_memory->set(results.at(index), value);
				}
			},
			{
				"setall",
				"[value]",
				"set a new value for all results",
				1,
				[&tokens, &results, &process_memory]
				{
					for (harava::result& r : results)
						process_memory->set(r, tokens.at(1));
				}
			},
			{
				"types",
				"",
				"list currently enabled types",
				0,
				[type_filter_mappings]()
				{
					for (const auto[type, boolean_pointer] : type_filter_mappings)
						if (*boolean_pointer)
							std::cout << type << '\n';
				}
			},
			{
				"types",
				"[i32|i64|f32|f64 ...]",
				"specify the types that should be searched for",
				-1,
				[&tokens, &type_filter_mappings]
				{
					// if "all" is specified as the argument, enabled all types
					// and don't do anything else
					if (tokens.at(1) == "all")
					{
						for (const auto[type, boolean_pointer] : type_filter_mappings)
							*boolean_pointer = true;
						return;
					}

					// set all types to disabled state
					for (const auto[type, boolean_pointer] : type_filter_mappings)
						*boolean_pointer = false;

					// loop over the arguments while skipping over the command
					// and enable the mentioned types
					for (auto it = tokens.begin() + 1; it != tokens.end(); ++it)
					{
						if (!type_filter_mappings.contains(*it))
						{
							std::cout << "invalid type: " << *it << '\n';
							break;
						}

						*type_filter_mappings.at(*it) = true;
					}
				}
			},
			{
				"reset",
				"",
				"clear the result list and start a new search",
				0,
				[&results, &first_search, &process_memory, opts]
				{
					results.clear();
					first_search = true;

					process_memory.reset();
					process_memory = std::make_unique<harava::memory>(opts.pid);
				}
			}
		};

		auto command_to_run = std::find_if(std::execution::par_unseq, commands.begin(), commands.end(), [&tokens](const auto& cmd)
				{
					// commands with variable argument count
					if (std::get<3>(cmd) == -1 && tokens.size() > 1 && std::get<0>(cmd) == tokens.at(0))
						return true;

					return std::get<0>(cmd) == tokens.at(0) && std::get<3>(cmd) == tokens.size() - 1;
				});


		if (command_to_run == commands.end())
		{
			std::cout << "unknown command\n";
			continue;
		}

		// execute the command
		std::get<4>(*command_to_run)();
	}

	return 0;
}
