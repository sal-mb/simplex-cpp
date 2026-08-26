#include <Eigen/Dense>
#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>
#include <chrono>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include "mps_reader.hpp"
#include "params.hpp"
#include "preprocess.hpp"
#include "simplex.hpp"

using namespace fmt;
using namespace Eigen;

int main(int argc, char* argv[]) {
    Params::parse(argc, argv);
    auto& p = Params::get();

    // Step 1: Read MPS file (pure parsing, no preprocessing)
    MpsReader reader;
    ProblemData problem_data = reader.read(p.instance_file);
    println("RAW: \n{}", problem_data);
    getchar();

    // Step 2: Preprocess (fixed variable removal -> scaling -> slack variable addition)
    Preprocessor preprocessor;
    preprocessor.remove_fixed = p.remove_fixed;
    preprocessor.scaling = p.scaling;
    ProblemData preprocessed = preprocessor.process(problem_data);
    println("PREPROCESSED: \n{}", preprocessed);
    getchar();

    // Step 3: Solve using Simplex with ProblemData.
    // Phase 0 finds an initial feasible basis (it temporarily overrides
    // c with a feasibility-penalty cost internally); Phase 1, warm-started
    // from Phase 0's resulting basis, then actually optimizes the true
    // objective.
    auto start = std::chrono::high_resolution_clock::now();

    Simplex phase0_solver(preprocessed, p, std::nullopt, 0);
    Solution phase0_solution = phase0_solver.solve();

    Simplex phase1_solver(preprocessed, p, phase0_solution, 1);
    Solution s = phase1_solver.solve();

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;

    // s.cost only covers variables still present in x/c; any variable
    // eliminated by remove_fixed_variables contributes obj_constant
    // instead, since it no longer appears in either.
    double true_cost = s.cost + preprocessed.obj_constant;

    println("Objective cost: {}", true_cost);
    println("Total time: {:.6f} seconds", elapsed.count());

    if (p.verbose) {
        println("Final solution:");
        println("  basic_idx: {}", s.basic_idx);
        println("  nonbasic_idx: {}", s.nonbasic_idx);
        println("  x: {}", s.x);
    }

    return 0;
}
