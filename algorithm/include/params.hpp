#pragma once

#include <cstdlib>
#include <cxxopts.hpp>
#include <fmt/core.h>
#include <string>

struct Params {
    /// Input QPS/MPS instance file
    std::string instance_file;

    /// Apply geometric scaling preprocess (Preprocessor::scaling)
    bool scaling{false};

    /// Eliminate fixed (lb == ub) original variables before solving
    /// (mpsReader `removeFixed` argument). Slack variables are never
    /// touched by this, regardless of this flag -- every row keeps its
    /// own slack column, since that structure is what the simplex
    /// implementation's cold-start basis relies on.
    bool remove_fixed{false};

    /// Random seed (-1 for random), for reproducible experiments
    int seed = -1;

    /// Epsilon for dealing with numeric precision
    double eps = 1e-5;

    size_t refactor_period = 20;

    /// Enable verbose output
    bool verbose{false};

    static Params& get() {
        static Params instance;
        return instance;
    }

    static void set_verbose() { get().verbose = true; }

    static void parse(int argc, char* argv[]) {
        auto& p = get();
        cxxopts::Options options("dracula", "Simplex solver for QPS/MPS instances");
        options.positional_help("instance").show_positional_help();

        options.add_options()("instance", "Instance file (positional)", cxxopts::value<std::string>())(
                "c,scaling", "Apply geometric scaling preprocess")(
                "f,remove-fixed", "Eliminate fixed (lb==ub) original variables before solving")(
                "s,seed", "Random seed (-1 for random)", cxxopts::value<int>()->default_value(std::to_string(p.seed)))(
                "e,epsilon", "Epsilon for arithmetic computations",
                cxxopts::value<double>()->default_value(std::to_string(p.eps)))

                ("r,refactor_period", "How many eta matrices needed until refactoring",
                 cxxopts::value<size_t>()->default_value(std::to_string(p.refactor_period)))(
                        "v,verbose", "Enable verbose output")("h,help", "Print help message");

        options.parse_positional({"instance"});
        auto result = options.parse(argc, argv);

        if (result.count("help") || result.count("instance") == 0) {
            fmt::print("{}", options.help());
            std::exit(0);
        }

        p.instance_file = result["instance"].as<std::string>();
        p.scaling = result.count("scaling") > 0;
        p.remove_fixed = result.count("remove-fixed") > 0;
        p.seed = result["seed"].as<int>();
        p.eps = result["epsilon"].as<double>();
        p.refactor_period = result["refactor_period"].as<size_t>();
        p.verbose = result.count("verbose") > 0;

        if (p.verbose) {
            fmt::print("PARAMETERS:\n");
            fmt::print("  INSTANCE: {}\n", p.instance_file);
            fmt::print("  SCALING: {}\n", p.scaling);
            fmt::print("  REMOVE_FIXED: {}\n", p.remove_fixed);
            fmt::print("  SEED: {}\n", p.seed);
            fmt::print("  EPSILON: {}\n", p.eps);
        }
    }
};
