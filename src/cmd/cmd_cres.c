#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cu.h"

int cmd_cres(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: cres <width> <height> [refresh_rate]\n");
        fprintf(stderr, "cres list\n");
        fprintf(stderr, "cres info\n");
        fprintf(stderr, "Note that changing resolution (including custom resolutions) may not be supported on all hardware.\n");
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        unsigned int caps[16] = {0};
        int rc = fb_getcaps(caps, 16);
        if (rc != 0) {
            fprintf(stderr, "Failed to query framebuffer caps (%d)\n", rc);
            return 1;
        }

        unsigned int cur_w = caps[0];
        unsigned int cur_h = caps[1];
        unsigned int font_w = caps[6] ? caps[6] : 8;
        unsigned int font_h = caps[7] ? caps[7] : 16;
        unsigned int hw_w = caps[8] ? caps[8] : cur_w;
        unsigned int hw_h = caps[9] ? caps[9] : cur_h;
        unsigned int flags = caps[12];
        unsigned int refresh = caps[13] ? caps[13] : 60;

        if (flags & 1u) {
            struct mode { unsigned int width; unsigned int height; } modes[128];
            int mcount = 0;

            unsigned int cand_h = hw_h;
            int h_attempts = 0;
            while (cand_h >= font_h && h_attempts < 16 && mcount < (int)(sizeof(modes)/sizeof(modes[0]))) {
                unsigned int cand_w = hw_w;
                int w_attempts = 0;
                while (cand_w >= font_w && w_attempts < 16 && mcount < (int)(sizeof(modes)/sizeof(modes[0]))) {
                    int res = fb_set_mode(cand_w, cand_h, refresh);
                    if (res == 0) {
                        int found = 0;
                        for (int i = 0; i < mcount; i++) {
                            if (modes[i].width == cand_w && modes[i].height == cand_h) {
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            modes[mcount].width = cand_w;
                            modes[mcount].height = cand_h;
                            mcount++;
                        }
                        fb_set_mode(cur_w, cur_h, refresh);
                    }
                    unsigned int delta = cand_w % font_w;
                    if (delta == 0) delta = font_w;
                    if (delta >= cand_w) break;
                    cand_w -= delta;
                    w_attempts++;
                }
                unsigned int delta_h = cand_h % font_h;
                if (delta_h == 0) delta_h = font_h;
                if (delta_h >= cand_h) break;
                cand_h -= delta_h;
                h_attempts++;
            }

            if (mcount > 0) {
                printf("Native supported resolutions:\n");
                for (int i = 0; i < mcount; i++) {
                    printf("%ux%u\n", modes[i].width, modes[i].height);
                }
                return 0;
            }

            printf("No native modes discovered via hardware probing. Showing fallback list:\n");
            printf("640x480\n800x600\n1024x768\n1280x1024\n1920x1080\n");
            return 0;
        }

        printf("Hardware mode switching not supported on this device. Showing fallback list:\n");
        printf("640x480\n800x600\n1024x768\n1280x1024\n1920x1080\n");
        return 0;
    }

    if (strcmp(argv[1], "info") == 0) {
        unsigned int caps[16] = {0};
        int rc = fb_getcaps(caps, 16);
        if (rc != 0) {
            fprintf(stderr, "Failed to query framebuffer info (%d)\n", rc);
            return 1;
        }
        printf("Resolution: %ux%u @ %u bpp\n", caps[0], caps[1], caps[2]);
        printf("Pitch: %u (hw pitch %u)\n", caps[3], caps[10]);
        printf("Text: %u rows x %u cols (font %ux%u)\n", caps[5], caps[4], caps[6], caps[7]);
        printf("Hardware: %ux%u\n", caps[8], caps[9]);
        printf("HW switch: %s\n", (caps[12] & 1u) ? "yes (BGA)" : "no");
        return 0;
    }

    char *endptr;
    long width = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || width <= 0 || width > 16384) {
        fprintf(stderr, "Error: Invalid width '%s'\n", argv[1]);
        return 1;
    }

    if (argc < 3) {
        fprintf(stderr, "Error: Height argument required\n");
        return 1;
    }

    long height = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || height <= 0 || height > 16384) {
        fprintf(stderr, "Error: Invalid height '%s'\n", argv[2]);
        return 1;
    }

    long refresh_rate = 60;
    if (argc >= 4) {
        refresh_rate = strtol(argv[3], &endptr, 10);
        if (*endptr != '\0' || refresh_rate <= 0 || refresh_rate > 200) {
            fprintf(stderr, "Error: Invalid refresh rate '%s'\n", argv[3]);
            return 1;
        }
    }

    printf("Setting resolution to %ldx%ld @ %ldHz\n", width, height, refresh_rate);
    
    int result = fb_set_mode((unsigned int)width, (unsigned int)height, (unsigned int)refresh_rate);
    
    if (result == 0) {
        return 0;
    } else if (result == -2) {
        fprintf(stderr, "Error: Resolution out of supported range\n");
        return 1;
    } else if (result == -3) {
        fprintf(stderr, "Error: Framebuffer not initialized\n");
        return 1;
    } else if (result == -4) {
        fprintf(stderr, "Error: Resolution exceeds physical framebuffer size\n");
        fprintf(stderr, "Use 'cres list' to see limits\n");
        return 1;
    } else {
        fprintf(stderr, "Error: Failed to change resolution (error code: %d)\n", result);
        return 1;
    }
}
