#include <stdio.h>
#include "parking.h"

void write_log_msg(const char *action) {
    char timebuf[64];
    char line[512];
    format_time(now_time(), timebuf, sizeof(timebuf));
    snprintf(line, sizeof(line), "[%s] %s", timebuf, action ? action : "");
    append_text_line(LOG_FILE, line);
}
