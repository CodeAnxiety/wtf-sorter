#include "logging.h"
#include "args.h"

#include <fmt/format.h>
#include <fmt/os.h>

#include <fstream>
#include <functional>
#include <unordered_set>

using namespace app;
namespace fs = std::filesystem;

namespace
{
    bool less_than(std::string_view lhs, std::string_view rhs)
    {
        return std::lexicographical_compare(
            lhs.begin(), lhs.end(),  //
            rhs.begin(), rhs.end(),  //
            [](const char & lhs, const char & rhs) {
                return std::tolower(lhs) < std::tolower(rhs);
            });
    }

    bool equal(const fs::path & path, std::string_view match)
    {
        std::string path_string = path.string();
        return std::equal(path_string.begin(), path_string.end(),  //
                          match.begin(), match.end(),              //
                          [](const char & lhs, const char & rhs) {
                              return std::tolower(lhs) == std::tolower(rhs);
                          });
    }

    class file_searcher
    {
      private:
        std::function<bool(const fs::path &)> m_predicate;
        std::vector<fs::path> m_files;
        std::unordered_set<std::string> m_visited;

      public:
        template <typename Predicate>
        file_searcher(const fs::path & path, Predicate && predicate)
            : m_predicate(predicate)
        {
            visit(path, 0);
        }

        auto begin() const { return m_files.begin(); }
        auto end() const { return m_files.end(); }

        auto empty() const { return m_files.empty(); }
        auto size() const { return m_files.size(); }

      private:
        void visit(const fs::path & path, int depth)
        {
            debug(depth, "Visiting: {}", path);

            if (!fs::exists(path)) {
                debug(depth + 1, "Invalid: {}", path);
                return;
            }

            // ensure we only visit a path once
            {
                std::string canonical = fs::canonical(path).string();
                if (m_visited.contains(canonical))
                    return;
                m_visited.emplace(std::move(canonical));
            }

            if (fs::is_directory(path)) {
                for (const fs::path & child : fs::directory_iterator{path})
                    visit(child, depth + 1);
            }
            else if (m_predicate(path)) {
                debug(depth + 1, "Found: {}", path);
                m_files.emplace_back(path);
            }
            else
                debug(depth + 1, "Ignored: {}", path);
        }
    };

    bool make_directory(const fs::path & path)
    {
        auto directory = fs::is_directory(path) ? path : path.parent_path();
        if (fs::exists(directory) || fs::create_directories(directory))
            return true;

        error("Could not create directory: {}", directory);
        return false;
    }

    fs::path determine_output(const fs::path & path)
    {
        static bool is_output_directory = fs::is_directory(args.output_path);
        if (!is_output_directory)
            return args.output_path;

        static bool is_input_directory = fs::is_directory(args.input_path);
        if (!is_input_directory)
            return args.output_path / path.filename();

        return args.output_path / fs::relative(path, args.input_path);
    }

    bool save_to_output(const fs::path & path, std::string_view text)
    {
        auto output_path = determine_output(path);

        if (args.print_output) {
            fmt::print("# BEGIN: {0}\n{1}\n# END: {0}\n", output_path, text);
        }

        if (args.dry_run) {
            debug("{} -> {}", fs::relative(path, args.input_path), output_path);
            return true;
        }

        if (!make_directory(output_path)) {
            error("Could nto create directory: {}", output_path.parent_path());
            return false;
        }

        std::ofstream stream{output_path};
        if (stream.fail()) {
            error("Could not save file: {}", output_path);
            return false;
        }

        stream << text;
        return true;
    }

    std::optional<std::string> read_file(const std::filesystem::path & path)
    {
        std::ifstream stream{path};
        stream.exceptions(std::ios::badbit);

        std::string buffer;
        buffer.resize(4096);

        std::string result;
        while (stream.read(&buffer[0], buffer.size()))
            result.append(buffer, 0, stream.gcount());

        result.append(buffer, 0, stream.gcount());
        return result;
    }

    std::vector<std::string_view> split(std::string_view text,
                                        std::string_view separator)
    {
        std::vector<std::string_view> result;

        std::size_t last_index = 0;
        while (last_index < text.size()) {
            std::size_t next_index =
                std::min(text.size(), text.find(separator, last_index));
            result.emplace_back(std::string_view(text.data() + last_index,
                                                 text.data() + next_index));
            last_index = next_index + separator.size();
        }

        return result;
    }

    std::string_view trim(std::string_view text,
                          std::string_view characters = " \t\r\n")
    {
        text.remove_prefix(text.find_first_not_of(characters));
        text.remove_suffix(text.size() - text.find_last_not_of(characters) - 1);
        return text;
    }

    std::string join(const std::vector<std::string_view> & lines,
                     std::string_view separator)
    {
        fmt::memory_buffer buffer;
        for (std::string_view line : lines) {
            line = trim(line);
            if (line.empty())
                continue;
            buffer.append(line);
            buffer.append(separator);
        }
        return {buffer.data(), buffer.size()};
    }

    std::optional<std::string> sort(const std::filesystem::path & path)
    {
        auto contents = read_file(path);
        if (!contents) {
            error("Could not load file: {}", path);
            return std::nullopt;
        }

        auto lines = split(contents.value(), "\n");
        std::sort(lines.begin(), lines.end(), less_than);

        return join(lines, "\n");
    }
}

int main(int argc, char ** argv)
{
    if (const auto result = parse_args(argc, argv))
        return *result;

    info("{} v0.0.1-alpha", args.exe.filename());
    debug("arguments:");
    debug("- verbosity:       {}", args.verbosity);
    debug("- dry_run:         {}", args.dry_run ? "true" : "false");
    debug("- input_path:      {}", args.input_path);
    debug("- output_path:     {}", args.output_path);

    if (!std::filesystem::exists(args.input_path)) {
        error("Input path not found: {}", args.input_path);
        return 1;
    }

    std::vector<fs::path> files;

    // find .wtf files
    {
        file_searcher searcher{args.input_path, [](const fs::path & path) {
                                   return equal(path.extension().string(),
                                                ".wtf");
                               }};
        files.insert(files.end(), searcher.begin(), searcher.end());
    }

    // find addons.txt files
    {
        file_searcher searcher{args.input_path, [](const fs::path & path) {
                                   return equal(path.filename().string(),
                                                "addons.txt");
                               }};
        files.insert(files.end(), searcher.begin(), searcher.end());
    }

    if (files.empty()) {
        error("No wtf files found for path: {}", args.input_path);
        return 0;
    }

    size_t index = 0;
    double percent_multiplier = 100.0 / files.size();
    for (const fs::path & path : files) {
        verbose("[{0:>3.0f}%] {1} of {2}: {3}", ++index * percent_multiplier,
                index, files.size(), path);
        if (auto sorted = sort(path)) {
            if (save_to_output(path, trim(sorted.value())))
                continue;
        }
        info("Problems encountered, aborting...");
        break;
    }

    info("Done. Sorted {} file(s).", files.size());
    return 0;
}
