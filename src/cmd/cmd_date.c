#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "cu.h"

struct tz_entry {
    const char *name;
    int offset_hours;
    int offset_minutes;
};

static const struct tz_entry timezones[] = {
    {"UTC", 0, 0},
    {"GMT", 0, 0},
    {"WET", 0, 0},
    {"GMT0", 0, 0},
    {"GMT+0", 0, 0},
    {"GMT-0", 0, 0},
    {"CET", 1, 0},
    {"MET", 1, 0},
    {"Europe/Berlin", 1, 0},
    {"Europe/Paris", 1, 0},
    {"Europe/London", 0, 0},
    {"Europe/Rome", 1, 0},
    {"Europe/Madrid", 1, 0},
    {"Europe/Amsterdam", 1, 0},
    {"Europe/Vienna", 1, 0},
    {"Europe/Brussels", 1, 0},
    {"Europe/Prague", 1, 0},
    {"Europe/Budapest", 1, 0},
    {"Europe/Warsaw", 1, 0},
    {"Europe/Athens", 2, 0},
    {"Europe/Helsinki", 2, 0},
    {"Europe/Istanbul", 3, 0},
    {"Europe/Moscow", 3, 0},
    {"Europe/Dubai", 4, 0},
    {"Europe/Bangkok", 7, 0},
    {"Europe/Hong_Kong", 8, 0},
    {"Europe/Tokyo", 9, 0},
    {"Europe/Sydney", 10, 0},
    {"EST", -5, 0},
    {"CST", -6, 0},
    {"MST", -7, 0},
    {"PST", -8, 0},
    {"EST5EDT", -5, 0},
    {"CST6CDT", -6, 0},
    {"MST7MDT", -7, 0},
    {"PST8PDT", -8, 0},
    {"America/New_York", -5, 0},
    {"America/Chicago", -6, 0},
    {"America/Denver", -7, 0},
    {"America/Los_Angeles", -8, 0},
    {"America/Anchorage", -9, 0},
    {"America/Toronto", -5, 0},
    {"America/Mexico_City", -6, 0},
    {"America/argentina/Buenos_Aires", -3, 0},
    {"America/Sao_Paulo", -3, 0},
    {"Africa/Cairo", 2, 0},
    {"Africa/Lagos", 1, 0},
    {"Africa/Johannesburg", 2, 0},
    {"Asia/Shanghai", 8, 0},
    {"Asia/Tokyo", 9, 0},
    {"Asia/Seoul", 9, 0},
    {"Asia/Bangkok", 7, 0},
    {"Asia/Singapore", 8, 0},
    {"Asia/Hong_Kong", 8, 0},
    {"Asia/Mumbai", 5, 30},
    {"Asia/Kolkata", 5, 30},
    {"Asia/Jakarta", 7, 0},
    {"Australia/Sydney", 10, 0},
    {"Australia/Melbourne", 10, 0},
    {"Australia/Brisbane", 10, 0},
    {"Pacific/Auckland", 12, 0},
    {NULL, 0, 0}
};

static int parse_gmt_offset(const char *tz_str, int *hours, int *minutes) {
    const char *p;
    int sign;
    int h, m;

    if (tz_str[0] != 'G' || tz_str[1] != 'M' || tz_str[2] != 'T') {
        return 0;
    }

    p = tz_str + 3;
    if (*p == '\0') {
        *hours = 0;
        *minutes = 0;
        return 1;
    }

    if (*p == '+') {
        sign = 1;
        p++;
    } else if (*p == '-') {
        sign = -1;
        p++;
    } else {
        return 0;
    }

    h = 0;
    while (*p >= '0' && *p <= '9') {
        h = h * 10 + (*p - '0');
        p++;
    }

    m = 0;
    if (*p == ':') {
        p++;
        while (*p >= '0' && *p <= '9') {
            m = m * 10 + (*p - '0');
            p++;
        }
    }

    if (*p != '\0' || h > 14 || m > 59) {
        return 0;
    }

    *hours = sign * h;
    *minutes = sign * m;
    return 1;
}

static const struct tz_entry *lookup_timezone(const char *tz_str) {
    int i;

    for (i = 0; timezones[i].name; i++) {
        if (strcasecmp(timezones[i].name, tz_str) == 0) {
            return &timezones[i];
        }
    }
    return NULL;
}

static int get_timezone_offset(const char *tz_str, int *hours, int *minutes) {
    int h, m;
    const struct tz_entry *tz;

    h = 0;
    m = 0;

    if (parse_gmt_offset(tz_str, &h, &m)) {
        *hours = h;
        *minutes = m;
        return 1;
    }

    tz = lookup_timezone(tz_str);
    if (tz) {
        *hours = tz->offset_hours;
        *minutes = tz->offset_minutes;
        return 1;
    }

    return 0;
}

int cmd_date(int argc, char **argv) {
    time_t now;
    struct tm *tm_info;
    struct tm adjusted_tm;
    char buffer[128];
    int i;
    const char *timezone;
    int tz_hours, tz_minutes;
    int offset_seconds;
    time_t adjusted_time;

    timezone = NULL;

    for (i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--timezone") == 0 || strcmp(argv[i], "-t") == 0) && i + 1 < argc) {
            timezone = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts("Usage: date [OPTION]");
            puts("Display the current date and time");
            puts("");
            puts("  -t, --timezone TZ   set timezone (UTC, GMT, GMT+1, GMT-5, Europe/Berlin, etc.)");
            puts("  --help              display this help and exit");
            return 0;
        }
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        fprintf(stderr, "date: cannot get current time\n");
        return 1;
    }

    if (timezone) {
        if (!get_timezone_offset(timezone, &tz_hours, &tz_minutes)) {
            fprintf(stderr, "date: invalid timezone '%s'\n", timezone);
            fprintf(stderr, "Valid timezones include: UTC, GMT, GMT+1, GMT-1, Europe/Berlin,\n");
            fprintf(stderr, "America/New_York, Asia/Tokyo, Australia/Sydney, and standard zone abbreviations.\n");
            fprintf(stderr, "Please note that all standard timezone abbreviations may not be available. As an alternative, use \"GMT+<number>\" or \"GMT-<number>\".\n");
            return 1;
        }

        offset_seconds = tz_hours * 3600 + tz_minutes * 60;
        adjusted_time = now + offset_seconds;
        
        tm_info = gmtime(&adjusted_time);
        if (!tm_info) {
            fprintf(stderr, "date: cannot convert time\n");
            return 1;
        }
        
        adjusted_tm = *tm_info;
        tm_info = &adjusted_tm;
    } else {
        tm_info = localtime(&now);
        if (!tm_info) {
            fprintf(stderr, "date: cannot convert time\n");
            return 1;
        }
    }

    if (strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Z %Y", tm_info) == 0) {
        fprintf(stderr, "date: buffer too small\n");
        return 1;
    }

    printf("%s\n", buffer);
    return 0;
}
