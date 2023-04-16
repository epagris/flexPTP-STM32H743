#include "utils.h"

#include "embfmt/embformat.h"

#include <stdio.h>

//char gMSGBuf[64];



#define MAX_LINE_LENGTH (383)

static char linebuf[MAX_LINE_LENGTH + 1];

void MSG(const char *pcString, ...) {
    va_list vaArgP;
    va_start(vaArgP, pcString);
    vembfmt(linebuf, MAX_LINE_LENGTH, pcString, vaArgP);
    va_end(vaArgP);
    printf(linebuf);
}
