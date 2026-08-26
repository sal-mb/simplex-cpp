// main.cpp
#include <chrono>
#include <fmt/core.h>

#include "mps_reader.hpp"
#include "params.hpp"
#include "preprocess.hpp"
#include "simplex.hpp"

int main(int argc, char* argv[]) {
    Params::parse(argc, argv);
    auto& p = Params::get();

    MpsReader reader;
    ProblemData problem_data = reader.read(p.instance_file);

    // Passo 2: Preprocessamento
    Preprocessor preprocessor(p, problem_data);
    ProblemData preprocessed = preprocessor.process();

    auto start = std::chrono::high_resolution_clock::now();

    // Fase 0: Encontrar base viável
    auto t0_start = std::chrono::high_resolution_clock::now();
    Simplex phase0_solver(preprocessed, p, std::nullopt, 0);
    Solution phase0_solution = phase0_solver.solve();
    auto t0_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> phase0_time = t0_end - t0_start;

    // Fase 1: Otimização do objetivo real
    auto t1_start = std::chrono::high_resolution_clock::now();
    Simplex phase1_solver(preprocessed, p, phase0_solution, 1);
    Solution s = phase1_solver.solve();
    auto t1_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> phase1_time = t1_end - t1_start;

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    double true_cost = s.cost + preprocessed.obj_constant;

    // Impressão estruturada de métricas para o Runner Python
    fmt::print("[STATS] Orig_Constraints: {}\n", problem_data.m);
    fmt::print("[STATS] Orig_Variables: {}\n", problem_data.n);
    fmt::print("[STATS] Prep_Constraints: {}\n", preprocessed.m);
    fmt::print("[STATS] Prep_Variables: {}\n", preprocessed.n);
    fmt::print("[STATS] Phase0_Iterations: {}\n", phase0_solver.iteration);
    fmt::print("[STATS] Phase0_Cost: {:.10e}\n", phase0_solution.cost);
    fmt::print("[STATS] Phase0_Time: {:.6f}\n", phase0_time.count());
    fmt::print("[STATS] Phase1_Iterations: {}\n", phase1_solver.iteration);
    fmt::print("[STATS] Phase1_Cost: {:.10e}\n", s.cost);
    fmt::print("[STATS] Objective_Cost: {:.10e}\n", true_cost);
    fmt::print("[STATS] Total_Time: {:.6f}\n", elapsed.count());

    return 0;
}
