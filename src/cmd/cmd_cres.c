#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cu.h"

#define FB_CAPS_WORDS 1056
#define MAX_DIMENSION 16384u
#define MAX_REFRESH_RATE 200u

enum {
    CAP_CUR_WIDTH = 0,
    CAP_CUR_HEIGHT,
    CAP_BPP,
    CAP_PITCH,
    CAP_COLS,
    CAP_ROWS,
    CAP_FONT_WIDTH,
    CAP_FONT_HEIGHT,
    CAP_MAX_WIDTH,
    CAP_MAX_HEIGHT,
    CAP_MAX_PITCH,
    CAP_VRAM_BYTES,
    CAP_FLAGS,
    CAP_REFRESH_RATE,
    CAP_MODE_COUNT,
    CAP_RESERVED,
    CAP_FIRST_MODE = 16,
};

static void print_usage(void) {
    printf("Usage: cres <width> <height> [refresh_rate]\n");
    printf("       cres list\n");
    printf("       cres info\n");
    printf("cres list reports the hardware-supported modes without switching any mode.\n");
}

static int parse_positive(const char *input, const char *label, unsigned int limit, unsigned int *out) {
    char *endptr = NULL;
    long value = strtol(input, &endptr, 10);
    if (*input == '\0' || *endptr != '\0' || value <= 0 || (unsigned long)value > limit) {
        fprintf(stderr, "Invalid %s '%s' (must be 1..%u)\n", label, input, limit);
        return -1;
    }
    *out = (unsigned int)value;
    return 0;
}

static int run_info(void) {
    unsigned int caps[64];
    unsigned int font_w, font_h, hw_w, hw_h;
    int rc;
    const char *hw_switch;

    memset(caps, 0, sizeof(caps));
    rc = fb_getcaps(caps, sizeof(caps) / sizeof(caps[0]));
    if (rc != 0) {
        fprintf(stderr, "Failed to query framebuffer caps (%d)\n", rc);
        return 1;
    }

    font_w = caps[CAP_FONT_WIDTH] ? caps[CAP_FONT_WIDTH] : 8;
    font_h = caps[CAP_FONT_HEIGHT] ? caps[CAP_FONT_HEIGHT] : 16;
    hw_w = caps[CAP_MAX_WIDTH] ? caps[CAP_MAX_WIDTH] : caps[CAP_CUR_WIDTH];
    hw_h = caps[CAP_MAX_HEIGHT] ? caps[CAP_MAX_HEIGHT] : caps[CAP_CUR_HEIGHT];
    hw_switch = (caps[CAP_FLAGS] & 1u) ? "supported (BGA/VESA)" : "not supported";

    printf("Current resolution   : %ux%u @ %uHz\n", caps[CAP_CUR_WIDTH], caps[CAP_CUR_HEIGHT], caps[CAP_REFRESH_RATE]);
    printf("Text layout          : %u cols x %u rows (font %ux%u)\n", caps[CAP_COLS], caps[CAP_ROWS], font_w, font_h);
    printf("Hardware ceiling     : %ux%u (pitch %u bytes)\n", hw_w, hw_h, caps[CAP_MAX_PITCH]);
    printf("Framebuffer memory   : %u bytes\n", caps[CAP_VRAM_BYTES]);
    printf("Hardware mode switch : %s\n", hw_switch);
    printf("Reported mode count  : %u\n", caps[CAP_MODE_COUNT]);

    return 0;
}

static int run_list(void) {
    unsigned int caps[FB_CAPS_WORDS];
    unsigned int cap_words, font_w, font_h, hw_w, hw_h;
    unsigned int reported_modes, max_modes, cap_modes, printed, i, idx, mode_w, mode_h;
    int rc;
    const char *hw_switch;

    memset(caps, 0, sizeof(caps));
    cap_words = sizeof(caps) / sizeof(caps[0]);
    rc = fb_getcaps(caps, cap_words);
    if (rc != 0) {
        fprintf(stderr, "Failed to query framebuffer caps (%d)\n", rc);
        return 1;
    }

    font_w = caps[CAP_FONT_WIDTH] ? caps[CAP_FONT_WIDTH] : 8;
    font_h = caps[CAP_FONT_HEIGHT] ? caps[CAP_FONT_HEIGHT] : 16;
    hw_w = caps[CAP_MAX_WIDTH] ? caps[CAP_MAX_WIDTH] : caps[CAP_CUR_WIDTH];
    hw_h = caps[CAP_MAX_HEIGHT] ? caps[CAP_MAX_HEIGHT] : caps[CAP_CUR_HEIGHT];
    hw_switch = (caps[CAP_FLAGS] & 1u) ? "supported (BGA/VESA)" : "not supported";

    printf("Current resolution   : %ux%u @ %uHz\n", caps[CAP_CUR_WIDTH], caps[CAP_CUR_HEIGHT], caps[CAP_REFRESH_RATE]);
    printf("Hardware ceiling     : %ux%u\n", hw_w, hw_h);
    printf("Text layout          : %u cols x %u rows (font %ux%u)\n", caps[CAP_COLS], caps[CAP_ROWS], font_w, font_h);
    printf("Hardware mode switching: %s\n", hw_switch);

    reported_modes = caps[CAP_MODE_COUNT];
    max_modes = (cap_words - CAP_FIRST_MODE) / 2;
    if (reported_modes == 0) {
        printf("Mode list: unavailable\n");
        return 0;
    }

    cap_modes = reported_modes;
    if (cap_modes > max_modes) {
        cap_modes = max_modes;
    }
    printf("Mode list (%u entries):\n", reported_modes);
    printed = 0;
    for (i = 0; i < cap_modes; i++) {
        idx = CAP_FIRST_MODE + i * 2;
        mode_w = caps[idx];
        mode_h = caps[idx + 1];
        if (mode_w == 0 || mode_h == 0) {
            continue;
        }
        printf("  %ux%u\n", mode_w, mode_h);
        printed++;
    }
    if (printed == 0) {
        printf("  (no valid entries reported)\n");
    }
    if (reported_modes > max_modes) {
        printf("  (list truncated to %u entries because of buffer size)\n", max_modes);
    }

    return 0;
}

int cmd_cres(int argc, char **argv) {
    unsigned int width, height, refresh;
    int result;

    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        return run_list();
    }
    if (strcmp(argv[1], "info") == 0) {
        return run_info();
    }

    if (argc < 3) {
        fprintf(stderr, "Error: Height argument required\n");
        print_usage();
        return 1;
    }

    width = 0;
    height = 0;
    refresh = 60;

    if (parse_positive(argv[1], "width", MAX_DIMENSION, &width) != 0) {
        return 1;
    }
    if (parse_positive(argv[2], "height", MAX_DIMENSION, &height) != 0) {
        return 1;
    }
    if (argc >= 4) {
        if (parse_positive(argv[3], "refresh rate", MAX_REFRESH_RATE, &refresh) != 0) {
            return 1;
        }
    }

    printf("Setting resolution to %ux%u @ %uHz\n", width, height, refresh);
    result = fb_set_mode(width, height, refresh);

    switch (result) {
        case 0:
            return 0;
        case -2:
            fprintf(stderr, "Error: Resolution exceeds supported range\n");
            return 1;
        case -3:
            fprintf(stderr, "Error: Framebuffer not initialized\n");
            return 1;
        case -4:
            fprintf(stderr, "Error: Resolution exceeds physical framebuffer size\n");
            fprintf(stderr, "Use 'cres list' to inspect supported modes\n");
            return 1;
        case -5:
            fprintf(stderr, "Error: Not enough framebuffer memory for requested mode\n");
            return 1;
        case -6:
            fprintf(stderr, "Error: Internal framebuffer state invalid\n");
            return 1;
        default:
            fprintf(stderr, "Error: Failed to change resolution (error code: %d)\n", result);
            return 1;
    }
}
