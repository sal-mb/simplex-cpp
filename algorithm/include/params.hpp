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
    bool remove_fixed{false};

    /// Remove constraint rows with all zero coefficients
    bool remove_empty_rows{false};

    /// Remove variable columns with all zero coefficients
    bool remove_empty_cols{false};

    /// Eliminate singleton rows (rows with only one non-zero entry)
    bool remove_singleton_rows{false};

    /// Eliminate redundant and forcing constraints based on row bounds L_i and U_i
    bool remove_redundant_forcing{false};

    /// Tighten individual variable bounds using finite constraint limits (Gondzio's method)
    bool tighten_bounds{false};

    /// Random seed (-1 for random), for reproducible experiments
    int seed = -1;

    /// Epsilon for dealing with numeric precision
    double eps = 1e-5;

    size_t refactor_period = 20;

    /// Multiplier applied to (m + n) to get the number of consecutive
    /// degenerate pivots allowed before Simplex falls back to pure
    /// Bland's rule (both entering and leaving selection switch to
    /// smallest-index together, since that's required for the
    /// anti-cycling guarantee to hold). 0 disables Dantzig/cheapest-fix
    /// entirely (Bland's rule from the first iteration); larger values
    /// give Dantzig/cheapest-fix more room to run before the fallback
    /// engages. Fractional values are intentional (e.g. 0.5 means half
    /// of m + n).
    double bland_threshold = 1.0;

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
                "remove-empty-rows", "Remove rows with all zero coefficients")(
                "remove-empty-cols", "Remove columns with all zero coefficients")(
                "remove-singleton-rows", "Remove singleton rows (single non-zero entry)")(
                "remove-redundant-forcing", "Remove redundant and forcing constraints based on row bounds")(
                "tighten-bounds", "Tighten individual variable bounds using constraint limits")(
                "s,seed", "Random seed (-1 for random)", cxxopts::value<int>()->default_value(std::to_string(p.seed)))(
                "e,epsilon", "Epsilon for arithmetic computations",
                cxxopts::value<double>()->default_value(std::to_string(p.eps)))(
                "r,refactor_period", "How many eta matrices needed until refactoring",
                cxxopts::value<size_t>()->default_value(std::to_string(p.refactor_period)))(
                "b,bland_threshold",
                "Multiplier of (m+n) consecutive degenerate pivots before falling back to Bland's rule (0 = always "
                "Bland)",
                cxxopts::value<double>()->default_value(std::to_string(p.bland_threshold)))(
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
        p.remove_empty_rows = result.count("remove-empty-rows") > 0;
        p.remove_empty_cols = result.count("remove-empty-cols") > 0;
        p.remove_singleton_rows = result.count("remove-singleton-rows") > 0;
        p.remove_redundant_forcing = result.count("remove-redundant-forcing") > 0;
        p.tighten_bounds = result.count("tighten-bounds") > 0;
        p.seed = result["seed"].as<int>();
        p.eps = result["epsilon"].as<double>();
        p.refactor_period = result["refactor_period"].as<size_t>();
        p.bland_threshold = result["bland_threshold"].as<double>();
        p.verbose = result.count("verbose") > 0;

        if (p.verbose) {
            fmt::print("PARAMETERS:\n");
            fmt::print("  INSTANCE: {}\n", p.instance_file);
            fmt::print("  SCALING: {}\n", p.scaling);
            fmt::print("  REMOVE_FIXED: {}\n", p.remove_fixed);
            fmt::print("  REMOVE_EMPTY_ROWS: {}\n", p.remove_empty_rows);
            fmt::print("  REMOVE_EMPTY_COLS: {}\n", p.remove_empty_cols);
            fmt::print("  REMOVE_SINGLETON_ROWS: {}\n", p.remove_singleton_rows);
            fmt::print("  REMOVE_REDUNDANT_FORCING: {}\n", p.remove_redundant_forcing);
            fmt::print("  TIGHTEN_BOUNDS: {}\n", p.tighten_bounds);
            fmt::print("  SEED: {}\n", p.seed);
            fmt::print("  EPSILON: {}\n", p.eps);
            fmt::print("  REFACTOR_PERIOD: {}\n", p.refactor_period);
            fmt::print("  BLAND_THRESHOLD: {}\n", p.bland_threshold);
        }
    }
};
