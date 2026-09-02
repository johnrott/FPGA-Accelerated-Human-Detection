#include <iostream>
#include <cstdlib>
#include <cstring>

#include "svm_predict.h"

using namespace std;

// ------------------------------------------------------------
// Software reference version of score_window()
// This should match the optimized integer-scaled hardware version.
// ------------------------------------------------------------
static int score_window_sw(
    hist_t* all_hists,
    int     cell_row,
    int     cell_col
) {
    accum_t score = (accum_t)SVM_BIAS;

    int feat_idx = 0;

    for (int wr = 0; wr < WIN_CELLS_Y; wr++) {
        for (int wc = 0; wc < WIN_CELLS_X; wc++) {
            int full_cell = (cell_row + wr) * FULL_CELLS_X + (cell_col + wc);
            int hist_base = full_cell * NB_BINS;

            for (int b = 0; b < NB_BINS; b++) {
                score += (accum_t)SVM_WEIGHTS[feat_idx] *
                         (accum_t)all_hists[hist_base + b];

                feat_idx++;
            }
        }
    }

    return (score >= 0) ? 1 : 0;
}

// ------------------------------------------------------------
// Software reference version of full svm_predict()
// ------------------------------------------------------------
static int svm_predict_sw(hist_t* all_hists) {
    for (int wy = 0; wy < N_WINS_Y; wy++) {
        for (int wx = 0; wx < N_WINS_X; wx++) {
            int result = score_window_sw(
                all_hists,
                wy * STEP_CELLS,
                wx * STEP_CELLS
            );

            if (result == 1) {
                return 1;
            }
        }
    }

    return 0;
}

// ------------------------------------------------------------
// Fill histogram array with deterministic pseudo-random values
// ------------------------------------------------------------
static void fill_random_hists(hist_t* all_hists, int seed) {
    srand(seed);

    for (int i = 0; i < FULL_CELLS_Y * FULL_CELLS_X * NB_BINS; i++) {
        all_hists[i] = rand() % 16;
    }
}

// ------------------------------------------------------------
// Fill all histograms with zero
// ------------------------------------------------------------
static void fill_zero_hists(hist_t* all_hists) {
    for (int i = 0; i < FULL_CELLS_Y * FULL_CELLS_X * NB_BINS; i++) {
        all_hists[i] = 0;
    }
}

// ------------------------------------------------------------
// Fill all histograms with a constant value
// ------------------------------------------------------------
static void fill_constant_hists(hist_t* all_hists, hist_t value) {
    for (int i = 0; i < FULL_CELLS_Y * FULL_CELLS_X * NB_BINS; i++) {
        all_hists[i] = value;
    }
}

// ------------------------------------------------------------
// Run one test case
// ------------------------------------------------------------
static int run_test_case(const char* test_name, hist_t* all_hists) {
    int hw_result = -1;
    int hw_score = 0;
    int sw_result = svm_predict_sw(all_hists);

    svm_predict(all_hists, &hw_result, &hw_score);

    cout << "Hardware Score: " << hw_score << endl;

    cout << "Test: " << test_name << endl;
    cout << "  SW result: " << sw_result << endl;
    cout << "  HW result: " << hw_result << endl;

    if (hw_result != sw_result) {
        cout << "  FAILED" << endl;
        return 1;
    } else {
        cout << "  PASSED" << endl;
        return 0;
    }
}

int main() {
    cout << "========================================" << endl;
    cout << "SVM Predict HLS Testbench" << endl;
    cout << "========================================" << endl;

    const int TOTAL_HISTS = FULL_CELLS_Y * FULL_CELLS_X * NB_BINS;

    hist_t all_hists[TOTAL_HISTS];

    int errors = 0;

    // Test 1: all zeros
    fill_zero_hists(all_hists);
    errors += run_test_case("All zeros", all_hists);

    // Test 2: all ones
    fill_constant_hists(all_hists, 1);
    errors += run_test_case("All ones", all_hists);

    // Test 3: small constant
    fill_constant_hists(all_hists, 5);
    errors += run_test_case("All fives", all_hists);

    // Test 4: random case 1
    fill_random_hists(all_hists, 1);
    errors += run_test_case("Random seed 1", all_hists);

    // Test 5: random case 2
    fill_random_hists(all_hists, 2);
    errors += run_test_case("Random seed 2", all_hists);

    // Test 6: random case 3
    fill_random_hists(all_hists, 3);
    errors += run_test_case("Random seed 3", all_hists);

    cout << "========================================" << endl;

    if (errors == 0) {
        cout << "ALL TESTS PASSED" << endl;
        cout << "========================================" << endl;
        return 0;
    } else {
        cout << "TESTS FAILED: " << errors << endl;
        cout << "========================================" << endl;
        return 1;
    }
}