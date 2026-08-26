#pragma once

#include "params.hpp"
#include "problem_data.hpp"

// ---------------------------------------------------------------------
// Preprocessing steps. Declared together here; each is implemented in
// its own .cpp file (fixed_variables.cpp, scaling.cpp, preprocess.cpp).
// ---------------------------------------------------------------------

struct LimitSummary {
    double sum = 0.0;
    int inf_count = 0;
    int inf_var_idx = -1;

    bool is_infinite() const { return inf_count > 0; }
};

// Turns a raw (ProblemStage::Raw) ProblemData -- as returned directly by
// MpsReader::read() -- into a ProblemStage::Preprocessed one that
// Simplex can consume: optionally eliminates fixed original variables,
// optionally applies geometric scaling, then always appends the slack
// columns (one per row) that make A square-basis-ready.
// Implemented in preprocess.cpp.
class Preprocessor {
   public:
    Preprocessor(const Params& p, ProblemData data);

    const Params& p;
    ProblemData data;

    LimitSummary compute_lower_limit(int row_idx) const;
    LimitSummary compute_upper_limit(int row_idx) const;

    void rebuild_rows(const std::vector<int>& keep_rows);
    void rebuild_columns(const std::vector<int>& keep_cols);
    void check_contradicting_bounds();
    void tighten_individual_bounds();

    void remove_empty_rows();
    void remove_empty_columns();
    void remove_singleton_rows();
    void remove_redundant_forcing_constraints();
    // Applies the same iterative geometric-mean scaling as the original
    // Scaling class, in place, to raw (ProblemStage::Raw) data: A, b, c,
    // lb, ub. Alternates row/column passes (row-or-col chosen first based
    // on whichever currently has the worse min/max ratio), for up to 15
    // iterations, stopping early once the matrix's overall min/max ratio
    // stops improving by more than 10% per iteration.
    // Implemented in scaling.cpp.
    void apply_scaling();

    // Eliminates every original-variable column j (0 <= j < data.n) with
    // lb(j) == ub(j): it is fixed at value v = lb(j) = ub(j), its
    // contribution is folded into b (b -= A.col(j) * v) and into
    // obj_constant (+= c(j) * v), and the column is dropped from A, c, lb,
    // ub, and n.
    //
    // Must be called on raw (ProblemStage::Raw) data, before slack columns
    // exist -- it only ever looks at columns [0, data.n), so it is
    // structurally impossible for it to touch a slack column even after
    // slack creation, but it is intended to run before that point.
    // Implemented in fixed_variables.cpp.
    void remove_fixed_variables();

    // Appends one slack column per row to A (giving it its final
    // m x (n + m) shape) and extends c/lb/ub to match, then zeroes b. This
    // is always the last preprocessing step: everything before it
    // (remove_fixed_variables, apply_scaling) only ever touches the raw
    // m x n data, so slack columns are never at risk of being removed or
    // independently rescaled.
    //
    // Convention (matching the original mps_reader): each slack has
    // coefficient -1 in its own row and 0 elsewhere, so Ax_ext = 0. The
    // row's true RHS is folded entirely into that slack's bounds instead
    // of staying in b:
    //   L (<=):  Ax <= b   ->  s = Ax,  lb(s) = -inf,  ub(s) = b
    //   G (>=):  Ax >= b   ->  s = Ax,  lb(s) = b,      ub(s) = +inf
    //   E (=):   Ax  = b   ->  s = Ax,  lb(s) = ub(s) = b
    // Implemented in preprocess.cpp.
    void create_slack_variables();

    ProblemData process();

    bool changed = false;
};
