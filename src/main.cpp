#include "Options.hpp"
#include "Shell.hpp"
#include "Types.hpp"

#include <clipp.h>
#include <iostream>

int main(int argc, char** argv)
{
	bool show_help = false;
	harava::options opts;

	auto cli = (
		clipp::option("--help", "-h").set(show_help) % "display help",
		(clipp::option("--pid", "-p") & clipp::number("PID").set(opts.pid)) % "PID of the process to inspect",
		(clipp::option("--memory", "-m") & clipp::number("GB").set(opts.memory_limit)) % "set the maximum memory usage in gigabytes",
		clipp::option("--skip-volatile").set(opts.skip_volatile) % "during the initial search scan each region twice and skip values that change between the two scans",
		clipp::option("--skip-zeroes").set(opts.skip_zeroes) % "skip zeroes during the initial search to lower the memory usage (only really works for comparison searches)",
		clipp::option("--skip-null-regions").set(opts.skip_null_regions) % "skip memory regions that are full of zeroes during the initial search",
		clipp::option("--stack").set(opts.stack_scan) % "only scan the stack of the process"
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

	harava::run_shell(opts);

	return 0;
}
