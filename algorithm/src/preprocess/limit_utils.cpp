#include <cmath>

#include "constants.hpp"
#include "preprocess.hpp"

// Lower limit L_i = sum_{j in P_i} a_ij * l_j + sum_{j in M_i} a_ij * u_j
LimitSummary compute_lower_limit(const ProblemData& data, const Params& p, int row_idx) {
    LimitSummary limit;
    for (int j = 0; j < data.n; ++j) {
        double a_ij = data.A(row_idx, j);
        if (std::abs(a_ij) <= p.eps) continue;

        if (a_ij > 0.0) { // P_i set: positive coefficients
            if (data.lb(j) <= nInf / 2.0) {
                limit.inf_count++;
                limit.inf_var_idx = j;
            } else {
                limit.sum += a_ij * data.lb(j);
            }
        } else { // M_i set: negative coefficients
            if (data.ub(j) >= pInf / 2.0) {
                limit.inf_count++;
                limit.inf_var_idx = j;
            } else {
                limit.sum += a_ij * data.ub(j);
            }
        }
    }
    return limit;
}

// Upper limit U_i = sum_{j in P_i} a_ij * u_j + sum_{j in M_i} a_ij * l_j
LimitSummary compute_upper_limit(const ProblemData& data, const Params& p, int row_idx) {
    LimitSummary limit;
    for (int j = 0; j < data.n; ++j) {
        double a_ij = data.A(row_idx, j);
        if (std::abs(a_ij) <= p.eps) continue;

        if (a_ij > 0.0) { // P_i set: positive coefficients
            if (data.ub(j) >= pInf / 2.0) {
                limit.inf_count++;
                limit.inf_var_idx = j;
            } else {
                limit.sum += a_ij * data.ub(j);
            }
        } else { // M_i set: negative coefficients
            if (data.lb(j) <= nInf / 2.0) {
                limit.inf_count++;
                limit.inf_var_idx = j;
            } else {
                limit.sum += a_ij * data.lb(j);
            }
        }
    }
    return limit;
}
