#include "svm_predict.h"

static accum_t score_window_local(
    hist_t local_hists[HIST_DEPTH],
    int    cell_row,
    int    cell_col
) {
#pragma HLS INLINE

    accum_t score = (accum_t)SVM_BIAS;
    int feat_idx = 0;

    win_row: for (int wr = 0; wr < WIN_CELLS_Y; wr++) {
        win_col: for (int wc = 0; wc < WIN_CELLS_X; wc++) {
#pragma HLS PIPELINE II=1

            int full_cell = (cell_row + wr) * FULL_CELLS_X + (cell_col + wc);
            int hist_base = full_cell * NB_BINS;

            accum_t bin_sum = 0;

            win_bin: for (int b = 0; b < NB_BINS; b++) {
#pragma HLS UNROLL
                accum_t w = (accum_t)SVM_WEIGHTS[feat_idx + b];
                accum_t x = (accum_t)local_hists[hist_base + b];

                bin_sum += w * x;
            }

            score += bin_sum;
            feat_idx += NB_BINS;
        }
    }

    return score;
}

void svm_predict(
    hist_t* all_hists,
    int*    led_out,
    int*    score_out
) {
#pragma HLS ARRAY_PARTITION variable=SVM_WEIGHTS cyclic factor=9 dim=1

#pragma HLS INTERFACE m_axi port=all_hists  offset=slave bundle=gmem0 depth=HIST_DEPTH max_read_burst_length=64 num_read_outstanding=16
#pragma HLS INTERFACE m_axi port=led_out    offset=slave bundle=gmem1 depth=1
#pragma HLS INTERFACE m_axi port=score_out  offset=slave bundle=gmem2 depth=1

#pragma HLS INTERFACE s_axilite port=all_hists bundle=CTRL
#pragma HLS INTERFACE s_axilite port=led_out   bundle=CTRL
#pragma HLS INTERFACE s_axilite port=score_out bundle=CTRL
#pragma HLS INTERFACE s_axilite port=return    bundle=CTRL

    hist_t local_hists[HIST_DEPTH];
#pragma HLS ARRAY_PARTITION variable=local_hists cyclic factor=9 dim=1

    load_hists: for (int i = 0; i < HIST_DEPTH; i++) {
#pragma HLS PIPELINE II=1
        local_hists[i] = all_hists[i];
    }

    accum_t best_score = -140737488355328LL;
    int detected = 0;

    scan_y: for (int wy = 0; wy < N_WINS_Y; wy++) {
        scan_x: for (int wx = 0; wx < N_WINS_X; wx++) {
#pragma HLS LOOP_FLATTEN off

            int cell_row = wy * STEP_CELLS;
            int cell_col = wx * STEP_CELLS;

            accum_t score = score_window_local(
                local_hists,
                cell_row,
                cell_col
            );

            if (score > best_score) {
                best_score = score;
            }

            if (score >= 0) {
                detected = 1;
            }
        }
    }

    led_out[0] = detected;

    score_out[0] = (int)best_score;
}