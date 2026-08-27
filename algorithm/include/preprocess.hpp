#pragma once

#include <vector>

#include "params.hpp"
#include "problem_data.hpp"

// ---------------------------------------------------------------------
// Preprocessing steps. Declared together here; each is implemented in
// its own .cpp file (fixed_variables.cpp, scaling.cpp, preprocess.cpp,
// rows_and_columns.cpp, redundant_constraints.cpp, bounds.cpp,
// limit_utils.cpp).
// ---------------------------------------------------------------------

struct LimitSummary {
    double sum = 0.0;
    int inf_count = 0;
    int inf_var_idx = -1;

    bool is_infinite() const { return inf_count > 0; }
};

LimitSummary compute_lower_limit(const ProblemData& data, const Params& p, int row_idx);
LimitSummary compute_upper_limit(const ProblemData& data, const Params& p, int row_idx);

// Shared row/column-elimination helpers used by several techniques below.
// Compact A/b/row_types (or A/c/lb/ub) down to the given kept indices.
// Return true iff the row/column count actually shrank.
bool rebuild_rows(ProblemData& data, const std::vector<int>& keep_rows);
bool rebuild_columns(ProblemData& data, const std::vector<int>& keep_cols);

// Throws if any lb(j) > ub(j) + eps for an original variable j (0 <= j <
// data.n). Run once before presolve starts and again after every pass:
// several techniques below (forcing constraints, bound tightening) can
// turn a previously-consistent bound pair into a contradiction if the
// instance is genuinely infeasible, and that should be caught as soon as
// it happens rather than only at the very start.
void check_contradicting_bounds(const ProblemData& data, const Params& p);

// ---------------------------------------------------------------------
// Individual presolve techniques.
//
// Each takes the current (raw-stage) ProblemData and Params, mutates
// `data` in place, and reports what it did via PresolveResult. They are
// meant to be run in a loop (see Preprocessor::process()) until none of
// them report `changed` or a pass limit is hit.
// ---------------------------------------------------------------------

// Outcome of a single presolve technique's pass over the problem: whether
// it changed anything (drives the outer convergence loop) and how many
// rows/columns it eliminated (for reporting/diagnostics).
struct PresolveResult {
    bool changed = false;
    int rows_removed = 0;
    int cols_removed = 0;
};

// Applies equations (7.19)/(7.20) (finite-limit case) and the Gondzio
// extension (7.23)/(7.24) (single-infinite-bound case) to tighten
// individual variable bounds from each row's achievable [L_i, U_i] range.
// Implemented in bounds.cpp.
PresolveResult tighten_individual_bounds(ProblemData& data, const Params& p);

// Drops rows with every structural coefficient zero, after checking the
// (now purely constant) row for infeasibility. Implemented in
// rows_and_columns.cpp.
PresolveResult remove_empty_rows(ProblemData& data, const Params& p);

// Fixes columns with every structural coefficient zero at whichever bound
// minimizes c_j * x_j (this is a minimization problem -- see
// mps_reader.hpp), folding the contribution into obj_constant, and drops
// the column. Implemented in rows_and_columns.cpp.
PresolveResult remove_empty_columns(ProblemData& data, const Params& p);

// Folds each singleton row's single nonzero coefficient into a tightened
// bound on its one variable, then drops the row. Implemented in
// rows_and_columns.cpp.
PresolveResult remove_singleton_rows(ProblemData& data, const Params& p);

// Classifies each row against its achievable [L_i, U_i] range: infeasible
// (b_i outside the range) throws, forcing (b_i at one end) fixes every
// variable touching the row to the bound that achieves that end and
// drops the row, redundant (b_i beyond the far end) just drops the row.
// Implemented in redundant_constraints.cpp.
PresolveResult remove_redundant_forcing_constraints(ProblemData& data, const Params& p);

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
PresolveResult remove_fixed_variables(ProblemData& data, const Params& p);

// ---------------------------------------------------------------------
// One-shot steps: not part of the iterative technique pipeline above,
// run exactly once by Preprocessor::process() after it converges.
// ---------------------------------------------------------------------

// Applies the same iterative geometric-mean scaling as the original
// Scaling class, in place, to raw (ProblemStage::Raw) data: A, b, c, lb,
// ub. Alternates row/column passes (row-or-col chosen first based on
// whichever currently has the worse min/max ratio), for up to 15
// iterations, stopping early once the matrix's overall min/max ratio
// stops improving by more than 10% per iteration.
// Implemented in scaling.cpp.
void apply_scaling(ProblemData& data, const Params& p);

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
void create_slack_variables(ProblemData& data);

// ---------------------------------------------------------------------
// Pipeline registration
// ---------------------------------------------------------------------

// A single registered presolve technique: its name, which
// ProblemData::*_removed flag to set when it runs (nullptr if none
// applies), whether it's enabled for a given run, and the technique
// itself.
//
// Deliberately plain function pointers rather than std::function or a
// virtual interface + unique_ptr: the set of techniques is fixed at
// compile time (there is no dynamic loading/plugin story here), so a
// PresolveTechnique is just a trivially-copyable bundle of pointers with
// no ownership to manage, and the pipeline itself is a plain
// std::array<PresolveTechnique, N> (see preprocess.cpp) rather than a
// vector of owned polymorphic objects. Adding a new technique never
// requires touching this struct or the loop that walks the array --
// only: (1) write `PresolveResult my_technique(ProblemData&, const
// Params&)` in its own .cpp file, (2) declare it above, (3) add one
// line to the array in preprocess.cpp.
struct PresolveTechnique {
    const char* name;
    bool ProblemData::* removed_flag;
    bool (*enabled)(const Params&);
    PresolveResult (*apply)(ProblemData&, const Params&);
};

// Turns a raw (ProblemStage::Raw) ProblemData -- as returned directly by
// MpsReader::read() -- into a ProblemStage::Preprocessed one that
// Simplex can consume: iterates the presolve technique pipeline to
// convergence, optionally scales, then always appends slack columns.
// Implemented in preprocess.cpp.
class Preprocessor {
   public:
    Preprocessor(const Params& p, ProblemData data);

    const Params& p;
    ProblemData data;

    ProblemData process();
};
