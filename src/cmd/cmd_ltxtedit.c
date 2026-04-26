#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include "cu.h"

#define LTXT_MAX_LINES    8192
#define LTXT_MAX_LINE_LEN 1024
#define LTXT_MAX_TABS     8
#define LTXT_MAX_CMD      256
#define LTXT_GUTTER_WIDTH 6

#define LTXT_KEY_CTRL(k) ((k) & 0x1F)

enum ltxt_mode {
    MODE_EDIT,
    MODE_SEARCH,
    MODE_REPLACE,
    MODE_REPLACE_WITH,
    MODE_GOTO,
    MODE_COMMAND,
    MODE_SAVE_AS
};

struct ltxt_line {
    char *data;
    int len;
    int cap;
};

struct ltxt_buf {
    struct ltxt_line *lines;
    int num_lines;
    int cx;
    int cy;
    int scroll_y;
    int scroll_x;
    int modified;
    char filename[256];
    int active;
};

struct ltxt_state {
    struct ltxt_buf tabs[LTXT_MAX_TABS];
    int num_tabs;
    int cur_tab;
    int screen_rows;
    int screen_cols;
    int running;
    enum ltxt_mode mode;
    char prompt_buf[LTXT_MAX_CMD];
    int prompt_len;
    char search_buf[LTXT_MAX_CMD];
    int search_len;
    int quit_count;
    char replace_buf[LTXT_MAX_CMD];
    int replace_len;
    int title_dirty;
    char title_cache[2048];
    int title_cache_len;
    char msg[128];
    int msg_len;
};

static struct ltxt_state E;

static struct termios orig_termios;
static int raw_mode_on = 0;

static void ltxt_write(const char *s, int len) {
    write(STDOUT_FILENO, s, len);
}

static void ltxt_puts(const char *s) {
    ltxt_write(s, strlen(s));
}

static void ltxt_enable_raw(void) {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, 0, &raw);
    raw_mode_on = 1;
}

static void ltxt_disable_raw(void) {
    if (raw_mode_on) {
        tcsetattr(STDIN_FILENO, 0, &orig_termios);
        raw_mode_on = 0;
    }
}

static int ltxt_read_key(void) {
    char c;
    int n;

    n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) return -1;

    if (c == '\x1b') {
        char seq[4];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                    case '1': return 1000;
                    case '3': return 1003;
                    case '4': return 1004;
                    case '5': return 1005;
                    case '6': return 1006;
                    case '7': return 1000;
                    case '8': return 1004;
                    }
                }
            } else {
                switch (seq[1]) {
                case 'A': return 1100;
                case 'B': return 1101;
                case 'C': return 1102;
                case 'D': return 1103;
                case 'H': return 1000;
                case 'F': return 1004;
                }
            }
        }
        return '\x1b';
    }
    return (unsigned char)c;
}

static void ltxt_get_screen_size(void) {
    int r;

    r = fb_getinfo(NULL, NULL, NULL, NULL, (unsigned int *)&E.screen_rows,
                   NULL, NULL, (unsigned int *)&E.screen_cols);
    if (r < 0 || E.screen_rows <= 0 || E.screen_cols <= 0) {
        E.screen_rows = 24;
        E.screen_cols = 80;
    }
}

static void ltxt_line_init(struct ltxt_line *l) {
    l->cap = 128;
    l->data = malloc(l->cap);
    l->data[0] = '\0';
    l->len = 0;
}

static void ltxt_line_ensure(struct ltxt_line *l, int need) {
    int newcap;

    if (l->cap >= need) return;
    newcap = l->cap;
    while (newcap < need) newcap *= 2;
    l->data = realloc(l->data, newcap);
    l->cap = newcap;
}

static void ltxt_line_insert(struct ltxt_line *l, int pos, char c) {
    int i;

    if (pos < 0) pos = 0;
    if (pos > l->len) pos = l->len;
    ltxt_line_ensure(l, l->len + 2);
    for (i = l->len; i > pos; i--)
        l->data[i] = l->data[i - 1];
    l->data[pos] = c;
    l->len++;
    l->data[l->len] = '\0';
}

static void ltxt_line_delete(struct ltxt_line *l, int pos) {
    int i;

    if (pos < 0 || pos >= l->len) return;
    for (i = pos; i < l->len - 1; i++)
        l->data[i] = l->data[i + 1];
    l->len--;
    l->data[l->len] = '\0';
}

static void ltxt_buf_init(struct ltxt_buf *b) {
    memset(b, 0, sizeof(*b));
    b->lines = malloc(sizeof(struct ltxt_line) * 16);
    b->num_lines = 1;
    ltxt_line_init(&b->lines[0]);
    b->active = 1;
}

static void ltxt_buf_free(struct ltxt_buf *b) {
    int i;

    for (i = 0; i < b->num_lines; i++)
        free(b->lines[i].data);
    free(b->lines);
    b->active = 0;
}

static void ltxt_buf_insert_line(struct ltxt_buf *b, int at) {
    int i;
    int newcap;
    struct ltxt_line *newlines;

    if (at < 0) at = 0;
    if (at > b->num_lines) at = b->num_lines;

    newcap = b->num_lines + 1;
    newlines = malloc(sizeof(struct ltxt_line) * (newcap + 16));
    for (i = 0; i < at; i++)
        newlines[i] = b->lines[i];
    ltxt_line_init(&newlines[at]);
    for (i = at; i < b->num_lines; i++)
        newlines[i + 1] = b->lines[i];
    free(b->lines);
    b->lines = newlines;
    b->num_lines++;
}

static void ltxt_buf_delete_line(struct ltxt_buf *b, int at) {
    int i;

    if (at < 0 || at >= b->num_lines) return;
    if (b->num_lines <= 1) return;
    free(b->lines[at].data);
    for (i = at; i < b->num_lines - 1; i++)
        b->lines[i] = b->lines[i + 1];
    b->num_lines--;
}

static int ltxt_buf_load(struct ltxt_buf *b, const char *path) {
    int fd;
    int rd;
    int i;
    char fbuf[512];
    char linebuf[LTXT_MAX_LINE_LEN];
    int lpos;

    strncpy(b->filename, path, sizeof(b->filename) - 1);
    b->filename[sizeof(b->filename) - 1] = '\0';

    fd = vfs_open(path, 0);
    if (fd < 0) {
        if (b->num_lines == 0)
            ltxt_buf_insert_line(b, 0);
        return 0;
    }

    for (i = 0; i < b->num_lines; i++)
        free(b->lines[i].data);
    free(b->lines);
    b->lines = malloc(sizeof(struct ltxt_line) * 16);
    b->num_lines = 0;

    lpos = 0;
    while ((rd = vfs_read_fd(fd, fbuf, sizeof(fbuf))) > 0) {
        for (i = 0; i < rd; i++) {
            if (fbuf[i] == '\n' || lpos >= LTXT_MAX_LINE_LEN - 1) {
                linebuf[lpos] = '\0';
                ltxt_buf_insert_line(b, b->num_lines);
                ltxt_line_ensure(&b->lines[b->num_lines - 1], lpos + 1);
                memcpy(b->lines[b->num_lines - 1].data, linebuf, lpos + 1);
                b->lines[b->num_lines - 1].len = lpos;
                lpos = 0;
            } else {
                linebuf[lpos++] = fbuf[i];
            }
        }
    }

    if (lpos > 0) {
        linebuf[lpos] = '\0';
        ltxt_buf_insert_line(b, b->num_lines);
        ltxt_line_ensure(&b->lines[b->num_lines - 1], lpos + 1);
        memcpy(b->lines[b->num_lines - 1].data, linebuf, lpos + 1);
        b->lines[b->num_lines - 1].len = lpos;
    }

    vfs_close_fd(fd);

    if (b->num_lines == 0) {
        ltxt_buf_insert_line(b, 0);
    }

    b->cx = 0;
    b->cy = 0;
    b->scroll_y = 0;
    b->scroll_x = 0;
    b->modified = 0;
    return 0;
}

static int ltxt_buf_save(struct ltxt_buf *b) {
    int fd;
    int i;

    if (b->filename[0] == '\0') return -1;

    fd = vfs_open(b->filename, 0x0001 | 0x0040 | 0x0200);
    if (fd < 0) return -1;

    for (i = 0; i < b->num_lines; i++) {
        vfs_write_fd(fd, b->lines[i].data, b->lines[i].len);
        if (i < b->num_lines - 1)
            vfs_write_fd(fd, "\n", 1);
    }

    vfs_close_fd(fd);
    b->modified = 0;
    return 0;
}

static struct ltxt_buf *ltxt_cur_buf(void) {
    return &E.tabs[E.cur_tab];
}

static void ltxt_draw_title_bar(char *abuf, int *alen) {
    int i;
    int tab;
    int used;

    memcpy(abuf + *alen, "\x1b[7m", 4);
    *alen += 4;

    memcpy(abuf + *alen, " ltxtedit ", 10);
    *alen += 10;

    used = 10;
    for (tab = 0; tab < E.num_tabs; tab++) {
        struct ltxt_buf *b;
        const char *name;
        int nlen;
        int tlen;

        b = &E.tabs[tab];
        name = b->filename[0] ? b->filename : "[new]";
        nlen = strlen(name);

        if (tab == E.cur_tab) {
            memcpy(abuf + *alen, "\x1b[0m\x1b[7m\x1b[1m", 12);
            *alen += 12;
        } else {
            memcpy(abuf + *alen, "\x1b[0m\x1b[7m", 8);
            *alen += 8;
        }

        abuf[(*alen)++] = ' ';
        if (nlen > 20) {
            memcpy(abuf + *alen, name + nlen - 20, 20);
            *alen += 20;
            tlen = 20;
        } else {
            memcpy(abuf + *alen, name, nlen);
            *alen += nlen;
            tlen = nlen;
        }
        if (b->modified) {
            memcpy(abuf + *alen, " [+]", 4);
            *alen += 4;
            tlen += 4;
        }
        abuf[(*alen)++] = ' ';
        used += tlen + 2;
    }

    memcpy(abuf + *alen, "\x1b[0m\x1b[7m", 8);
    *alen += 8;

    for (i = used; i < E.screen_cols - 1; i++)
        abuf[(*alen)++] = ' ';

    memcpy(abuf + *alen, "\x1b[0m", 4);
    *alen += 4;
}

static void ltxt_draw_status_bar(char *abuf, int *alen) {
    struct ltxt_buf *b;
    char left[128];
    char right[64];
    int llen;
    int rlen;
    int pad;
    int i;

    b = ltxt_cur_buf();

    if (b->filename[0])
        llen = snprintf(left, sizeof(left), " %s%s",
                        b->filename, b->modified ? " [modified]" : "");
    else
        llen = snprintf(left, sizeof(left), "[new file]%s",
                        b->modified ? " [modified]" : "");

    rlen = snprintf(right, sizeof(right), "Ln %d, Col %d  %d lines ",
                    b->cy + 1, b->cx + 1, b->num_lines);

    memcpy(abuf + *alen, "\x1b[7m", 4);
    *alen += 4;

    if (llen > E.screen_cols - 1 - rlen)
        llen = E.screen_cols - 1 - rlen;
    if (llen < 0) llen = 0;
    memcpy(abuf + *alen, left, llen);
    *alen += llen;

    pad = E.screen_cols - 1 - llen - rlen;
    if (pad < 0) pad = 0;
    for (i = 0; i < pad; i++)
        abuf[(*alen)++] = ' ';

    memcpy(abuf + *alen, right, rlen);
    *alen += rlen;

    memcpy(abuf + *alen, "\x1b[0m", 4);
    *alen += 4;
}

static void ltxt_draw_prompt_bar(char *abuf, int *alen) {
    const char *label;
    int llen;
    int plen;
    int i;
    int pad;

    switch (E.mode) {
    case MODE_SEARCH:       label = "Search: "; break;
    case MODE_REPLACE:      label = "Replace: "; break;
    case MODE_REPLACE_WITH: label = "Replace with: "; break;
    case MODE_GOTO:         label = "Goto line: "; break;
    case MODE_COMMAND:      label = "Command: "; break;
    case MODE_SAVE_AS:      label = "Save as: "; break;
    default:
        if (E.msg_len > 0)
            label = E.msg;
        else if (E.quit_count > 0)
            label = "Unsaved changes! Press Ctrl+Q again to quit, or Ctrl+S to save.";
        else
            label = "^S Save  ^Q Quit  ^F Find  ^R Replace  ^G Goto  ^P Command  ^T New tab  ^W Close tab";
        break;
    }

    llen = strlen(label);
    memcpy(abuf + *alen, "\x1b[7m", 4);
    *alen += 4;

    memcpy(abuf + *alen, label, llen);
    *alen += llen;

    if (E.mode != MODE_EDIT) {
        plen = E.prompt_len;
        if (plen > E.screen_cols - 1 - llen - 1) plen = E.screen_cols - 1 - llen - 1;
        if (plen < 0) plen = 0;
        memcpy(abuf + *alen, E.prompt_buf, plen);
        *alen += plen;
        pad = E.screen_cols - 1 - llen - plen;
    } else {
        pad = E.screen_cols - 1 - llen;
    }

    if (pad < 0) pad = 0;
    for (i = 0; i < pad; i++)
        abuf[(*alen)++] = ' ';

    memcpy(abuf + *alen, "\x1b[0m", 4);
    *alen += 4;
}

static void ltxt_draw(void) {
    struct ltxt_buf *b;
    char *abuf;
    int alen;
    int y;
    int text_rows;
    int file_row;
    int vis_cols;
    int gutter;
    char posbuf[32];
    int poslen;

    ltxt_get_screen_size();

    b = ltxt_cur_buf();
    text_rows = E.screen_rows - 3;
    if (text_rows < 1) text_rows = 1;
    gutter = LTXT_GUTTER_WIDTH;
    vis_cols = E.screen_cols - gutter;
    if (vis_cols < 1) vis_cols = 1;

    abuf = malloc((unsigned)(E.screen_rows * (E.screen_cols + 64) + 4096));
    if (!abuf) return;
    alen = 0;

    memcpy(abuf + alen, "\x1b[?25l\x1b[1;1H", 12);
    alen += 12;

    ltxt_draw_title_bar(abuf, &alen);

    for (y = 0; y < text_rows; y++) {
        int line_x_start;
        int line_len;
        int draw_len;

        poslen = snprintf(posbuf, sizeof(posbuf), "\x1b[%d;1H", y + 2);
        memcpy(abuf + alen, posbuf, poslen);
        alen += poslen;

        memcpy(abuf + alen, "\x1b[0m", 4);
        alen += 4;

        file_row = b->scroll_y + y;

        if (file_row < b->num_lines) {
            char gstr[16];
            int gn;

            gn = snprintf(gstr, sizeof(gstr), "%*d ", gutter - 1, file_row + 1);

            if (file_row == b->cy) {
                memcpy(abuf + alen, "\x1b[33m", 5);
                alen += 5;
            } else {
                memcpy(abuf + alen, "\x1b[90m", 5);
                alen += 5;
            }

            memcpy(abuf + alen, gstr, gn);
            alen += gn;

            memcpy(abuf + alen, "\x1b[0m", 4);
            alen += 4;

            line_x_start = b->scroll_x;
            line_len = b->lines[file_row].len;
            if (line_x_start > line_len) line_x_start = line_len;
            draw_len = line_len - line_x_start;
            if (draw_len > vis_cols) draw_len = vis_cols;
            if (draw_len < 0) draw_len = 0;

            if (draw_len > 0)
                memcpy(abuf + alen, b->lines[file_row].data + line_x_start, draw_len);
            alen += draw_len;
        }

        memcpy(abuf + alen, "\x1b[K", 3);
        alen += 3;
    }

    poslen = snprintf(posbuf, sizeof(posbuf), "\x1b[%d;1H", E.screen_rows - 1);
    memcpy(abuf + alen, posbuf, poslen);
    alen += poslen;
    ltxt_draw_status_bar(abuf, &alen);

    poslen = snprintf(posbuf, sizeof(posbuf), "\x1b[%d;1H", E.screen_rows);
    memcpy(abuf + alen, posbuf, poslen);
    alen += poslen;
    ltxt_draw_prompt_bar(abuf, &alen);

    {
        int scr_cx;
        int scr_cy;
        char pos[32];
        int pn;

        scr_cy = b->cy - b->scroll_y + 2;
        scr_cx = b->cx - b->scroll_x + gutter + 1;
        pn = snprintf(pos, sizeof(pos), "\x1b[%d;%dH", scr_cy, scr_cx);
        memcpy(abuf + alen, pos, pn);
        alen += pn;
    }

    memcpy(abuf + alen, "\x1b[?25h", 6);
    alen += 6;

    ltxt_write(abuf, alen);
    free(abuf);
}

static void ltxt_scroll(void) {
    struct ltxt_buf *b;
    int text_rows;
    int vis_cols;

    b = ltxt_cur_buf();
    text_rows = E.screen_rows - 3;
    vis_cols = E.screen_cols - LTXT_GUTTER_WIDTH;

    if (b->cy < b->scroll_y) b->scroll_y = b->cy;
    if (b->cy >= b->scroll_y + text_rows) b->scroll_y = b->cy - text_rows + 1;
    if (b->cx < b->scroll_x) b->scroll_x = b->cx;
    if (b->cx >= b->scroll_x + vis_cols) b->scroll_x = b->cx - vis_cols + 1;
}

static void ltxt_clamp_cursor(void) {
    struct ltxt_buf *b;

    b = ltxt_cur_buf();
    if (b->cy < 0) b->cy = 0;
    if (b->cy >= b->num_lines) b->cy = b->num_lines - 1;
    if (b->cx < 0) b->cx = 0;
    if (b->cx > b->lines[b->cy].len) b->cx = b->lines[b->cy].len;
}

static void ltxt_insert_char(int c) {
    struct ltxt_buf *b;

    b = ltxt_cur_buf();
    ltxt_line_insert(&b->lines[b->cy], b->cx, (char)c);
    b->cx++;
    if (!b->modified) E.title_dirty = 1;
    b->modified = 1;
}

static void ltxt_insert_newline(void) {
    struct ltxt_buf *b;
    int rest_len;

    b = ltxt_cur_buf();
    ltxt_buf_insert_line(b, b->cy + 1);

    rest_len = b->lines[b->cy].len - b->cx;
    if (rest_len > 0) {
        ltxt_line_ensure(&b->lines[b->cy + 1], rest_len + 1);
        memcpy(b->lines[b->cy + 1].data, b->lines[b->cy].data + b->cx, rest_len);
        b->lines[b->cy + 1].len = rest_len;
        b->lines[b->cy + 1].data[rest_len] = '\0';
        b->lines[b->cy].len = b->cx;
        b->lines[b->cy].data[b->cx] = '\0';
    }

    b->cy++;
    b->cx = 0;
    if (!b->modified) E.title_dirty = 1;
    b->modified = 1;
}

static void ltxt_delete_char(void) {
    struct ltxt_buf *b;
    int prev_len;

    b = ltxt_cur_buf();
    if (b->cx > 0) {
        ltxt_line_delete(&b->lines[b->cy], b->cx - 1);
        b->cx--;
        if (!b->modified) E.title_dirty = 1;
        b->modified = 1;
    } else if (b->cy > 0) {
        prev_len = b->lines[b->cy - 1].len;
        ltxt_line_ensure(&b->lines[b->cy - 1],
                         prev_len + b->lines[b->cy].len + 1);
        memcpy(b->lines[b->cy - 1].data + prev_len,
               b->lines[b->cy].data, b->lines[b->cy].len);
        b->lines[b->cy - 1].len += b->lines[b->cy].len;
        b->lines[b->cy - 1].data[b->lines[b->cy - 1].len] = '\0';
        ltxt_buf_delete_line(b, b->cy);
        b->cy--;
        b->cx = prev_len;
        if (!b->modified) E.title_dirty = 1;
        b->modified = 1;
    }
}

static void ltxt_delete_forward(void) {
    struct ltxt_buf *b;
    int cur_len;

    b = ltxt_cur_buf();
    if (b->cx < b->lines[b->cy].len) {
        ltxt_line_delete(&b->lines[b->cy], b->cx);
        if (!b->modified) E.title_dirty = 1;
        b->modified = 1;
    } else if (b->cy < b->num_lines - 1) {
        cur_len = b->lines[b->cy].len;
        ltxt_line_ensure(&b->lines[b->cy],
                         cur_len + b->lines[b->cy + 1].len + 1);
        memcpy(b->lines[b->cy].data + cur_len,
               b->lines[b->cy + 1].data, b->lines[b->cy + 1].len);
        b->lines[b->cy].len += b->lines[b->cy + 1].len;
        b->lines[b->cy].data[b->lines[b->cy].len] = '\0';
        ltxt_buf_delete_line(b, b->cy + 1);
        if (!b->modified) E.title_dirty = 1;
        b->modified = 1;
    }
}

static void ltxt_move_cursor(int key) {
    struct ltxt_buf *b;

    b = ltxt_cur_buf();
    switch (key) {
    case 1100:
        if (b->cy > 0) b->cy--;
        break;
    case 1101:
        if (b->cy < b->num_lines - 1) b->cy++;
        break;
    case 1102:
        b->cx++;
        break;
    case 1103:
        if (b->cx > 0) b->cx--;
        break;
    case 1000:
        b->cx = 0;
        break;
    case 1004:
        b->cx = b->lines[b->cy].len;
        break;
    case 1005:
        b->cy -= E.screen_rows - 3;
        if (b->cy < 0) b->cy = 0;
        break;
    case 1006:
        b->cy += E.screen_rows - 3;
        if (b->cy >= b->num_lines) b->cy = b->num_lines - 1;
        break;
    }
    ltxt_clamp_cursor();
}

static void ltxt_do_search(void) {
    struct ltxt_buf *b;
    int i;
    int j;
    int slen;

    b = ltxt_cur_buf();
    slen = E.search_len;
    if (slen <= 0) return;

    for (i = b->cy; i < b->num_lines; i++) {
        int start;

        start = (i == b->cy) ? b->cx + 1 : 0;
        for (j = start; j <= b->lines[i].len - slen; j++) {
            if (memcmp(b->lines[i].data + j, E.search_buf, slen) == 0) {
                b->cy = i;
                b->cx = j;
                return;
            }
        }
    }

    for (i = 0; i <= b->cy; i++) {
        int end_j;

        end_j = (i == b->cy) ? b->cx : b->lines[i].len - slen;
        for (j = 0; j <= end_j; j++) {
            if (j + slen > b->lines[i].len) break;
            if (memcmp(b->lines[i].data + j, E.search_buf, slen) == 0) {
                b->cy = i;
                b->cx = j;
                return;
            }
        }
    }
}

static void ltxt_do_replace_at_cursor(void) {
    struct ltxt_buf *b;
    struct ltxt_line *l;
    int slen;
    int rlen;
    int i;

    b = ltxt_cur_buf();
    slen = E.search_len;
    rlen = E.replace_len;
    if (slen <= 0) return;

    l = &b->lines[b->cy];
    if (b->cx + slen > l->len) return;
    if (memcmp(l->data + b->cx, E.search_buf, slen) != 0) return;

    if (rlen > slen) {
        ltxt_line_ensure(l, l->len + (rlen - slen) + 1);
        for (i = l->len; i >= b->cx + slen; i--)
            l->data[i + (rlen - slen)] = l->data[i];
    } else if (rlen < slen) {
        for (i = b->cx + rlen; i <= l->len - (slen - rlen); i++)
            l->data[i] = l->data[i + (slen - rlen)];
    }

    memcpy(l->data + b->cx, E.replace_buf, rlen);
    l->len += (rlen - slen);
    l->data[l->len] = '\0';
    b->cx += rlen;
    b->modified = 1;
}

static void ltxt_do_goto(void) {
    struct ltxt_buf *b;
    int line;
    int i;

    b = ltxt_cur_buf();
    line = 0;
    for (i = 0; i < E.prompt_len; i++) {
        if (E.prompt_buf[i] >= '0' && E.prompt_buf[i] <= '9')
            line = line * 10 + (E.prompt_buf[i] - '0');
    }
    if (line < 1) line = 1;
    if (line > b->num_lines) line = b->num_lines;
    b->cy = line - 1;
    b->cx = 0;
    ltxt_clamp_cursor();
}

static void ltxt_exec_command(void) {
    struct ltxt_buf *b;

    b = ltxt_cur_buf();
    E.prompt_buf[E.prompt_len] = '\0';

    if (strcmp(E.prompt_buf, "quit") == 0) {
        if (b->modified) {
            return;
        }
        E.running = 0;
    } else if (strcmp(E.prompt_buf, "quit!") == 0) {
        E.running = 0;
    } else if (strcmp(E.prompt_buf, "save") == 0) {
        if (ltxt_buf_save(b) < 0) {
            strncpy(E.msg, "Error: could not save file", sizeof(E.msg) - 1);
            E.msg[sizeof(E.msg) - 1] = '\0';
            E.msg_len = strlen(E.msg);
        }
        E.title_dirty = 1;
    } else if (strcmp(E.prompt_buf, "save-quit") == 0) {
        if (ltxt_buf_save(b) < 0) {
            strncpy(E.msg, "Error: could not save file", sizeof(E.msg) - 1);
            E.msg[sizeof(E.msg) - 1] = '\0';
            E.msg_len = strlen(E.msg);
        } else {
            E.running = 0;
        }
    } else if (strncmp(E.prompt_buf, "save ", 5) == 0) {
        strncpy(b->filename, E.prompt_buf + 5, sizeof(b->filename) - 1);
        b->filename[sizeof(b->filename) - 1] = '\0';
        if (ltxt_buf_save(b) < 0) {
            strncpy(E.msg, "Error: could not save file", sizeof(E.msg) - 1);
            E.msg[sizeof(E.msg) - 1] = '\0';
            E.msg_len = strlen(E.msg);
        }
        E.title_dirty = 1;
    } else if (strncmp(E.prompt_buf, "open ", 5) == 0) {
        if (E.num_tabs < LTXT_MAX_TABS) {
            ltxt_buf_init(&E.tabs[E.num_tabs]);
            ltxt_buf_load(&E.tabs[E.num_tabs], E.prompt_buf + 5);
            E.cur_tab = E.num_tabs;
            E.num_tabs++;
            E.title_dirty = 1;
        }
    } else if (strncmp(E.prompt_buf, "goto ", 5) == 0) {
        int i;
        int line;

        line = 0;
        for (i = 5; E.prompt_buf[i]; i++) {
            if (E.prompt_buf[i] >= '0' && E.prompt_buf[i] <= '9')
                line = line * 10 + (E.prompt_buf[i] - '0');
        }
        if (line >= 1 && line <= b->num_lines) {
            b->cy = line - 1;
            b->cx = 0;
        }
    } else if (strcmp(E.prompt_buf, "help") == 0) {
        strncpy(E.msg, "quit quit! save save-quit save <file> open <file> goto <n> help", sizeof(E.msg) - 1);
        E.msg[sizeof(E.msg) - 1] = '\0';
        E.msg_len = strlen(E.msg);
    }
}

static void ltxt_new_tab(void) {
    if (E.num_tabs >= LTXT_MAX_TABS) return;
    ltxt_buf_init(&E.tabs[E.num_tabs]);
    E.cur_tab = E.num_tabs;
    E.num_tabs++;
    E.title_dirty = 1;
}

static void ltxt_close_tab(void) {
    int i;

    if (E.num_tabs <= 1) {
        E.running = 0;
        return;
    }
    ltxt_buf_free(&E.tabs[E.cur_tab]);
    for (i = E.cur_tab; i < E.num_tabs - 1; i++)
        E.tabs[i] = E.tabs[i + 1];
    E.num_tabs--;
    if (E.cur_tab >= E.num_tabs) E.cur_tab = E.num_tabs - 1;
    E.title_dirty = 1;
}

static void ltxt_process_key(int key) {
    if (E.mode != MODE_EDIT) {
        if (key == '\x1b') {
            E.mode = MODE_EDIT;
            return;
        }
        if (key == '\r' || key == '\n') {
            E.prompt_buf[E.prompt_len] = '\0';
            switch (E.mode) {
            case MODE_SEARCH:
                memcpy(E.search_buf, E.prompt_buf, E.prompt_len + 1);
                E.search_len = E.prompt_len;
                ltxt_do_search();
                break;
            case MODE_REPLACE:
                memcpy(E.search_buf, E.prompt_buf, E.prompt_len + 1);
                E.search_len = E.prompt_len;
                E.mode = MODE_REPLACE_WITH;
                E.prompt_len = 0;
                E.prompt_buf[0] = '\0';
                return;
            case MODE_REPLACE_WITH:
                memcpy(E.replace_buf, E.prompt_buf, E.prompt_len + 1);
                E.replace_len = E.prompt_len;
                ltxt_do_search();
                ltxt_do_replace_at_cursor();
                break;
            case MODE_GOTO:
                ltxt_do_goto();
                break;
            case MODE_COMMAND:
                ltxt_exec_command();
                break;
            case MODE_SAVE_AS:
                strncpy(ltxt_cur_buf()->filename, E.prompt_buf, sizeof(ltxt_cur_buf()->filename) - 1);
                ltxt_cur_buf()->filename[sizeof(ltxt_cur_buf()->filename) - 1] = '\0';
                if (ltxt_buf_save(ltxt_cur_buf()) < 0) {
                    strncpy(E.msg, "Error: could not save file", sizeof(E.msg) - 1);
                    E.msg[sizeof(E.msg) - 1] = '\0';
                    E.msg_len = strlen(E.msg);
                }
                E.title_dirty = 1;
                break;
            default:
                break;
            }
            E.mode = MODE_EDIT;
            return;
        }
        if (key == 127 || key == 8) {
            if (E.prompt_len > 0) E.prompt_len--;
            return;
        }
        if (key >= 32 && key < 127) {
            if (E.prompt_len < LTXT_MAX_CMD - 1) {
                E.prompt_buf[E.prompt_len++] = (char)key;
            }
            return;
        }
        return;
    }

    if (key != LTXT_KEY_CTRL('q'))
        E.quit_count = 0;
    E.msg_len = 0;

    switch (key) {
    case LTXT_KEY_CTRL('q'):
        if (ltxt_cur_buf()->modified && E.quit_count == 0) {
            E.quit_count = 1;
            return;
        }
        E.running = 0;
        break;
    case LTXT_KEY_CTRL('s'):
        if (ltxt_cur_buf()->filename[0] == '\0') {
            E.mode = MODE_SAVE_AS;
            E.prompt_len = 0;
            return;
        }
        if (ltxt_buf_save(ltxt_cur_buf()) < 0) {
            strncpy(E.msg, "Error: could not save file", sizeof(E.msg) - 1);
            E.msg[sizeof(E.msg) - 1] = '\0';
            E.msg_len = strlen(E.msg);
        }
        E.title_dirty = 1;
        break;
    case LTXT_KEY_CTRL('f'):
        E.mode = MODE_SEARCH;
        E.prompt_len = 0;
        break;
    case LTXT_KEY_CTRL('r'):
        E.mode = MODE_REPLACE;
        E.prompt_len = 0;
        break;
    case LTXT_KEY_CTRL('g'):
        E.mode = MODE_GOTO;
        E.prompt_len = 0;
        break;
    case LTXT_KEY_CTRL('p'):
        E.mode = MODE_COMMAND;
        E.prompt_len = 0;
        break;
    case LTXT_KEY_CTRL('t'):
        ltxt_new_tab();
        break;
    case LTXT_KEY_CTRL('w'):
        ltxt_close_tab();
        break;
    case LTXT_KEY_CTRL('n'):
        if (E.cur_tab < E.num_tabs - 1) {
            E.cur_tab++;
            E.title_dirty = 1;
        }
        break;
    case LTXT_KEY_CTRL('b'):
        if (E.cur_tab > 0) {
            E.cur_tab--;
            E.title_dirty = 1;
        }
        break;
    case '\r':
    case '\n':
        ltxt_insert_newline();
        break;
    case 127:
    case 8:
        ltxt_delete_char();
        break;
    case 1003:
        ltxt_delete_forward();
        break;
    case '\t':
        ltxt_insert_char(' ');
        ltxt_insert_char(' ');
        ltxt_insert_char(' ');
        ltxt_insert_char(' ');
        break;
    case 1100:
    case 1101:
    case 1102:
    case 1103:
    case 1000:
    case 1004:
    case 1005:
    case 1006:
        ltxt_move_cursor(key);
        break;
    default:
        if (key >= 32 && key < 127)
            ltxt_insert_char(key);
        break;
    }
}

int cmd_ltxtedit(int argc, char **argv) {
    int i;
    int key;

    memset(&E, 0, sizeof(E));
    ltxt_get_screen_size();
    E.running = 1;
    E.mode = MODE_EDIT;

    ltxt_buf_init(&E.tabs[0]);
    E.num_tabs = 1;
    E.cur_tab = 0;

    if (argc >= 2) {
        for (i = 1; i < argc && i <= LTXT_MAX_TABS; i++) {
            if (i > 1) {
                ltxt_buf_init(&E.tabs[E.num_tabs]);
                E.num_tabs++;
            }
            ltxt_buf_load(&E.tabs[i - 1], argv[i]);
        }
    }

    ltxt_enable_raw();
    ltxt_puts("\x1b[?1049h\x1b[r\x1b[2J\x1b[H");
    E.title_dirty = 1;

    while (E.running) {
        ltxt_scroll();
        ltxt_draw();
        key = ltxt_read_key();
        if (key < 0) continue;
        ltxt_process_key(key);
    }

    ltxt_puts("\x1b[?1049l\x1b[2J\x1b[H");
    ltxt_disable_raw();

    for (i = 0; i < E.num_tabs; i++) {
        if (E.tabs[i].active)
            ltxt_buf_free(&E.tabs[i]);
    }

    return 0;
}
