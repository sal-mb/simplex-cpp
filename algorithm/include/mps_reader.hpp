/*
 *  mps_reader.hpp
 *  cppipm
 *
 *  Created by Yiming Yan on 11/07/2014.
 *  Copyright (c) 2014 Yiming Yan. All rights reserved.
 *
 * =================================================================
 *
 * Problem format
 * min 1/2 x'Qx + c'x
 * s.t.
 *      Ax = b
 *      lb <= x <= ub
 *
 * Note that in order to have the above format, slack variables will
 * be added if needed.
 *
 * After call trans2standardForm() function, we get
 * min 1/2 x'Qx + c'x
 * s.t.
 *      Ax = b
 *      x >= 0
 *
 * =================================================================
 *
 * MpsReader's only job is parsing: it reads an MPS file and returns a
 * raw ProblemData (ProblemStage::Raw) with:
 *   - A sized m x n (just the original variables -- no slack columns)
 *   - b sized m, holding the true RHS value per row
 *   - c, lb, ub sized n (original variables only)
 *   - row_types sized m ('L'/'G'/'E' per row, in the same row order as A/b)
 *
 * It does NOT scale anything, remove fixed variables, or create slack
 * columns -- that is entirely the Preprocessor's job (see preprocess.hpp),
 * which turns this raw ProblemData into a ProblemStage::Preprocessed one
 * that Simplex can consume directly.
 *
 * =================================================================
 * Accepted format: mps, qps, free formatted mps, free formatted qps
 *
 * In the ROWS section, each row of the constraint matrix must have a
 * row type and a row name specified. The code for indicating row type
 * is as follows:
 *
 *      type        meaning
 * ---------------------------
 *      E           equality
 *      L           less than or equal
 *      G           greater than or equal
 *      N           objective
 *
 * *** N will only be recognised as objective function.
 *
 * RANGES and SOS are not accepted currently.
 *
 * For BOUNDS, we accept only
 *      type            meaning
 *  ---------------------------------------------------
 *      LO              lower bound        lb <= x (< +inf)
 *      UP              upper bound        (0 <=) x <= ub
 *      FR              free                -inf < x < +inf
 *      FX              fixed               x = value
 *
 * For details about MPS format, see
 *      http://lpsolve.sourceforge.net/5.5/mps-format.htm
 *
 */

#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "Eigen/Dense"
#include "Eigen/src/Core/Matrix.h"
#include "problem_data.hpp"

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;

class MpsReader {
   public:
    MpsReader() = default;

    // Parses file_name and returns a raw (ProblemStage::Raw) ProblemData.
    // Also populates this instance's row_list/col_list/row_labels, in
    // case the caller wants them for debugging (they are not part of
    // ProblemData itself).
    ProblemData read(const string& file_name);

    vector<string> row_labels;
    vector<string> row_list;
    vector<string> col_list;

   private:
    int n_rows{0};
    int n_rows_eq{0};
    int n_rows_inq{0};
    int n_cols{0};
    string name{"0"};

    long col_pos{0};
    long rhs_pos{0};
    long bnd_pos{0};

    bool bnd_exist{false};

    static void find_pos2_start(ifstream& read_file);
    void preproc_scan(ifstream& read_file);
    static int check_section_name(const string& check_word);
    static void next_line(ifstream& read_file);
    static int get_index(const vector<string>& list, const string& item);
    ProblemData extract_data(ifstream& read_file);
    void get_araw(ifstream& read_file, MatrixXd& Araw);
    void get_braw(ifstream& read_file, VectorXd& braw);
    void get_bnds(ifstream& read_file, VectorXd& lb, VectorXd& ub);

    // Pulls the objective (first "N" row) out of Araw into c, zeroing that
    // row and marking it "X" so split_raw() drops it from the constraint
    // matrix. Also captures the objective's constant term, if the MPS file
    // gives one via an RHS entry keyed on the objective row's name (see
    // mps-format.htm's note on the RHS section) -- 0 if none was given.
    void split_c(MatrixXd& Araw, const VectorXd& braw, VectorXd& c, double& obj_constant);

    // Compacts Araw/braw (n_rows x n_cols, including the now-zeroed
    // objective row) down to the m real constraint rows, dropping the
    // objective row entirely and recording each kept row's type.
    void split_raw(const MatrixXd& Araw, const VectorXd& braw, MatrixXd& A, VectorXd& b,
                   vector<char>& row_types);
};