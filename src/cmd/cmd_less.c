#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include "cu.h"

struct less_lines {
    char **items;
    int count;
    int capacity;
};

static struct termios less_saved_termios;
static int less_terminal_fd = -1;
static int less_terminal_active;

static void less_restore_terminal(void)
{
    if (less_terminal_active) {
        tcsetattr(less_terminal_fd, TCSANOW, &less_saved_termios);
        write(STDOUT_FILENO, "\033[?25h", sizeof("\033[?25h") - 1);
        less_terminal_active = 0;
    }
}

static void less_signal_handler(int signal_number)
{
    less_restore_terminal();
    write(STDOUT_FILENO, "\033[?25h\033[0m\033[2J\033[H", sizeof("\033[?25h\033[0m\033[2J\033[H") - 1);
    _exit(128 + signal_number);
}

static void less_free_lines(struct less_lines *lines)
{
    int i;

    for (i = 0; i < lines->count; i++) free(lines->items[i]);
    free(lines->items);
    lines->items = NULL;
    lines->count = 0;
    lines->capacity = 0;
}

static int less_add_line(struct less_lines *lines, const char *data, int length)
{
    char **new_items;
    char *line;
    int new_capacity;

    if (lines->count >= lines->capacity) {
        new_capacity = lines->capacity ? lines->capacity * 2 : 128;
        new_items = (char **)realloc(lines->items, (size_t)new_capacity * sizeof(char *));
        if (!new_items) return -1;
        lines->items = new_items;
        lines->capacity = new_capacity;
    }
    line = (char *)malloc((size_t)length + 1);
    if (!line) return -1;
    memcpy(line, data, (size_t)length);
    line[length] = '\0';
    lines->items[lines->count++] = line;
    return 0;
}

static int less_read_lines(FILE *file, struct less_lines *lines)
{
    char *buffer;
    char *new_buffer;
    int capacity;
    int length;
    int c;
    int result;

    buffer = NULL;
    capacity = 0;
    length = 0;
    result = 0;
    while ((c = fgetc(file)) != EOF) {
        if (c == '\n') {
            result = less_add_line(lines, buffer ? buffer : "", length);
            length = 0;
            if (result < 0) break;
            continue;
        }
        if (length + 1 >= capacity) {
            capacity = capacity ? capacity * 2 : 256;
            new_buffer = (char *)realloc(buffer, (size_t)capacity);
            if (!new_buffer) {
                result = -1;
                break;
            }
            buffer = new_buffer;
        }
        buffer[length++] = (char)c;
    }
    if (result == 0 && length > 0) result = less_add_line(lines, buffer, length);
    free(buffer);
    return result;
}

static void less_print_visible_line(const char *line, int columns, int numbered, int line_number)
{
    int used;
    int i;
    int spaces;

    used = 0;
    if (numbered) {
        printf("%6d  ", line_number);
        used = 8;
    }
    for (i = 0; line[i] && used < columns; i++) {
        if (line[i] == '\t') {
            spaces = 8 - (used % 8);
            while (spaces > 0 && used < columns) {
                putchar(' ');
                spaces--;
                used++;
            }
        } else if ((unsigned char)line[i] >= 32 || line[i] == '\r') {
            if (line[i] != '\r') {
                putchar(line[i]);
                used++;
            }
        }
    }
    fputs("\033[K\n", stdout);
}

static void less_draw(struct less_lines *lines, const char *name, int top,
                      int rows, int columns, int numbered)
{
    int visible_rows;
    int i;
    int index;
    int percent;

    visible_rows = rows - 1;
    if (visible_rows < 1) visible_rows = 1;
    fputs("\033[H", stdout);
    for (i = 0; i < visible_rows; i++) {
        index = top + i;
        if (index < lines->count)
            less_print_visible_line(lines->items[index], columns, numbered, index + 1);
        else
            fputs("~\033[K\n", stdout);
    }
    percent = lines->count ? (top + visible_rows) * 100 / lines->count : 100;
    if (percent > 100) percent = 100;
    printf("\033[7m%s  lines %d-%d/%d  %d%%  q:quit h:help\033[K\033[0m",
           name, lines->count ? top + 1 : 0,
           top + visible_rows < lines->count ? top + visible_rows : lines->count,
           lines->count, percent);
    fflush(stdout);
}

static void less_draw_help(int rows)
{
    int i;

    fputs("\033[H\033[2J", stdout);
    puts("less keys");
    puts("");
    puts("  Space, f, Page Down  forward one page");
    puts("  b, Page Up           backward one page");
    puts("  j, Down, Enter       forward one line");
    puts("  k, Up                 backward one line");
    puts("  g                     go to first line");
    puts("  G                     go to last page");
    puts("  q                     quit");
    for (i = 10; i < rows - 1; i++) putchar('\n');
    fputs("\033[7mPress any key to return\033[K\033[0m", stdout);
    fflush(stdout);
}

static int less_read_key(int fd)
{
    unsigned char c;
    unsigned char sequence[2];
    int result;

    result = read(fd, &c, 1);
    if (result <= 0) return 'q';
    if (c != 27) return c;
    if (read(fd, &sequence[0], 1) <= 0) return 27;
    if (read(fd, &sequence[1], 1) <= 0) return 27;
    if (sequence[0] != '[') return 27;
    if (sequence[1] == 'A') return 'k';
    if (sequence[1] == 'B') return 'j';
    if (sequence[1] == '5') {
        read(fd, &c, 1);
        return 'b';
    }
    if (sequence[1] == '6') {
        read(fd, &c, 1);
        return 'f';
    }
    return 0;
}

static int less_page(struct less_lines *lines, const char *name, int numbered)
{
    struct termios raw;
    struct winsize size;
    int rows;
    int columns;
    int page_rows;
    int top;
    int maximum_top;
    int key;

    less_terminal_fd = STDIN_FILENO;
    if (tcgetattr(less_terminal_fd, &less_saved_termios) < 0) return -1;

    raw = less_saved_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(less_terminal_fd, TCSANOW, &raw) < 0) return -1;
    less_terminal_active = 1;
    write(STDOUT_FILENO, "\033[?25l", sizeof("\033[?25l") - 1);
    atexit(less_restore_terminal);
    signal(SIGINT, less_signal_handler);
    signal(SIGTERM, less_signal_handler);

    rows = 24;
    columns = 80;
    if (tcgetwinsize(less_terminal_fd, &size) == 0) {
        if (size.ws_row > 1) rows = size.ws_row;
        if (size.ws_col > 0) columns = size.ws_col;
    }
    page_rows = rows - 1;
    top = 0;
    maximum_top = lines->count > page_rows ? lines->count - page_rows : 0;

    fputs("\033[2J", stdout);
    for (;;) {
        less_draw(lines, name, top, rows, columns, numbered);
        key = less_read_key(less_terminal_fd);
        if (key == 'q' || key == 'Q') break;
        if (key == ' ' || key == 'f') {
            top += page_rows;
        } else if (key == 'b') {
            top -= page_rows;
        } else if (key == 'j' || key == '\n' || key == '\r') {
            top++;
        } else if (key == 'k') {
            top--;
        } else if (key == 'g') {
            top = 0;
        } else if (key == 'G') {
            top = maximum_top;
        } else if (key == 'h') {
            less_draw_help(rows);
            less_read_key(less_terminal_fd);
        }
        if (top < 0) top = 0;
        if (top > maximum_top) top = maximum_top;
    }

    less_restore_terminal();
    fputs("\033[?25h\033[0m\033[2J\033[H", stdout);
    less_terminal_fd = -1;
    return 0;
}

int cmd_less(int argc, char **argv)
{
    struct less_lines lines;
    const char *path;
    FILE *file;
    int numbered;
    int i;
    int result;

    memset(&lines, 0, sizeof(lines));
    path = NULL;
    numbered = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: less [-N] [FILE]");
            puts("View text one screen at a time.");
            puts("  -N  display line numbers");
            return 0;
        }
        if (strcmp(argv[i], "-N") == 0) numbered = 1;
        else if (argv[i][0] == '-') {
            fprintf(stderr, "less: unknown option '%s'\n", argv[i]);
            return 1;
        } else if (!path) path = argv[i];
        else {
            fprintf(stderr, "less: extra operand '%s'\n", argv[i]);
            return 1;
        }
    }

    file = path ? fopen(path, "r") : stdin;
    if (!file) {
        fprintf(stderr, "less: cannot open '%s'\n", path);
        return 1;
    }
    result = less_read_lines(file, &lines);
    if (file != stdin) fclose(file);
    if (result < 0) {
        fprintf(stderr, "less: out of memory\n");
        less_free_lines(&lines);
        return 1;
    }

    if (!isatty(STDOUT_FILENO) || !path) {
        for (i = 0; i < lines.count; i++) printf("%s\n", lines.items[i]);
        less_free_lines(&lines);
        return 0;
    }

    result = less_page(&lines, path ? path : "standard input", numbered);
    less_free_lines(&lines);
    if (result < 0) {
        fprintf(stderr, "less: cannot access terminal\n");
        return 1;
    }
    return 0;
}
