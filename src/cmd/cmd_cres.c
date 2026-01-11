#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cu.h"

int cmd_cres(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: cres <width> <height> [refresh_rate]\n");
        fprintf(stderr, "       cres list\n");
        fprintf(stderr, "       cres info\n");
        fprintf(stderr, "\nNotes:\n");
        fprintf(stderr, "- On some machines/emulators the driver can switch hardware modes (BGA).\n");
        fprintf(stderr, "- Otherwise this clamps to the framebuffer provided at boot.\n");
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        unsigned int caps[16] = {0};
        int rc = fb_getcaps(caps, 16);
        if (rc != 0) {
            fprintf(stderr, "Failed to query framebuffer caps (%d)\n", rc);
            return 1;
        }

        unsigned int w = caps[0];
        unsigned int h = caps[1];
        unsigned int bpp = caps[2];
        unsigned int pitch = caps[3];
        unsigned int cols = caps[4];
        unsigned int rows = caps[5];
        unsigned int font_w = caps[6];
        unsigned int font_h = caps[7];
        unsigned int hw_w = caps[8];
        unsigned int hw_h = caps[9];
        unsigned int hw_pitch = caps[10];
        unsigned int vram = caps[11];
        unsigned int flags = caps[12];

        printf("Current:  %ux%u @ %u bpp (pitch %u)  text %ux%u  font %ux%u\n",
               w, h, bpp, pitch, cols, rows, font_w, font_h);

        printf("Hardware: %ux%u (pitch %u)", hw_w, hw_h, hw_pitch);
        if (vram) printf("  VRAM %u bytes", vram);
        printf("\n");

        if (flags & 1u) {
            printf("Hardware mode switching: supported (BGA)\n");
            if (vram && bpp) {
                unsigned int bytespp = bpp / 8u;
                if (bytespp == 0) bytespp = 4;
                unsigned int max_pixels = vram / bytespp;
                printf("Max pixels (rough): %u (VRAM-limited, not a mode list)\n", max_pixels);
            }
        } else {
            printf("Hardware mode switching: not available\n");
            printf("Limits: requests larger than boot framebuffer will fail\n");
        }

        printf("\nTry: cres <w> <h> [hz]\n");
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

    printf("Setting resolution to %ldx%ld @ %ldHz...\n", width, height, refresh_rate);
    
    int result = fb_set_mode((unsigned int)width, (unsigned int)height, (unsigned int)refresh_rate);
    
    if (result == 0) {
        printf("Resolution changed successfully.\n");
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
