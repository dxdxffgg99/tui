#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline int tui_get_size(unsigned short *rows, unsigned short *cols) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) return -1;
    *cols = (unsigned short)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    *rows = (unsigned short)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
    return 0;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) return -1;
    *rows = w.ws_row;
    *cols = w.ws_col;
    return 0;
#endif
}

typedef struct {
    size_t rows, cols;
    char *cells;
} tui_frame_t;


static inline char tui_sanitize_ascii(char ch) {
    if ((unsigned char)ch < 32 || (unsigned char)ch > 126) return '?';
    return ch;
}

static inline tui_frame_t *tui_frame_new(size_t rows, size_t cols) {
    if (rows == 0 || cols == 0 || cols > SIZE_MAX / rows) return NULL;
    size_t total = rows * cols;
    tui_frame_t *f = malloc(sizeof *f);
    if (!f) return NULL;
    f->rows = rows;
    f->cols = cols;
    f->cells = calloc(total, sizeof(char));
    if (!f->cells) { free(f); return NULL; }
    return f;
}


static inline void tui_frame_free(tui_frame_t *f) {
    if (!f) return;
    free(f->cells);
    free(f);
}


static inline int tui_frame_set_char(tui_frame_t *f, size_t r, size_t c, char ch) {
    if (!f || r >= f->rows || c >= f->cols) return -1;
    f->cells[r * f->cols + c] = tui_sanitize_ascii(ch);
    return 0;
}


static inline char tui_frame_get_char(tui_frame_t *f, size_t r, size_t c) {
    if (!f || r >= f->rows || c >= f->cols) return '\0';
    return f->cells[r * f->cols + c];
}


static inline void tui_frame_clear(tui_frame_t *f, char ch) {
    if (!f) return;
    memset(f->cells, (unsigned char)ch, f->rows * f->cols);
}


static inline int tui_frame_resize(tui_frame_t *f, size_t rows, size_t cols) {
    if (!f || rows == 0 || cols == 0 || cols > SIZE_MAX / rows) return -1;
    size_t total = rows * cols;
    char *newcells = calloc(total, sizeof(char));
    if (!newcells) return -1;
    size_t minr = rows < f->rows ? rows : f->rows;
    size_t minc = cols < f->cols ? cols : f->cols;
    for (size_t r = 0; r < minr; ++r) {
        memcpy(newcells + r * cols, f->cells + r * f->cols, minc);
    }
    free(f->cells);
    f->cells = newcells;
    f->rows = rows;
    f->cols = cols;
    return 0;
}


static inline void tui_frame_draw(FILE *out, const tui_frame_t *f) {
    if (!f || !out) return;
    for (size_t r = 0; r < f->rows; ++r) {
        fwrite(f->cells + r * f->cols, 1, f->cols, out);
        fputc('\n', out);
    }
    fflush(out);
}


static inline void tui_ansi_move_cursor(FILE *out, size_t r, size_t c) {
    if (!out) return;
    fprintf(out, "\x1b[%zu;%zuH", r + 1, c + 1);
}

static inline void tui_ansi_clear_screen(FILE *out) {
    if (!out) return;
    fprintf(out, "\x1b[2J\x1b[H");
    fflush(out);
}

static inline void tui_ansi_hide_cursor(FILE *out) {
    if (!out) return;
    fprintf(out, "\x1b[?25l");
    fflush(out);
}

static inline void tui_ansi_show_cursor(FILE *out) {
    if (!out) return;
    fprintf(out, "\x1b[?25h");
    fflush(out);
}


#ifndef _WIN32
static inline void tui_disable_raw_mode(void);

static struct termios tui_orig_termios;
static int tui_termios_saved = 0;


static inline int tui_enable_raw_mode(void) {
    struct termios raw;
    if (tui_termios_saved) return 0;
    if (!isatty(STDIN_FILENO)) return -1;
    if (tcgetattr(STDIN_FILENO, &tui_orig_termios) == -1) return -1;
    raw = tui_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return -1;
    tui_termios_saved = 1;
    atexit(tui_disable_raw_mode);
    return 0;
}

static inline void tui_disable_raw_mode(void) {
    if (!tui_termios_saved) return;
    if (!isatty(STDIN_FILENO)) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tui_orig_termios);
    tui_termios_saved = 0;
}
#endif

#ifdef _WIN32
static inline void tui_disable_raw_mode(void);

static DWORD tui_orig_mode;
static int tui_mode_saved = 0;


static inline int tui_enable_raw_mode(void) {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return -1;
    if (!tui_mode_saved) {
        tui_orig_mode = mode;
        tui_mode_saved = 1;
        atexit(tui_disable_raw_mode);
    }
    mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    if (!SetConsoleMode(h, mode)) return -1;
    return 0;
}

static inline void tui_disable_raw_mode(void) {
    if (!tui_mode_saved) return;
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return;
    SetConsoleMode(h, tui_orig_mode);
    tui_mode_saved = 0;
}
#endif


static inline int tui_poll_key(int timeout_ms) {
#ifndef _WIN32
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rv = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);

    if (rv <= 0) return -1;
    char ch = 0;
    ssize_t n = read(STDIN_FILENO, &ch, 1);

    if (n <= 0) return -1;
    return (unsigned char)ch;
#else
    if (timeout_ms < 0) timeout_ms = INFINITE;

    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);

    if (h == INVALID_HANDLE_VALUE) return -1;

    DWORD wait = WaitForSingleObject(h, (DWORD)timeout_ms);

    if (wait != WAIT_OBJECT_0) return -1;
    if (!_kbhit()) return -1;

    return (unsigned char)_getch();
#endif
}


static volatile sig_atomic_t tui_resize_flag = 0;
static inline void tui_sigwinch_handler(int unused) { (void)unused; tui_resize_flag = 1; }

static inline void tui_install_resize_handler(void) {
#ifndef _WIN32
#ifdef SIGWINCH
    signal(SIGWINCH, tui_sigwinch_handler);
#endif
#endif
}

typedef struct {
    FILE *out;
    tui_frame_t *front;
    tui_frame_t *back;
} tui_ctx_t;


static inline tui_ctx_t *tui_ctx_new(FILE *out) {
    unsigned short rows = 0, cols = 0;

    if (tui_get_size(&rows, &cols) == -1) {
        rows = 24;
        cols = 80;
    }

    tui_ctx_t *ctx = malloc(sizeof *ctx);

    if (!ctx) return NULL;

    ctx->out = out ? out : stdout;
    ctx->front = tui_frame_new(rows, cols);
    ctx->back = tui_frame_new(rows, cols);

    if (!ctx->front || !ctx->back) {
        tui_frame_free(ctx->front);
        tui_frame_free(ctx->back);
        free(ctx);
        return NULL;
    }

    tui_frame_clear(ctx->front, ' ');
    tui_frame_clear(ctx->back, ' ');
    tui_enable_raw_mode();
    tui_install_resize_handler();

    return ctx;
}


static inline void tui_ctx_free(tui_ctx_t *ctx) {
    if (!ctx) return;

    tui_frame_free(ctx->front);
    tui_frame_free(ctx->back);
    free(ctx);
}


static inline int tui_ctx_handle_resize(tui_ctx_t *ctx) {
    if (!ctx) return -1;
    if (!tui_resize_flag) return 0;

    tui_resize_flag = 0;
    unsigned short rows = 0, cols = 0;

    if (tui_get_size(&rows, &cols) == -1) return -1;
    if (tui_frame_resize(ctx->front, rows, cols) == -1) return -1;
    if (tui_frame_resize(ctx->back, rows, cols) == -1) return -1;

    tui_frame_clear(ctx->back, ' ');
    memset(ctx->front->cells, 0, ctx->front->rows * ctx->front->cols);

    return 1;
}


static inline void tui_present(tui_ctx_t *ctx) {
    if (!ctx || !ctx->out) return;

    tui_frame_t *f = ctx->back;
    tui_frame_t *g = ctx->front;

    if (!f || !g) return;

    size_t rows = f->rows, cols = f->cols;

    for (size_t r = 0; r < rows; ++r) {
        size_t c = 0;
        while (c < cols) {
            char b = g->cells[r * cols + c];
            if (f->cells[r * cols + c] == b) {
                ++c;
                continue;
            }
            size_t start = c;
            while (c < cols && f->cells[r * cols + c] != g->cells[r * cols + c]) ++c;
            size_t len = c - start;
            tui_ansi_move_cursor(ctx->out, r, start);
            for (size_t i = 0; i < len; ++i) {
                char ch = tui_sanitize_ascii(f->cells[r * cols + start + i]);
                fputc(ch, ctx->out);
                g->cells[r * cols + start + i] = ch;
            }
        }
    }
    
    fflush(ctx->out);
}

#ifdef __cplusplus
}
#endif