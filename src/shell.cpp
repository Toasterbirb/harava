#include "Memory.hpp"
#include "ScopeTimer.hpp"
#include "Shell.hpp"

#include <algorithm>
#include <execution>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace harava
{
	std::vector<std::string> tokenize_string(const std::string& line, const char separator);

	struct command
	{
		command(const std::string& cmd_line)
		{
			if (cmd_line.empty())
				return;

			const std::vector<std::string> tokens = tokenize_string(cmd_line, ' ');
			cmd = *tokens.begin();
			args.insert(args.begin(), tokens.begin() + 1, tokens.end());
		}

		std::string cmd;
		std::vector<std::string> args;
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

	void run_shell(const options opts)
	{
		std::unique_ptr<harava::memory> process_memory = std::make_unique<harava::memory>(opts.pid, opts);

		harava::filter filter;

		// map strings to filter options to make arg parsing simpler (and more fun)
		std::map<std::string, bool*> type_filter_mappings = {
			{ "i32", &filter.enable_i32 },
			{ "i64", &filter.enable_i64 },
			{ "f32", &filter.enable_f32 },
			{ "f64", &filter.enable_f64 }
		};

		results results;
		bool first_search = true;
		bool running = true;

		const std::string scan_duration_str = "scan duration: ";
		const std::string do_initial_search_notif_str = "do an initial scan first";
		const auto print_result_count = [&results]() { std::cout << "results: " << results.count() << '\n'; };

		std::cout << "type 'help' for a list of commands\n";

		while (running)
		{
			std::cout << " > ";

			constexpr size_t max_command_size = 64;

			char buffer[max_command_size];
			std::cin.getline(buffer, max_command_size, '\n');
			command command(buffer);

			// if the command is empty, don't even attempt to execute it
			if (command.cmd.empty())
				continue;

			// command format: <comand name, argument description, command description, argument count, function to run>
			const static std::vector<std::tuple<std::string, std::string, std::string, i8, std::function<void()>>> commands = {
				{
					"help",
					"",
					"show help",
					0,
					[&]
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
					[&] { running = false; }
				},
				{
					"=",
					"[value]",
					"find matching values",
					1,
					[&]
					{
						harava::scope_timer timer(scan_duration_str);
						harava::type_bundle value(command.args.at(0));
						if (!value.valid)
							return;

						results = first_search
							? process_memory->search(opts, filter, value, harava::comparison::eq)
							: process_memory->refine_search(value, results, harava::comparison::eq);

						first_search = false;
						print_result_count();
					}
				},
				{
					">",
					"[value]",
					"find values higher than the given value",
					1,
					[&]
					{
						harava::scope_timer timer(scan_duration_str);
						harava::type_bundle value(command.args.at(0));
						if (!value.valid)
							return;

						results = first_search
							? process_memory->search(opts, filter, value, harava::comparison::gt)
							: process_memory->refine_search(value, results, harava::comparison::gt);

						first_search = false;
						print_result_count();
					}
				},
				{
					"<",
					"[value]",
					"find values lower than the given value",
					1,
					[&]
					{
						harava::scope_timer timer(scan_duration_str);
						harava::type_bundle value(command.args.at(0));
						if (!value.valid)
							return;

						results = first_search
							? process_memory->search(opts, filter, value, harava::comparison::lt)
							: process_memory->refine_search(value, results, harava::comparison::lt);

						first_search = false;
						print_result_count();
					}
				},
				{
					">=",
					"[value]",
					"find values higher than or equal to the given value",
					1,
					[&]
					{
						harava::scope_timer timer(scan_duration_str);
						harava::type_bundle value(command.args.at(0));
						if (!value.valid)
							return;

						results = first_search
							? process_memory->search(opts, filter, value, harava::comparison::ge)
							: process_memory->refine_search(value, results, harava::comparison::ge);

						first_search = false;
						print_result_count();
					}
				},
				{
					"<=",
					"[value]",
					"find values lower than or equal to the given value",
					1,
					[&]
					{
						harava::scope_timer timer(scan_duration_str);
						harava::type_bundle value(command.args.at(0));
						if (!value.valid)
							return;

						results = first_search
							? process_memory->search(opts, filter, value, harava::comparison::le)
							: process_memory->refine_search(value, results, harava::comparison::le);

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
						if (first_search)
						{
							std::cout << do_initial_search_notif_str << '\n';
							return;
						}

						harava::scope_timer timer(scan_duration_str);
						results = process_memory->refine_search_change(results, true);
						print_result_count();
					}
				},
				{
					"!",
					"",
					"find values that have changed since last scan",
					0,
					[&]
					{
						if (first_search)
						{
							std::cout << do_initial_search_notif_str << '\n';
							return;
						}

						harava::scope_timer timer(scan_duration_str);
						results = process_memory->refine_search_change(results, false);
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
						if (first_search)
						{
							std::cout << do_initial_search_notif_str << '\n';
							return;
						}

						char comparison = command.args.at(0).at(0);
						i32 count{0};

						try
						{
							count = std::stoi(command.args.at(1));
						}
						catch (const std::exception& e)
						{
							std::cout << "invalid argument: " << command.args.at(1) << '\n';
							return;
						}

						if (count < 1)
							count = 1;

						size_t previous_result_count{results.count()};
						u8 same_result_streak{0};

						for (i32 i = 0; i < count; ++i)
						{
							harava::scope_timer timer(scan_duration_str);

							switch (comparison)
							{
								case '!':
									results = process_memory->refine_search_change(results, false);
									break;

								case '=':
									results = process_memory->refine_search_change(results, true);
									break;

								default:
									std::cout << "unimplemented repeat comparison\n";
									return;
							}

							if (results.count() == previous_result_count)
								++same_result_streak;
							else
								same_result_streak = 0;

							print_result_count();
							previous_result_count = results.count();

							if (same_result_streak >= 3)
							{
								std::cout << "stopping the repeat check as it doesn't seem to help\n";
								break;
							}
						}
					}
				},
				{
					"repeat",
					"[!|=]",
					"repeat a comparison until the result count stops changing",
					1,
					[&]
					{
						if (first_search)
						{
							std::cout << do_initial_search_notif_str << '\n';
							return;
						}

						char comparison = command.args.at(0).at(0);
						size_t previous_result_count{0};

						while (previous_result_count != results.count())
						{
							harava::scope_timer timer(scan_duration_str);
							previous_result_count = results.count();

							switch (comparison)
							{
								case '!':
									results = process_memory->refine_search_change(results, false);
									break;

								case '=':
									results = process_memory->refine_search_change(results, true);
									break;

								default:
									std::cout << "unimplemented repeat comparison\n";
									return;
							}

							print_result_count();
						}
					}
				},
				{
					"list",
					"",
					"list out all results found so far",
					0,
					[&results, &process_memory]
					{
						u64 counter{0};

						const auto print_value = [&process_memory](const harava::result result)
						{
							switch (result.type)
							{
								case datatype::INT:
									std::cout << std::dec << process_memory->get_result_value<i32>(result);
									break;

								case datatype::LONG:
									std::cout << std::dec << process_memory->get_result_value<i64>(result);
									break;

								case datatype::FLOAT:
									std::cout << std::dec << process_memory->get_result_value<f32>(result);
									break;

								case datatype::DOUBLE:
									std::cout << std::dec << process_memory->get_result_value<f64>(result);
									break;
							}
						};

						const auto result_vecs = results.result_vecs();

						for (const auto& [index, vec] : result_vecs)
						{
							for (const result r : *vec)
							{
								const u8 type_index = (static_cast<u8>(r.type) & 0xF0) >> 4UL;
								std::cout << std::dec << "[" << counter++ << "] "
									<< std::right << std::hex << std::setw(5) << r.location << " | "
									<< datatype_names[type_index] << " | ";

								print_value(r);
								std::cout << '\n';
							}
						}
					}
				},
				{
					"set",
					"[index] [value]",
					"set a new value for a result",
					2,
					[&command, &results, &process_memory]
					{
						i32 index{0};

						try
						{
							index = std::stoi(command.args.at(0));
						}
						catch (const std::exception& e)
						{
							std::cout << "invalid argument: " << command.args.at(0) << '\n';
							return;
						}

						const std::string& new_value = command.args.at(1);

						harava::type_bundle value(new_value);
						if (!value.valid)
							return;

						std::optional<result*> result = results.at(index);

						if (result.has_value())
							process_memory->set(*result.value(), value);
					}
				},
				{
					"setall",
					"[value]",
					"set a new value for all results",
					1,
					[&command, &results, &process_memory]
					{
						for (auto& [index, vec] : results.result_vecs())
						{
							for (harava::result& r : *vec)
								process_memory->set(r, command.args.at(0));
						}
					}
				},
				{
					"types",
					"",
					"list currently enabled types",
					0,
					[type_filter_mappings]
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
					[&command, &type_filter_mappings]
					{
						// if "all" is specified as the argument, enabled all types
						// and don't do anything else
						if (command.args.at(0) == "all")
						{
							for (const auto[type, boolean_pointer] : type_filter_mappings)
								*boolean_pointer = true;
							return;
						}

						// validate the types
						for (auto it = command.args.begin(); it != command.args.end(); ++it)
						{
							if (!type_filter_mappings.contains(*it))
							{
								std::cout << "invalid type: " << *it << '\n';
								return;
							}
						}

						// set all types to disabled state
						for (const auto[type, boolean_pointer] : type_filter_mappings)
							*boolean_pointer = false;

						// loop over the arguments and enable the mentioned types
						for (auto it = command.args.begin(); it != command.args.end(); ++it)
							*type_filter_mappings.at(*it) = true;
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
						process_memory = std::make_unique<harava::memory>(opts.pid, opts);
					}
				}
			};

			auto command_to_run = std::find_if(std::execution::par_unseq, commands.begin(), commands.end(), [&command](const auto& cmd)
					{
						// commands with variable argument count
						if (std::get<3>(cmd) == -1 && !command.args.empty() && std::get<0>(cmd) == command.cmd) [[unlikely]]
							return true;

						return std::get<0>(cmd) == command.cmd && std::get<3>(cmd) == command.args.size();
					});


			if (command_to_run == commands.end())
			{
				std::cout << "unknown command\n";
				continue;
			}

			// execute the command
			std::get<std::function<void()>>(*command_to_run)();
		}
	}
}
