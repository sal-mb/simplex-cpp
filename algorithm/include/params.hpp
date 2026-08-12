#pragma once

#include <cstdlib>
#include <cxxopts.hpp>
#include <fmt/core.h>
#include <string>

struct Params {
    /// Input QPS/MPS instance file
    std::string instance_file;

    /// Apply geometric scaling preprocess (mpsReader `pre` argument)
    bool preprocess{false};

    /// Random seed (-1 for random), for reproducible experiments
    int seed = -1;

    /// Enable verbose output
    bool verbose{false};

    // ---- future parameters go here ----
    // e.g. presolve, simplex method choice, tolerances, max iterations

    static Params& get() {
        static Params instance;
        return instance;
    }

    static void set_verbose() { get().verbose = true; }

    static void parse(int argc, char* argv[]) {
        auto& p = get();
        cxxopts::Options options("dracula", "Simplex solver for QPS/MPS instances");
        options.positional_help("instance").show_positional_help();

        options.add_options()("instance", "Instance file (positional)",
                              cxxopts::value<std::string>())(
            "p,preprocess", "Apply geometric scaling preprocess")(
            "s,seed", "Random seed (-1 for random)",
            cxxopts::value<int>()->default_value(std::to_string(p.seed)))(
            "v,verbose", "Enable verbose output")("h,help", "Print help message");

        options.parse_positional({"instance"});
        auto result = options.parse(argc, argv);

        if (result.count("help") || result.count("instance") == 0) {
            fmt::print("{}", options.help());
            std::exit(0);
        }

        p.instance_file = result["instance"].as<std::string>();
        p.preprocess = result.count("preprocess") > 0;
        p.seed = result["seed"].as<int>();
        p.verbose = result.count("verbose") > 0;

        if (p.verbose) {
            fmt::print("PARAMETERS:\n");
            fmt::print("  INSTANCE: {}\n", p.instance_file);
            fmt::print("  PREPROCESS: {}\n", p.preprocess);
            fmt::print("  SEED: {}\n", p.seed);
        }
    }
};
