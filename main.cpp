#include <arglet/arglet.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using hrc = std::chrono::high_resolution_clock;
using seconds_t = std::chrono::duration<double, std::ratio<1, 1>>;

std::string_view std_tuple_code = R"(
#include <tuple>

int my_func() {
    auto tup = std::tuple { VALUES };
    auto sum = [](auto... values) { return (values + ...); };
    return std::apply(sum, tup);
}
)";

std::string_view tuplet_tuple_code = R"(
#include <tuplet/tuplet.hpp>

int my_func() {
    auto tup = tuplet::tuple { VALUES };
    auto sum = [](auto... values) { return (values + ...); };
    return tuplet::apply(sum, tup);
}
)";

enum class lib { stdlib, tuplet };

namespace tags {
using arglet::tag;
constexpr tag<0> library;
constexpr tag<1> sizes;
constexpr tag<2> include_dir;
constexpr tag<3> bench_file;
constexpr tag<4> output_files;
constexpr tag<5> repetitions;
constexpr tag<6> print_command;
} // namespace tags
auto get_parser() {
    using namespace arglet;
    using namespace std::literals;
    return sequence {
        ignore_arg,
        group {
            flag {tags::print_command, "--print-command"},
            value_flag {tags::repetitions, "--repetitions", 1},
            value_flag {tags::output_files, 'o', std::vector<fs::path>()},
            prefixed_value {tags::include_dir, 'I', "tuplet/include"sv},
            value_flag {
                tags::bench_file, "--bench-file", fs::path("tmp/bench.cpp")},
            option_set {
                tags::library,
                lib::stdlib,
                option {"--stdlib", lib::stdlib},
                option {"--tuplet", lib::tuplet}},
            list {tags::sizes, std::vector<size_t>()}}};
}

void prepare_path(fs::path& path) {
    if (path.is_relative()) {
        path = (fs::current_path() / path);
    }
    path = path.lexically_normal();
    if (fs::is_directory(path)) {
        throw std::invalid_argument("Expected file but input was a directory");
    }

    fs::create_directories(path.parent_path());
}

void write(fs::path const& p, std::string_view contents) {
    auto file =
        std::fstream(p.string(), std::ios_base::trunc | std::ios_base::out);

    file.write(contents.data(), contents.size());
}
int main(int argc, char const** argv) try {

    auto parser = get_parser();
    parser.parse(argc, argv);
    auto& bench_file = parser[tags::bench_file];

    prepare_path(bench_file);

    switch (parser[tags::library]) {
        case lib::stdlib: write(bench_file, std_tuple_code); break;
        case lib::tuplet: write(bench_file, tuplet_tuple_code); break;
    }

    std::vector<std::fstream> outputs;
    for (auto& p : parser[tags::output_files]) {
        outputs.emplace_back(
            p.string(), std::ios_base::trunc | std::ios_base::out);
    }

    int reps = parser[tags::repetitions];
    for (auto size : parser[tags::sizes]) {
        auto values = std::vector<size_t>(size);
        std::iota(values.begin(), values.end(), size_t(0));
        auto command = fmt::format(
            "g++-10 -std=c++20 -x c++ -c -I{0} -DVALUES='{1}' {2} -o {2}.o",
            parser[tags::include_dir],
            fmt::join(values, ", "),
            bench_file.string());

        if (parser[tags::print_command])
            fmt::print("Command: {}\n", command);

        for (int i = 0; i < reps; i++) {
            asm volatile("");
            auto t0 = hrc::now();
            system(command.c_str());
            double time =
                std::chrono::duration_cast<seconds_t>(hrc::now() - t0).count();
            asm volatile("");

            auto line = fmt::format("{}, {}\n", size, time);

            std::cout << line;
            for (auto& dest : outputs) {
                dest << line;
            }
        }
    }
} catch (std::exception& ex) {
    fmt::print(stderr, "Error: {}", ex.what());
}
