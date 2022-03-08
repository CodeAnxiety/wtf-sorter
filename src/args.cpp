#include "args.h"

#include <filesystem>
#include <optional>

#include <lyra/lyra.hpp>
#include <lyra/help.hpp>

namespace fs = std::filesystem;

static app::arguments s_args;
const app::arguments & app::args = s_args;

bool app::parse_args(int argc, char ** argv)
{
    bool show_help = false;
    std::string exe, input_path, output_path;

    auto cli = lyra::cli() | lyra::exe_name(exe) | lyra::help(show_help)
             | lyra::opt([&](bool) { s_args.verbosity++; })["-v"](
                   "Increase verbosity.")
                   .cardinality(0, 2)
             | lyra::opt([&](bool) { s_args.verbosity--; })["-q"](
                   "Decrease verbosity.")
                   .cardinality(0, 2)
             | lyra::opt(s_args.dry_run)["--dry-run"](
                   "Skip saving the formatted file(s).")
             | lyra::opt(s_args.print_output)["--print-output"](
                   "Print formatted result(s).")
             | lyra::arg(input_path, "input-path")("Path to be formatted.")
                   .required()
             | lyra::arg(output_path, "output-path")("Path to save changes.");

    auto result = cli.parse({argc, argv});
    if (!result) {
        std::cerr << result.message() << "\n\n";
        std::cout << cli;
        return false;
    }
    if (show_help) {
        std::cout << cli;
        return false;
    }

    if (output_path.empty())
        output_path = input_path;

    s_args.exe = exe;
    s_args.input_path = input_path;
    s_args.output_path = output_path;
    return true;
}
