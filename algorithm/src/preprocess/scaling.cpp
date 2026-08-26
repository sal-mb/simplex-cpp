#include "preprocess.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "constants.hpp"

namespace {

double compute_min_vector(const Eigen::VectorXd& v) {
    double result = std::numeric_limits<double>::max();
    for (long i = 0; i < v.size(); i++) {
        if (v(i) > EPSILON_1 && result > v(i)) {
            result = v(i);
        }
    }
    return result;
}

double compute_min_aij(const Eigen::MatrixXd& A_abs) {
    double result = std::numeric_limits<double>::max();
    for (long i = 0; i < A_abs.rows(); i++) {
        double row_min = compute_min_vector(A_abs.row(i));
        if (row_min < result) result = row_min;
    }
    return result;
}

std::pair<double, double> min_max_row(const Eigen::MatrixXd& A_abs, long i) {
    return {compute_min_vector(A_abs.row(i)), A_abs.row(i).maxCoeff()};
}

std::pair<double, double> min_max_col(const Eigen::MatrixXd& A_abs, long j) {
    return {compute_min_vector(A_abs.col(j)), A_abs.col(j).maxCoeff()};
}

std::pair<double, double> min_max_row_ratio(const Eigen::MatrixXd& A_abs) {
    double lo = pInf, hi = 0;
    for (long i = 0; i < A_abs.rows(); i++) {
        auto [mn, mx] = min_max_row(A_abs, i);
        double ratio = mx / mn;
        lo = std::min(lo, ratio);
        hi = std::max(hi, ratio);
    }
    return {lo, hi};
}

std::pair<double, double> min_max_col_ratio(const Eigen::MatrixXd& A_abs) {
    double lo = pInf, hi = 0;
    for (long j = 0; j < A_abs.cols(); j++) {
        auto [mn, mx] = min_max_col(A_abs, j);
        double ratio = mx / mn;
        lo = std::min(lo, ratio);
        hi = std::max(hi, ratio);
    }
    return {lo, hi};
}

// One row-scaling pass and one column-scaling pass (order controlled by
// flag), using A_abs computed by the caller just before this call.
void geometric_scale(ProblemData& data, const Eigen::MatrixXd& A_abs, int flag) {
    const long m = A_abs.rows();
    const long n = A_abs.cols();

    for (int pass = 0; pass < 2; pass++) {
        if (pass == flag) {
            for (long j = 0; j < m; j++) {
                auto [mn, mx] = min_max_row(A_abs, j);
                if (mx == 0) continue;
                double fac = 1.0 / std::sqrt(mn * mx);
                data.A.row(j) *= fac;
                data.b(j) *= fac;
            }
        } else {
            for (long j = 0; j < n; j++) {
                auto [mn, mx] = min_max_col(A_abs, j);
                if (mx == 0) continue;
                double r = std::sqrt(mn * mx);
                double fac = 1.0 / r;
                data.A.col(j) *= fac;
                data.c(j) *= fac;
                data.lb(j) *= r;
                data.ub(j) *= r;
            }
        }
    }
}

} // namespace

void apply_scaling(ProblemData& data) {
    if (data.m == 0 || data.n == 0) return;

    Eigen::MatrixXd A_abs = data.A.cwiseAbs();

    auto row_ratio = min_max_row_ratio(A_abs);
    auto col_ratio = min_max_col_ratio(A_abs);
    int flag = row_ratio.second > col_ratio.second;

    double min_A = compute_min_aij(A_abs);
    double max_A = A_abs.maxCoeff();
    double old_ratio;
    double ratio = 0;

    for (int i = 1; i <= 15; i++) {
        old_ratio = ratio;

        geometric_scale(data, A_abs, flag);

        // A_abs must be recomputed after every scaling pass: both the
        // convergence check below and the NEXT iteration's row/column
        // min/max need to reflect the matrix as it currently stands,
        // not the original unscaled one.
        A_abs = data.A.cwiseAbs();
        min_A = compute_min_aij(A_abs);
        max_A = A_abs.maxCoeff();
        ratio = max_A / min_A;

        if (i > 1 && ratio > 0.9 * old_ratio) {
            break;
        }
    }
}