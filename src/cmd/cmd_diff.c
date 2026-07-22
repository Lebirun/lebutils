#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cu.h"

#define DIFF_MAX_LINES 2048

struct diff_lines {
    char **items;
    int count;
    int capacity;
};

static void diff_free_lines(struct diff_lines *lines)
{
    int i;

    if (!lines) return;
    for (i = 0; i < lines->count; i++) free(lines->items[i]);
    free(lines->items);
    lines->items = NULL;
    lines->count = 0;
    lines->capacity = 0;
}

static int diff_add_line(struct diff_lines *lines, const char *data, int length)
{
    char **new_items;
    char *line;
    int new_capacity;

    if (lines->count >= DIFF_MAX_LINES) return -2;
    if (lines->count >= lines->capacity) {
        new_capacity = lines->capacity ? lines->capacity * 2 : 64;
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

static int diff_read_lines(const char *path, struct diff_lines *lines)
{
    FILE *file;
    char *buffer;
    char *new_buffer;
    int capacity;
    int length;
    int c;
    int result;

    file = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
    if (!file) return -1;

    buffer = NULL;
    capacity = 0;
    length = 0;
    result = 0;
    while ((c = fgetc(file)) != EOF) {
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
        if (c == '\n') {
            result = diff_add_line(lines, buffer, length);
            length = 0;
            if (result < 0) break;
        }
    }
    if (result == 0 && length > 0) result = diff_add_line(lines, buffer, length);
    free(buffer);
    if (file != stdin) fclose(file);
    return result;
}

static void diff_print_range(int first, int last)
{
    if (first == last) printf("%d", first);
    else printf("%d,%d", first, last);
}

static void diff_print_line(const char *prefix, const char *line)
{
    size_t length;

    fputs(prefix, stdout);
    fputs(line, stdout);
    length = strlen(line);
    if (length == 0 || line[length - 1] != '\n') {
        putchar('\n');
        puts("\\ No newline at end of file");
    }
}

static int diff_compare(const char *left_name, const char *right_name,
                        struct diff_lines *left, struct diff_lines *right,
                        int brief)
{
    unsigned short *table;
    size_t columns;
    size_t cells;
    int i;
    int j;
    int left_start;
    int right_start;
    int removed;
    int added;
    int different;

    columns = (size_t)right->count + 1;
    cells = ((size_t)left->count + 1) * columns;
    table = (unsigned short *)calloc(cells, sizeof(unsigned short));
    if (!table) {
        fprintf(stderr, "diff: out of memory\n");
        return 2;
    }

    for (i = left->count - 1; i >= 0; i--) {
        for (j = right->count - 1; j >= 0; j--) {
            if (strcmp(left->items[i], right->items[j]) == 0) {
                table[(size_t)i * columns + (size_t)j] =
                    (unsigned short)(table[(size_t)(i + 1) * columns + (size_t)(j + 1)] + 1);
            } else if (table[(size_t)(i + 1) * columns + (size_t)j] >=
                       table[(size_t)i * columns + (size_t)(j + 1)]) {
                table[(size_t)i * columns + (size_t)j] =
                    table[(size_t)(i + 1) * columns + (size_t)j];
            } else {
                table[(size_t)i * columns + (size_t)j] =
                    table[(size_t)i * columns + (size_t)(j + 1)];
            }
        }
    }

    different = table[0] != (unsigned short)left->count || left->count != right->count;
    if (!different) {
        free(table);
        return 0;
    }
    if (brief) {
        printf("Files %s and %s differ\n", left_name, right_name);
        free(table);
        return 1;
    }

    i = 0;
    j = 0;
    while (i < left->count || j < right->count) {
        if (i < left->count && j < right->count &&
            strcmp(left->items[i], right->items[j]) == 0) {
            i++;
            j++;
            continue;
        }

        left_start = i;
        right_start = j;
        while (i < left->count || j < right->count) {
            if (i < left->count && j < right->count &&
                strcmp(left->items[i], right->items[j]) == 0) break;
            if (j >= right->count ||
                (i < left->count &&
                 table[(size_t)(i + 1) * columns + (size_t)j] >=
                 table[(size_t)i * columns + (size_t)(j + 1)])) {
                i++;
            } else {
                j++;
            }
        }

        removed = i - left_start;
        added = j - right_start;
        if (removed > 0 && added > 0) {
            diff_print_range(left_start + 1, i);
            putchar('c');
            diff_print_range(right_start + 1, j);
            putchar('\n');
            for (removed = left_start; removed < i; removed++)
                diff_print_line("< ", left->items[removed]);
            puts("---");
            for (added = right_start; added < j; added++)
                diff_print_line("> ", right->items[added]);
        } else if (removed > 0) {
            diff_print_range(left_start + 1, i);
            printf("d%d\n", right_start);
            for (removed = left_start; removed < i; removed++)
                diff_print_line("< ", left->items[removed]);
        } else {
            printf("%da", left_start);
            diff_print_range(right_start + 1, j);
            putchar('\n');
            for (added = right_start; added < j; added++)
                diff_print_line("> ", right->items[added]);
        }
    }

    free(table);
    return 1;
}

int cmd_diff(int argc, char **argv)
{
    struct diff_lines left;
    struct diff_lines right;
    const char *left_name;
    const char *right_name;
    int brief;
    int i;
    int result;

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    left_name = NULL;
    right_name = NULL;
    brief = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: diff [-q] FILE1 FILE2");
            puts("Compare two text files line by line.");
            puts("  -q, --brief  report only whether files differ");
            return 0;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--brief") == 0) {
            brief = 1;
        } else if (argv[i][0] == '-' && strcmp(argv[i], "-") != 0) {
            fprintf(stderr, "diff: unknown option '%s'\n", argv[i]);
            return 2;
        } else if (!left_name) {
            left_name = argv[i];
        } else if (!right_name) {
            right_name = argv[i];
        } else {
            fprintf(stderr, "diff: extra operand '%s'\n", argv[i]);
            return 2;
        }
    }

    if (!left_name || !right_name ||
        (strcmp(left_name, "-") == 0 && strcmp(right_name, "-") == 0)) {
        fprintf(stderr, "diff: two file operands are required\n");
        return 2;
    }

    result = diff_read_lines(left_name, &left);
    if (result < 0) {
        if (result == -2) fprintf(stderr, "diff: '%s' exceeds %d lines\n", left_name, DIFF_MAX_LINES);
        else fprintf(stderr, "diff: cannot read '%s'\n", left_name);
        diff_free_lines(&left);
        return 2;
    }
    result = diff_read_lines(right_name, &right);
    if (result < 0) {
        if (result == -2) fprintf(stderr, "diff: '%s' exceeds %d lines\n", right_name, DIFF_MAX_LINES);
        else fprintf(stderr, "diff: cannot read '%s'\n", right_name);
        diff_free_lines(&left);
        diff_free_lines(&right);
        return 2;
    }

    result = diff_compare(left_name, right_name, &left, &right, brief);
    diff_free_lines(&left);
    diff_free_lines(&right);
    return result;
}
