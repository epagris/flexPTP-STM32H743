/* (C) András Wiesner, 2020 */

#ifndef SRC_UTILS_H_
#define SRC_UTILS_H_

#include <stdbool.h>
#include <string.h>
#include "stm32h7xx_hal.h"
#include "embfmt/embformat.h"

/*#ifndef htonl
    #define htonl(a)                    \
        ((((a) >> 24) & 0x000000ff) |   \
         (((a) >>  8) & 0x0000ff00) |   \
         (((a) <<  8) & 0x00ff0000) |   \
         (((a) << 24) & 0xff000000))
#endif

#ifndef ntohl
    #define ntohl(a)    htonl((a))
#endif

#ifndef htons
    #define htons(a)                \
        ((((a) >> 8) & 0x00ff) |    \
         (((a) << 8) & 0xff00))
#endif

#ifndef ntohs
    #define ntohs(a)    htons((a))
#endif*/

void MSG(const char *pcString, ...);

#define SPRINTF(str,n,fmt, ...) embfmt(str,n,fmt,__VA_ARGS__)
#define SNPRINTF(str,n,fmt, ...) embfmt(str,n,fmt,__VA_ARGS__)

#define CLILOG(en, ...) { if (en) MSG(__VA_ARGS__); }

#define MAX(x,y) ((x > y) ? (x) : (y))
#define MIN(x,y) ((x < y) ? (x) : (y))
#define LIMIT(x,l) (x < -l ? -l : (x > l ? l : x))

#define ONOFF(str) ((!strcmp(str, "on")) ? 1 : ((!strcmp(str, "off")) ? 0 : -1))
 
#endif /* SRC_UTILS_H_ */
