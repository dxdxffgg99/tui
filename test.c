#include "inc/tui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    tui_ctx_t *ctx = tui_ctx_new(stdout);
    if (!ctx) return 1;

    tui_ansi_hide_cursor(ctx->out);
    tui_ansi_clear_screen(ctx->out);

    size_t rows = ctx->back->rows;
    size_t cols = ctx->back->cols;
    size_t cur_r = rows / 2, cur_c = cols / 2;

    const char *msg = "TUIlib (w/a/s/d move, q quit) [very experimental text]";

    int running = 1;

    while (running) {
        if (tui_ctx_handle_resize(ctx) == 1) {
            rows = ctx->back->rows; cols = ctx->back->cols;
            if (cur_r >= rows) cur_r = rows - 1;
            if (cur_c >= cols) cur_c = cols - 1;
        }

        tui_frame_clear(ctx->back, ' ');

        size_t r = rows / 2;
        size_t c = (cols > strlen(msg)) ? (cols - strlen(msg)) / 2 : 0;

        for (size_t i = 0; i < strlen(msg) && (c + i) < ctx->back->cols; ++i) tui_frame_set_char(ctx->back, r, c + i, msg[i]);

        tui_frame_set_char(ctx->back, cur_r, cur_c, '#');

        tui_present(ctx);

        int k = tui_poll_key(200);
        if (k == -1) continue;
        if (k == 'q' || k == 'Q') { running = 0; break; }
        switch (k) {
            case 'a': if (cur_c > 0) cur_c--; break;
            case 'd': if (cur_c + 1 < cols) cur_c++; break;
            case 'w': if (cur_r > 0) cur_r--; break;
            case 's': if (cur_r + 1 < rows) cur_r++; break;
            default: break;
        }
    }

    tui_ansi_show_cursor(ctx->out);
    tui_ansi_clear_screen(ctx->out);
    tui_ctx_free(ctx);

    return 0;
}
