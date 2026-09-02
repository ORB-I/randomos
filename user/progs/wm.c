#include <fbdraw.h>
#include <fb.h>
#include <kbd.h>
#include <mouse.h>
#include <mem.h>
#include <io.h>
#include <sys/sysfn.h>
#include <sys/syscall.h>
#include <str.h>
#include <fs.h>
#include <time.h>
#include <stdbool.h>

#define WIN_MAX 8
#define TITLE_H 24
#define BAR_H 28

#define COLOR_DESKTOP_TOP  0xFF1A1B26
#define COLOR_DESKTOP_BOT  0xFF0F101A
#define COLOR_BAR_BG       0xFF24283B
#define COLOR_BAR_BORDER   0xFF414868
#define COLOR_MENU_BG      0xFF1F2335
#define COLOR_MENU_BORDER  0xFF565F89
#define COLOR_MENU_HOVER   0xFF3D4668
#define COLOR_WIN_BG       0xFF1A1B26
#define COLOR_WIN_TITLE    0xFF24283B
#define COLOR_WIN_TITLE_ACT 0xFF343B58
#define COLOR_WIN_BORDER   0xFF414868
#define COLOR_WIN_BORDER_ACT 0xFF7AA2F7
#define COLOR_TEXT         0xFFC0CAF5
#define COLOR_TEXT_DIM     0xFF787C99
#define COLOR_BTN_CLOSE    0xFFF7768E
#define COLOR_BTN_MIN      0xFFE0AF68
#define COLOR_ACCENT       0xFF7AA2F7
#define COLOR_GREEN        0xFF9ECE6A
#define COLOR_YELLOW       0xFFE0AF68
#define COLOR_CYAN         0xFF7DCFFF
#define COLOR_WHITE        0xFFFFFFFF
#define COLOR_BLACK        0xFF000000

typedef enum {
    APP_TERMINAL = 1,
    APP_FILES    = 2,
    APP_SYSINFO  = 3,
    APP_NOTES    = 4,
    APP_CALC     = 5,
    APP_ABOUT    = 6,
} app_type_t;

static void wm_strcpy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void wm_strncpy(char* dst, const char* src, usize n) {
    if (n == 0) return;
    usize i = 0;
    while (i + 1 < n && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void wm_strcat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static int wm_strncmp(const char* s1, const char* s2, usize n) {
    for (usize i = 0; i < n; i++) {
        if ((u8)s1[i] != (u8)s2[i] || s1[i] == '\0') {
            return (int)((u8)s1[i] - (u8)s2[i]);
        }
    }
    return 0;
}

typedef struct {
    char lines[16][48];
    int line_count;
    char input[48];
    int input_len;
    char cwd[64];
} term_state_t;

typedef struct {
    char current_path[128];
    char file_names[32][64];
    bool is_dir[32];
    usize file_sizes[32];
    int file_count;
    int selected_idx;
    char preview[128];
} files_state_t;

typedef struct {
    char text[8][40];
    int cursor_row;
    int cursor_col;
} notes_state_t;

typedef struct {
    char display[24];
    s64 acc;
    char op;
    bool clear_on_next;
} calc_state_t;

typedef struct {
    bool open;
    bool minimized;
    int id;
    int x, y;
    int w, h;
    char title[64];
    app_type_t app_type;
    u32* buf;
    union {
        term_state_t term;
        files_state_t files;
        notes_state_t notes;
        calc_state_t calc;
    } state;
} win_t;

static win_t wins[WIN_MAX];
static int win_order[WIN_MAX];
static int win_count = 0;

static u32* gui_fb = NULL;
static u32* backbuf = NULL;
static u64 gui_w = 0, gui_h = 0;
static u64 gui_pitch_px = 0;
static int gui_fb_id = -1;
static int term_fb = -1;

static int mouse_x = 100, mouse_y = 100;
static u8 mouse_prev_btns = 0;

static int drag_win = -1;
static int drag_off_x = 0, drag_off_y = 0;

static bool menu_open = false;
static u64 start_time_ms = 0;

/* --- Drawing Primitives --- */

static inline void put_pixel(u32* buf, int pitch, int x, int y, u32 col) {
    if (x >= 0 && x < (int)gui_w && y >= 0 && y < (int)gui_h) {
        buf[y * pitch + x] = col;
    }
}

static void fill_rect(u32* buf, int pitch, int x, int y, int w, int h, u32 col) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)gui_w) w = (int)gui_w - x;
    if (y + h > (int)gui_h) h = (int)gui_h - y;
    if (w <= 0 || h <= 0) return;

    for (int j = 0; j < h; j++) {
        u32* row = &buf[(y + j) * pitch + x];
        for (int i = 0; i < w; i++) {
            row[i] = col;
        }
    }
}

static void draw_rect(u32* buf, int pitch, int x, int y, int w, int h, u32 col) {
    fill_rect(buf, pitch, x, y, w, 1, col);
    fill_rect(buf, pitch, x, y + h - 1, w, 1, col);
    fill_rect(buf, pitch, x, y, 1, h, col);
    fill_rect(buf, pitch, x + w - 1, y, 1, h, col);
}

static void draw_gradient_v(u32* buf, int pitch, int x, int y, int w, int h, u32 c1, u32 c2) {
    if (w <= 0 || h <= 0) return;
    u8 r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    u8 r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;

    for (int j = 0; j < h; j++) {
        if (y + j < 0 || y + j >= (int)gui_h) continue;
        u8 r = (u8)(r1 + (r2 - r1) * j / h);
        u8 g = (u8)(g1 + (g2 - g1) * j / h);
        u8 b = (u8)(b1 + (b2 - b1) * j / h);
        u32 col = 0xFF000000 | ((u32)r << 16) | ((u32)g << 8) | b;

        int sx = (x < 0) ? 0 : x;
        int ex = (x + w > (int)gui_w) ? (int)gui_w : x + w;
        u32* row = &buf[(y + j) * pitch + sx];
        for (int i = 0; i < ex - sx; i++) {
            row[i] = col;
        }
    }
}

static void draw_char(u32* buf, int pitch, int x, int y, char c, u32 fg, u32 bg, bool transparent) {
    if ((u8)c < GUI_FONT_FIRST) c = ' ';
    const u8* rows = gui_font[(u8)c];

    for (int gy = 0; gy < GUI_FONT_H; gy++) {
        int py = y + gy;
        if (py < 0 || py >= (int)gui_h) continue;
        u8 byte = rows[gy];
        for (int gx = 0; gx < GUI_FONT_W; gx++) {
            int px = x + gx;
            if (px < 0 || px >= (int)gui_w) continue;
            if (byte & (1 << (7 - gx))) {
                buf[py * pitch + px] = fg;
            } else if (!transparent) {
                buf[py * pitch + px] = bg;
            }
        }
    }
}

static void draw_str(u32* buf, int pitch, int x, int y, const char* str, u32 fg, u32 bg, bool transparent) {
    if (!str) return;
    int cx = x;
    for (int i = 0; str[i]; i++) {
        if (str[i] == '\n') {
            cx = x;
            y += GUI_FONT_H;
            continue;
        }
        draw_char(buf, pitch, cx, y, str[i], fg, bg, transparent);
        cx += GUI_FONT_W;
    }
}

/* --- Window Buffer Helpers --- */

static void win_fill(win_t* w, int x, int y, int bw, int bh, u32 col) {
    if (!w->buf) return;
    if (x < 0) { bw += x; x = 0; }
    if (y < 0) { bh += y; y = 0; }
    if (x + bw > w->w) bw = w->w - x;
    if (y + bh > w->h) bh = w->h - y;
    if (bw <= 0 || bh <= 0) return;

    for (int j = 0; j < bh; j++) {
        u32* row = &w->buf[(y + j) * w->w + x];
        for (int i = 0; i < bw; i++) row[i] = col;
    }
}

static void win_draw_str(win_t* w, int x, int y, const char* str, u32 fg, u32 bg, bool transparent) {
    if (!w->buf || !str) return;
    int cx = x;
    for (int i = 0; str[i]; i++) {
        if (str[i] == '\n') {
            cx = x;
            y += GUI_FONT_H;
            continue;
        }
        if ((u8)str[i] >= GUI_FONT_FIRST) {
            const u8* rows = gui_font[(u8)str[i]];
            for (int gy = 0; gy < GUI_FONT_H; gy++) {
                int py = y + gy;
                if (py < 0 || py >= w->h) continue;
                u8 byte = rows[gy];
                for (int gx = 0; gx < GUI_FONT_W; gx++) {
                    int px = cx + gx;
                    if (px < 0 || px >= w->w) continue;
                    if (byte & (1 << (7 - gx))) {
                        w->buf[py * w->w + px] = fg;
                    } else if (!transparent) {
                        w->buf[py * w->w + px] = bg;
                    }
                }
            }
        }
        cx += GUI_FONT_W;
    }
}

/* --- Mouse Cursor --- */

static const char* cursor_sprite[18] = {
    "X...........",
    "XX..........",
    "X.X.........",
    "X..X........",
    "X...X.......",
    "X....X......",
    "X.....X.....",
    "X......X....",
    "X.......X...",
    "X........X..",
    "X.....XXXXXX",
    "X..X..X.....",
    "X.X.X..X....",
    "XX..X..X....",
    "X....X..X...",
    ".....X..X...",
    "......XX....",
    "............"
};

static void draw_mouse_cursor(u32* buf, int pitch, int mx, int my) {
    for (int j = 0; j < 18; j++) {
        for (int i = 0; i < 12; i++) {
            char p = cursor_sprite[j][i];
            if (p == 'X') {
                put_pixel(buf, pitch, mx + i, my + j, COLOR_BLACK);
            } else if (p == '.') {
                put_pixel(buf, pitch, mx + i, my + j, COLOR_WHITE);
            }
        }
    }
}

/* --- Keyboard Scancode Translation --- */

static char scancode_to_ascii(u8 sc, bool shift) {
    if (sc == 0x39) return ' ';
    if (sc == 0x0E) return '\b';
    if (sc == 0x1C) return '\n';
    if (sc == 0x0C) return shift ? '_' : '-';
    if (sc == 0x0D) return shift ? '+' : '=';
    if (sc == 0x1A) return shift ? '{' : '[';
    if (sc == 0x1B) return shift ? '}' : ']';
    if (sc == 0x27) return shift ? ':' : ';';
    if (sc == 0x28) return shift ? '"' : '\'';
    if (sc == 0x29) return shift ? '~' : '`';
    if (sc == 0x2B) return shift ? '|' : '\\';
    if (sc == 0x33) return shift ? '<' : ',';
    if (sc == 0x34) return shift ? '>' : '.';
    if (sc == 0x35) return shift ? '?' : '/';

    static const char num_normal[] = "1234567890";
    static const char num_shift[]  = "!@#$%^&*()";
    if (sc >= 0x02 && sc <= 0x0B) {
        return shift ? num_shift[sc - 0x02] : num_normal[sc - 0x02];
    }

    static const char row1[] = "qwertyuiop";
    if (sc >= 0x10 && sc <= 0x19) {
        char ch = row1[sc - 0x10];
        return shift ? (ch - 32) : ch;
    }
    static const char row2[] = "asdfghjkl";
    if (sc >= 0x1E && sc <= 0x26) {
        char ch = row2[sc - 0x1E];
        return shift ? (ch - 32) : ch;
    }
    static const char row3[] = "zxcvbnm";
    if (sc >= 0x2C && sc <= 0x32) {
        char ch = row3[sc - 0x2C];
        return shift ? (ch - 32) : ch;
    }

    return 0;
}

/* --- Window Z-Order & Management --- */

static void wm_bring_to_front(int idx) {
    int pos = -1;
    for (int i = 0; i < win_count; i++) {
        if (win_order[i] == idx) {
            pos = i;
            break;
        }
    }
    if (pos >= 0 && pos < win_count - 1) {
        int tmp = win_order[pos];
        for (int i = pos; i < win_count - 1; i++) {
            win_order[i] = win_order[i + 1];
        }
        win_order[win_count - 1] = tmp;
    }
}

static int wm_get_active_win(void) {
    for (int i = win_count - 1; i >= 0; i--) {
        int id = win_order[i];
        if (wins[id].open && !wins[id].minimized) return id;
    }
    return -1;
}

static void term_add_line(win_t* w, const char* text) {
    term_state_t* t = &w->state.term;
    if (t->line_count < 15) {
        wm_strncpy(t->lines[t->line_count], text, sizeof(t->lines[0]) - 1);
        t->lines[t->line_count][sizeof(t->lines[0]) - 1] = '\0';
        t->line_count++;
    } else {
        for (int i = 0; i < 14; i++) {
            memcpy(t->lines[i], t->lines[i + 1], sizeof(t->lines[0]));
        }
        wm_strncpy(t->lines[14], text, sizeof(t->lines[0]) - 1);
        t->lines[14][sizeof(t->lines[0]) - 1] = '\0';
    }
}

static void term_exec_cmd(win_t* w, const char* cmd) {
    term_state_t* t = &w->state.term;
    char echo_line[64];
    snprintf(echo_line, sizeof(echo_line), "$ %s", cmd);
    term_add_line(w, echo_line);

    if (strlen(cmd) == 0) return;

    if (streq(cmd, "help")) {
        term_add_line(w, "Builtin commands:");
        term_add_line(w, "  help, ls, pwd, clear, date, uname,");
        term_add_line(w, "  whoami, echo <msg>, cat <file>, exit");
    } else if (streq(cmd, "clear")) {
        t->line_count = 0;
    } else if (streq(cmd, "pwd")) {
        term_add_line(w, t->cwd);
    } else if (streq(cmd, "uname")) {
        term_add_line(w, "RandomOS 1.0 x86_64");
    } else if (streq(cmd, "whoami")) {
        term_add_line(w, "root");
    } else if (streq(cmd, "date")) {
        ctime_t ct;
        get_ctime(&ct);
        char buf[48];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u UTC",
                 (unsigned)ct.yr, (unsigned)ct.mon + 1, (unsigned)ct.day + 1,
                 (unsigned)ct.hrs, (unsigned)ct.min, (unsigned)ct.sec);
        term_add_line(w, buf);
    } else if (wm_strncmp(cmd, "echo ", 5) == 0) {
        term_add_line(w, cmd + 5);
    } else if (wm_strncmp(cmd, "ls", 2) == 0) {
        const char* dirpath = (strlen(cmd) > 3) ? cmd + 3 : "/";
        int fd = opendir((char*)dirpath);
        if (fd < 0) {
            term_add_line(w, "ls: cannot open directory");
        } else {
            struct stat st;
            char line[48];
            line[0] = '\0';
            int count = 0;
            while (readdir(fd, &st) == 0) {
                if (strlen(line) + strlen(st.st_name) + 2 < sizeof(line)) {
                    if (count > 0) wm_strcat(line, "  ");
                    wm_strcat(line, st.st_name);
                    count++;
                } else {
                    term_add_line(w, line);
                    wm_strncpy(line, st.st_name, sizeof(line) - 1);
                    count = 1;
                }
            }
            if (count > 0) term_add_line(w, line);
            close(fd);
        }
    } else if (wm_strncmp(cmd, "cat ", 4) == 0) {
        const char* path = cmd + 4;
        int fd = open((char*)path, O_RDONLY, 0);
        if (fd < 0) {
            term_add_line(w, "cat: cannot open file");
        } else {
            char fbuf[48];
            ssize n = read(fd, fbuf, sizeof(fbuf) - 1);
            if (n > 0) {
                fbuf[n] = '\0';
                term_add_line(w, fbuf);
            }
            close(fd);
        }
    } else if (streq(cmd, "exit")) {
        w->open = false;
    } else {
        term_add_line(w, "command not found. Type 'help'");
    }
}

static void files_refresh(win_t* w) {
    files_state_t* f = &w->state.files;
    f->file_count = 0;
    f->selected_idx = -1;

    int fd = opendir(f->current_path);
    if (fd < 0) return;

    struct stat st;
    while (readdir(fd, &st) == 0 && f->file_count < 32) {
        wm_strncpy(f->file_names[f->file_count], st.st_name, sizeof(f->file_names[0]) - 1);
        f->is_dir[f->file_count] = (S_TYPE(st.st_mode) == S_IFDIR) != 0;
        f->file_sizes[f->file_count] = st.st_size;
        f->file_count++;
    }
    close(fd);
}

static int wm_create(const char* title, int x, int y, int w, int h, app_type_t type) {
    int free_slot = -1;
    for (int i = 0; i < WIN_MAX; i++) {
        if (!wins[i].open) {
            free_slot = i;
            break;
        }
    }
    if (free_slot < 0) return -1;

    win_t* win = &wins[free_slot];
    memset(win, 0, sizeof(win_t));
    win->id = free_slot;
    win->open = true;
    win->minimized = false;
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    wm_strncpy(win->title, title, sizeof(win->title) - 1);
    win->app_type = type;

    win->buf = (u32*)malloc(w * h * sizeof(u32));
    if (!win->buf) {
        win->open = false;
        return -1;
    }

    win_order[win_count++] = free_slot;

    // Init App State
    if (type == APP_TERMINAL) {
        wm_strcpy(win->state.term.cwd, "/");
        term_add_line(win, "RandomOS Terminal 1.0");
        term_add_line(win, "Type 'help' for commands");
    } else if (type == APP_FILES) {
        wm_strcpy(win->state.files.current_path, "/");
        files_refresh(win);
    } else if (type == APP_NOTES) {
        wm_strcpy(win->state.notes.text[0], "Welcome to RandomOS Notes!");
        wm_strcpy(win->state.notes.text[1], "- Click or type to add notes");
        wm_strcpy(win->state.notes.text[2], "- Lightweight window manager");
        win->state.notes.cursor_row = 3;
    } else if (type == APP_CALC) {
        wm_strcpy(win->state.calc.display, "0");
        win->state.calc.clear_on_next = true;
    }

    return free_slot;
}

static void wm_close(int id) {
    if (id < 0 || id >= WIN_MAX || !wins[id].open) return;
    if (wins[id].buf) free(wins[id].buf);
    wins[id].open = false;

    for (int i = 0; i < win_count; i++) {
        if (win_order[i] == id) {
            for (int j = i; j < win_count - 1; j++) {
                win_order[j] = win_order[j + 1];
            }
            win_count--;
            break;
        }
    }
}

/* --- App Paint Handlers --- */

static void paint_terminal(win_t* w) {
    win_fill(w, 0, 0, w->w, w->h, 0xFF14161F);
    term_state_t* t = &w->state.term;

    int y = 6;
    for (int i = 0; i < t->line_count; i++) {
        u32 col = COLOR_TEXT;
        if (t->lines[i][0] == '$') col = COLOR_GREEN;
        win_draw_str(w, 8, y, t->lines[i], col, 0, true);
        y += GUI_FONT_H + 2;
    }

    // Input prompt
    char pbuf[64];
    snprintf(pbuf, sizeof(pbuf), "$ %s", t->input);
    win_draw_str(w, 8, y, pbuf, COLOR_CYAN, 0, true);

    // Cursor
    int cx = 8 + (int)strlen(pbuf) * GUI_FONT_W;
    win_fill(w, cx, y, GUI_FONT_W, GUI_FONT_H, COLOR_WHITE);
}

static void paint_files(win_t* w) {
    win_fill(w, 0, 0, w->w, w->h, COLOR_WIN_BG);
    files_state_t* f = &w->state.files;

    // Path bar
    win_fill(w, 0, 0, w->w, 24, 0xFF24283B);
    char path_disp[64];
    snprintf(path_disp, sizeof(path_disp), " Path: %s", f->current_path);
    win_draw_str(w, 4, 4, path_disp, COLOR_CYAN, 0, true);

    // File list
    int y = 30;
    for (int i = 0; i < f->file_count && y < w->h - 30; i++) {
        bool sel = (i == f->selected_idx);
        if (sel) {
            win_fill(w, 4, y - 2, w->w - 8, GUI_FONT_H + 4, 0xFF3D4668);
        }

        const char* tag = f->is_dir[i] ? "[DIR]" : "[FILE]";
        u32 col = f->is_dir[i] ? COLOR_YELLOW : COLOR_TEXT;
        char item[64];
        snprintf(item, sizeof(item), "%-6s %-16s %u B", tag, f->file_names[i], (unsigned)f->file_sizes[i]);
        win_draw_str(w, 8, y, item, col, 0, true);
        y += GUI_FONT_H + 4;
    }

    // Footer info
    win_fill(w, 0, w->h - 22, w->w, 22, 0xFF181A24);
    char count_info[64];
    snprintf(count_info, sizeof(count_info), "%d items", f->file_count);
    win_draw_str(w, 8, w->h - 18, count_info, COLOR_TEXT_DIM, 0, true);
}

static void paint_sysinfo(win_t* w) {
    win_fill(w, 0, 0, w->w, w->h, COLOR_WIN_BG);

    win_draw_str(w, 16, 12, "=== RandomOS System Info ===", COLOR_CYAN, 0, true);

    char buf[64];
    snprintf(buf, sizeof(buf), "OS:        RandomOS x86_64");
    win_draw_str(w, 16, 36, buf, COLOR_TEXT, 0, true);

    snprintf(buf, sizeof(buf), "Display:   %u x %u (32 bpp)", (unsigned)gui_w, (unsigned)gui_h);
    win_draw_str(w, 16, 54, buf, COLOR_TEXT, 0, true);

    snprintf(buf, sizeof(buf), "FS:        EXT2 Root Filesystem");
    win_draw_str(w, 16, 72, buf, COLOR_TEXT, 0, true);

    u64 uptime_s = (getclock(CLOCK_MONOMS) - start_time_ms) / 1000;
    snprintf(buf, sizeof(buf), "Uptime:    %u:%02u min", (unsigned)(uptime_s / 60), (unsigned)(uptime_s % 60));
    win_draw_str(w, 16, 90, buf, COLOR_GREEN, 0, true);

    // Visual Memory / CPU Bar
    win_draw_str(w, 16, 116, "Memory Utilization:", COLOR_TEXT_DIM, 0, true);
    win_fill(w, 16, 136, w->w - 32, 14, 0xFF24283B);
    win_fill(w, 16, 136, (w->w - 32) * 35 / 100, 14, COLOR_ACCENT);
    draw_rect(w->buf, w->w, 16, 136, w->w - 32, 14, COLOR_BAR_BORDER);

    win_draw_str(w, 16, 158, "CPU Utilization:", COLOR_TEXT_DIM, 0, true);
    win_fill(w, 16, 178, w->w - 32, 14, 0xFF24283B);
    win_fill(w, 16, 178, (w->w - 32) * 12 / 100, 14, COLOR_GREEN);
    draw_rect(w->buf, w->w, 16, 178, w->w - 32, 14, COLOR_BAR_BORDER);
}

static void paint_notes(win_t* w) {
    win_fill(w, 0, 0, w->w, w->h, 0xFF161821);
    notes_state_t* n = &w->state.notes;

    for (int r = 0; r < 8; r++) {
        int y = 8 + r * (GUI_FONT_H + 4);
        win_draw_str(w, 8, y, n->text[r], COLOR_TEXT, 0, true);
        if (r == n->cursor_row) {
            int cx = 8 + (int)strlen(n->text[r]) * GUI_FONT_W;
            win_fill(w, cx, y, 2, GUI_FONT_H, COLOR_ACCENT);
        }
    }
}

static void paint_calc(win_t* w) {
    win_fill(w, 0, 0, w->w, w->h, COLOR_WIN_BG);
    calc_state_t* c = &w->state.calc;

    // Display box
    win_fill(w, 12, 12, w->w - 24, 30, 0xFF14161F);
    draw_rect(w->buf, w->w, 12, 12, w->w - 24, 30, COLOR_BAR_BORDER);
    int text_x = w->w - 24 - (int)strlen(c->display) * GUI_FONT_W;
    win_draw_str(w, text_x, 18, c->display, COLOR_GREEN, 0, true);

    // Button Grid 4x4
    static const char* btns[4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "*"},
        {"1", "2", "3", "-"},
        {"C", "0", "=", "+"}
    };

    int bw = 40, bh = 28;
    int sx = 14, sy = 52;
    for (int r = 0; r < 4; r++) {
        for (int col = 0; col < 4; col++) {
            int bx = sx + col * (bw + 6);
            int by = sy + r * (bh + 6);
            u32 bg = (r == 3 && col == 0) ? 0xFF8A3B4A : 0xFF282D42;
            win_fill(w, bx, by, bw, bh, bg);
            draw_rect(w->buf, w->w, bx, by, bw, bh, COLOR_BAR_BORDER);
            win_draw_str(w, bx + 16, by + 6, btns[r][col], COLOR_WHITE, 0, true);
        }
    }
}

static void paint_about(win_t* w) {
    win_fill(w, 0, 0, w->w, w->h, COLOR_WIN_BG);
    win_draw_str(w, 16, 16, "RandomOS 64-Bit Desktop", COLOR_ACCENT, 0, true);
    win_draw_str(w, 16, 38, "Version: 1.0 Release", COLOR_TEXT_DIM, 0, true);
    win_draw_str(w, 16, 58, "Features:", COLOR_YELLOW, 0, true);
    win_draw_str(w, 24, 76, "- Multiprocessing & SMP", COLOR_TEXT, 0, true);
    win_draw_str(w, 24, 94, "- VirtIO Storage, Net & RNG", COLOR_TEXT, 0, true);
    win_draw_str(w, 24, 112, "- EXT2 Filesystem Driver", COLOR_TEXT, 0, true);
    win_draw_str(w, 24, 130, "- Dynamic Window Compositor", COLOR_TEXT, 0, true);
}

static void win_paint_content(win_t* w) {
    if (!w->open || !w->buf) return;
    switch (w->app_type) {
        case APP_TERMINAL: paint_terminal(w); break;
        case APP_FILES:    paint_files(w); break;
        case APP_SYSINFO:  paint_sysinfo(w); break;
        case APP_NOTES:    paint_notes(w); break;
        case APP_CALC:     paint_calc(w); break;
        case APP_ABOUT:    paint_about(w); break;
    }
}

/* --- Window Frame Rendering --- */

static void render_window(win_t* w, bool active) {
    if (!w->open || w->minimized) return;

    int wx = w->x, wy = w->y;
    int ww = w->w, wh = w->h;

    // Window Outer Shadow
    fill_rect(backbuf, gui_pitch_px, wx + 4, wy + 4, ww, wh + TITLE_H, 0xFF0B0C12);

    // Titlebar
    u32 title_col = active ? COLOR_WIN_TITLE_ACT : COLOR_WIN_TITLE;
    fill_rect(backbuf, gui_pitch_px, wx, wy, ww, TITLE_H, title_col);

    // Title text
    draw_str(backbuf, gui_pitch_px, wx + 8, wy + 5, w->title, active ? COLOR_WHITE : COLOR_TEXT_DIM, 0, true);

    // Minimize button [-]
    fill_rect(backbuf, gui_pitch_px, wx + ww - 38, wy + 5, 14, 14, COLOR_BTN_MIN);
    draw_char(backbuf, gui_pitch_px, wx + ww - 35, wy + 4, '-', COLOR_BLACK, 0, true);

    // Close button [X]
    fill_rect(backbuf, gui_pitch_px, wx + ww - 19, wy + 5, 14, 14, COLOR_BTN_CLOSE);
    draw_char(backbuf, gui_pitch_px, wx + ww - 16, wy + 4, 'x', COLOR_BLACK, 0, true);

    // Repaint client area
    win_paint_content(w);

    // Blit client area into backbuffer
    int client_y = wy + TITLE_H;
    for (int j = 0; j < wh; j++) {
        if (client_y + j < 0 || client_y + j >= (int)gui_h) continue;
        int sx = (wx < 0) ? 0 : wx;
        int ex = (wx + ww > (int)gui_w) ? (int)gui_w : wx + ww;
        int src_off_x = sx - wx;

        u32* dst = &backbuf[(client_y + j) * gui_pitch_px + sx];
        u32* src = &w->buf[j * ww + src_off_x];
        memcpy(dst, src, (ex - sx) * sizeof(u32));
    }

    // Window Border
    u32 border_col = active ? COLOR_WIN_BORDER_ACT : COLOR_WIN_BORDER;
    draw_rect(backbuf, gui_pitch_px, wx, wy, ww, wh + TITLE_H, border_col);
}

/* --- Top Menu Bar & Desktop --- */

static void render_desktop(void) {
    draw_gradient_v(backbuf, gui_pitch_px, 0, 0, gui_w, gui_h, COLOR_DESKTOP_TOP, COLOR_DESKTOP_BOT);

    // Desktop Launcher Icons
    static const struct { const char* name; const char* tag; int x, y; } dicons[] = {
        {"Terminal",   "[ >_ ]",  30,  50},
        {"Files",      "[ DIR]",  30, 130},
        {"SysInfo",    "[ CPU]",  30, 210},
        {"Notes",      "[ TXT]",  30, 290},
        {"Calculator", "[ 123]",  30, 370},
    };

    for (int i = 0; i < 5; i++) {
        fill_rect(backbuf, gui_pitch_px, dicons[i].x, dicons[i].y, 64, 48, 0x5524283B);
        draw_rect(backbuf, gui_pitch_px, dicons[i].x, dicons[i].y, 64, 48, COLOR_BAR_BORDER);
        draw_str(backbuf, gui_pitch_px, dicons[i].x + 8, dicons[i].y + 10, dicons[i].tag, COLOR_CYAN, 0, true);
        draw_str(backbuf, gui_pitch_px, dicons[i].x + 4, dicons[i].y + 30, dicons[i].name, COLOR_TEXT, 0, true);
    }
}

static void render_top_bar(void) {
    fill_rect(backbuf, gui_pitch_px, 0, 0, gui_w, BAR_H, COLOR_BAR_BG);
    draw_rect(backbuf, gui_pitch_px, 0, 0, gui_w, BAR_H, COLOR_BAR_BORDER);

    // Menu button
    u32 menu_btn_col = menu_open ? COLOR_ACCENT : 0xFF343B58;
    fill_rect(backbuf, gui_pitch_px, 4, 3, 72, BAR_H - 6, menu_btn_col);
    draw_rect(backbuf, gui_pitch_px, 4, 3, 72, BAR_H - 6, COLOR_BAR_BORDER);
    draw_str(backbuf, gui_pitch_px, 12, 7, "* Menu", menu_open ? COLOR_BLACK : COLOR_WHITE, 0, true);

    // Taskbar Tabs for Open Windows
    int tab_x = 84;
    for (int i = 0; i < win_count; i++) {
        int id = win_order[i];
        if (!wins[id].open) continue;

        bool active = (id == wm_get_active_win());
        u32 tab_bg = active ? COLOR_WIN_TITLE_ACT : (wins[id].minimized ? 0xFF1C1E2B : 0xFF2A2E44);
        fill_rect(backbuf, gui_pitch_px, tab_x, 3, 100, BAR_H - 6, tab_bg);
        draw_rect(backbuf, gui_pitch_px, tab_x, 3, 100, BAR_H - 6, active ? COLOR_ACCENT : COLOR_BAR_BORDER);

        char title_trunc[12];
        wm_strncpy(title_trunc, wins[id].title, 10);
        title_trunc[10] = '\0';
        draw_str(backbuf, gui_pitch_px, tab_x + 6, 7, title_trunc, active ? COLOR_WHITE : COLOR_TEXT_DIM, 0, true);

        tab_x += 106;
    }

    // Clock
    ctime_t ct;
    get_ctime(&ct);
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u", (unsigned)ct.hrs, (unsigned)ct.min, (unsigned)ct.sec);
    draw_str(backbuf, gui_pitch_px, gui_w - 74, 7, time_str, COLOR_YELLOW, 0, true);
}

static void render_app_menu(void) {
    if (!menu_open) return;

    int mx = 4, my = BAR_H + 2;
    int mw = 140, mh = 168;

    fill_rect(backbuf, gui_pitch_px, mx + 2, my + 2, mw, mh, 0xFF0B0C12);
    fill_rect(backbuf, gui_pitch_px, mx, my, mw, mh, COLOR_MENU_BG);
    draw_rect(backbuf, gui_pitch_px, mx, my, mw, mh, COLOR_MENU_BORDER);

    static const char* items[] = {
        "1. Terminal",
        "2. Files",
        "3. System Info",
        "4. Notes",
        "5. Calculator",
        "6. About",
        "7. Exit WM"
    };

    for (int i = 0; i < 7; i++) {
        int item_y = my + 4 + i * 23;
        bool hover = (mouse_x >= mx && mouse_x < mx + mw && mouse_y >= item_y && mouse_y < item_y + 22);
        if (hover) {
            fill_rect(backbuf, gui_pitch_px, mx + 2, item_y, mw - 4, 21, COLOR_MENU_HOVER);
        }
        draw_str(backbuf, gui_pitch_px, mx + 8, item_y + 3, items[i], hover ? COLOR_CYAN : COLOR_TEXT, 0, true);
    }
}

/* --- Input & Interaction Handling --- */

static bool wm_running = true;

static void handle_click(int mx, int my) {
    // 1. Menu Click
    if (mx >= 4 && mx <= 76 && my >= 0 && my <= BAR_H) {
        menu_open = !menu_open;
        return;
    }

    // 2. Menu Item Click
    if (menu_open && mx >= 4 && mx <= 144 && my >= BAR_H && my <= BAR_H + 170) {
        int item = (my - BAR_H - 4) / 23;
        menu_open = false;
        if (item == 0) wm_create("Terminal", 100, 60, 360, 240, APP_TERMINAL);
        else if (item == 1) wm_create("File Manager", 140, 90, 340, 230, APP_FILES);
        else if (item == 2) wm_create("System Info", 180, 110, 300, 220, APP_SYSINFO);
        else if (item == 3) wm_create("Notes", 220, 130, 280, 190, APP_NOTES);
        else if (item == 4) wm_create("Calculator", 250, 140, 210, 200, APP_CALC);
        else if (item == 5) wm_create("About RandomOS", 160, 100, 320, 180, APP_ABOUT);
        else if (item == 6) { wm_running = false; }
        return;
    }
    if (menu_open) {
        menu_open = false;
    }

    // 3. Taskbar Tab Click
    if (my >= 0 && my <= BAR_H && mx >= 84) {
        int tab_idx = (mx - 84) / 106;
        if (tab_idx >= 0 && tab_idx < win_count) {
            int id = win_order[tab_idx];
            if (wins[id].open) {
                if (wins[id].minimized) {
                    wins[id].minimized = false;
                    wm_bring_to_front(id);
                } else if (id == wm_get_active_win()) {
                    wins[id].minimized = true;
                } else {
                    wm_bring_to_front(id);
                }
                return;
            }
        }
    }

    // 4. Desktop Icon Click
    if (mx >= 30 && mx <= 94) {
        if (my >= 50 && my <= 98) { wm_create("Terminal", 100, 60, 360, 240, APP_TERMINAL); return; }
        if (my >= 130 && my <= 178) { wm_create("File Manager", 140, 90, 340, 230, APP_FILES); return; }
        if (my >= 210 && my <= 258) { wm_create("System Info", 180, 110, 300, 220, APP_SYSINFO); return; }
        if (my >= 290 && my <= 338) { wm_create("Notes", 220, 130, 280, 190, APP_NOTES); return; }
        if (my >= 370 && my <= 418) { wm_create("Calculator", 250, 140, 210, 200, APP_CALC); return; }
    }

    // 5. Window Hit Testing (from top to bottom)
    for (int i = win_count - 1; i >= 0; i--) {
        int id = win_order[i];
        win_t* w = &wins[id];
        if (!w->open || w->minimized) continue;

        // Inside Window Bounds?
        if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h + TITLE_H) {
            wm_bring_to_front(id);

            // Titlebar clicks
            if (my < w->y + TITLE_H) {
                // Close button [X]
                if (mx >= w->x + w->w - 20 && mx <= w->x + w->w - 4) {
                    wm_close(id);
                    return;
                }
                // Minimize button [-]
                if (mx >= w->x + w->w - 40 && mx <= w->x + w->w - 24) {
                    w->minimized = true;
                    return;
                }
                // Drag start
                drag_win = id;
                drag_off_x = mx - w->x;
                drag_off_y = my - w->y;
                return;
            }

            // Client area clicks
            int cx = mx - w->x;
            int cy = my - (w->y + TITLE_H);

            if (w->app_type == APP_CALC) {
                calc_state_t* c = &w->state.calc;
                int sx = 14, sy = 52, bw = 40, bh = 28;
                static const char* btns[4][4] = {
                    {"7", "8", "9", "/"},
                    {"4", "5", "6", "*"},
                    {"1", "2", "3", "-"},
                    {"C", "0", "=", "+"}
                };
                for (int r = 0; r < 4; r++) {
                    for (int col = 0; col < 4; col++) {
                        int bx = sx + col * (bw + 6);
                        int by = sy + r * (bh + 6);
                        if (cx >= bx && cx < bx + bw && cy >= by && cy < by + bh) {
                            const char* b = btns[r][col];
                            if (b[0] >= '0' && b[0] <= '9') {
                                if (c->clear_on_next || streq(c->display, "0")) {
                                    c->display[0] = b[0];
                                    c->display[1] = '\0';
                                    c->clear_on_next = false;
                                } else if (strlen(c->display) < 16) {
                                    wm_strcat(c->display, b);
                                }
                            } else if (b[0] == 'C') {
                                wm_strcpy(c->display, "0");
                                c->acc = 0;
                                c->op = 0;
                                c->clear_on_next = true;
                            } else if (b[0] == '=') {
                                s64 val = atoi(c->display);
                                if (c->op == '+') c->acc += val;
                                else if (c->op == '-') c->acc -= val;
                                else if (c->op == '*') c->acc *= val;
                                else if (c->op == '/' && val != 0) c->acc /= val;
                                snprintf(c->display, sizeof(c->display), "%d", (int)c->acc);
                                c->op = 0;
                                c->clear_on_next = true;
                            } else {
                                c->acc = atoi(c->display);
                                c->op = b[0];
                                c->clear_on_next = true;
                            }
                            return;
                        }
                    }
                }
            } else if (w->app_type == APP_FILES) {
                files_state_t* f = &w->state.files;
                int clicked_item = (cy - 30) / (GUI_FONT_H + 4);
                if (clicked_item >= 0 && clicked_item < f->file_count) {
                    if (f->selected_idx == clicked_item && f->is_dir[clicked_item]) {
                        // Enter directory
                        if (streq(f->file_names[clicked_item], ".")) {
                            // nothing
                        } else if (streq(f->file_names[clicked_item], "..")) {
                            wm_strcpy(f->current_path, "/");
                            files_refresh(w);
                        } else {
                            if (streq(f->current_path, "/")) {
                                snprintf(f->current_path, sizeof(f->current_path), "/%s", f->file_names[clicked_item]);
                            } else {
                                snprintf(f->current_path, sizeof(f->current_path), "%s/%s", f->current_path, f->file_names[clicked_item]);
                            }
                            files_refresh(w);
                        }
                    } else {
                        f->selected_idx = clicked_item;
                    }
                }
            }
            return;
        }
    }
}

static void handle_key(u8 sc, bool shift) {
    if (sc == 0x01) { // ESC toggles app menu
        menu_open = !menu_open;
        return;
    }

    int active_id = wm_get_active_win();
    if (active_id < 0) return;
    win_t* w = &wins[active_id];

    char ch = scancode_to_ascii(sc, shift);

    if (w->app_type == APP_TERMINAL) {
        term_state_t* t = &w->state.term;
        if (ch == '\n') {
            term_exec_cmd(w, t->input);
            t->input[0] = '\0';
            t->input_len = 0;
        } else if (ch == '\b') {
            if (t->input_len > 0) {
                t->input_len--;
                t->input[t->input_len] = '\0';
            }
        } else if (ch >= ' ' && t->input_len < 40) {
            t->input[t->input_len++] = ch;
            t->input[t->input_len] = '\0';
        }
    } else if (w->app_type == APP_NOTES) {
        notes_state_t* n = &w->state.notes;
        if (ch == '\n') {
            if (n->cursor_row < 7) n->cursor_row++;
        } else if (ch == '\b') {
            int len = strlen(n->text[n->cursor_row]);
            if (len > 0) n->text[n->cursor_row][len - 1] = '\0';
        } else if (ch >= ' ') {
            int len = strlen(n->text[n->cursor_row]);
            if (len < 36) {
                n->text[n->cursor_row][len] = ch;
                n->text[n->cursor_row][len + 1] = '\0';
            }
        }
    }
}

/* --- Main Window Manager Entrypoint --- */

int main(void) {
    term_fb = get_currfb();
    if (term_fb < 0) return 1;

    gui_fb_id = create_fb(FBTYPE_GUI);
    if (gui_fb_id < 0) return 1;
    if (switch_fb(gui_fb_id) < 0) return 1;

    framebuf_info_t info;
    if (get_fbinfo(gui_fb_id, &info) < 0) return 1;

    gui_fb = (u32*)info.ptr;
    gui_w = info.width;
    gui_h = info.height;
    gui_pitch_px = info.pitch / 4;

    guictx_t* g = gui_init(gui_fb_id);
    if (!g) return 1;
    gui_free(g);

    backbuf = (u32*)malloc(gui_pitch_px * gui_h * sizeof(u32));
    if (!backbuf) return 1;

    start_time_ms = getclock(CLOCK_MONOMS);
    mouse_x = gui_w / 2;
    mouse_y = gui_h / 2;

    // Launch initial windows
    wm_create("Terminal", 80, 50, 360, 240, APP_TERMINAL);
    wm_create("File Manager", 220, 100, 340, 230, APP_FILES);

    bool shift_pressed = false;

    while (wm_running) {
        // 1. Mouse Input
        mouse_info_t minfo;
        if (get_mouse_info(&minfo) == 0) {
            mouse_x += minfo.x;
            mouse_y += minfo.y;

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= (int)gui_w) mouse_x = (int)gui_w - 1;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= (int)gui_h) mouse_y = (int)gui_h - 1;

            bool left_down = (minfo.buttons & MOUSE_BUTTON_LEFT) != 0;
            bool left_click = left_down && !(mouse_prev_btns & MOUSE_BUTTON_LEFT);

            if (left_click) {
                handle_click(mouse_x, mouse_y);
            }

            if (left_down && drag_win >= 0) {
                win_t* dw = &wins[drag_win];
                dw->x = mouse_x - drag_off_x;
                dw->y = mouse_y - drag_off_y;
                if (dw->y < BAR_H) dw->y = BAR_H;
            } else if (!left_down) {
                drag_win = -1;
            }

            mouse_prev_btns = minfo.buttons;
        }

        // 2. Keyboard Input
        u8 sc = kbd_get_raw_to(5);
        if (sc != 0) {
            if (sc == 0x2A || sc == 0x36) {
                shift_pressed = true;
            } else if (sc == 0xAA || sc == 0xB6) {
                shift_pressed = false;
            } else if (!(sc & 0x80)) { // Key press (not release)
                handle_key(sc, shift_pressed);
            }
        }

        // 3. Compose Scene to Backbuffer
        render_desktop();

        int active_id = wm_get_active_win();
        for (int i = 0; i < win_count; i++) {
            int id = win_order[i];
            render_window(&wins[id], (id == active_id));
        }

        render_top_bar();
        render_app_menu();
        draw_mouse_cursor(backbuf, gui_pitch_px, mouse_x, mouse_y);

        // 4. Blit Backbuffer to Screen
        memcpy(gui_fb, backbuf, gui_pitch_px * gui_h * sizeof(u32));
        flush_scr();
    }

    if (backbuf) free(backbuf);
    switch_fb(term_fb);
    return 0;
}