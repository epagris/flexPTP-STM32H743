/* (C) Wiesner András, 2022 */

#ifndef EMBFORMAT_EMBFORMAT_H
#define EMBFORMAT_EMBFORMAT_H

#include <stdarg.h>

unsigned long int vembfmt(char *str, unsigned long int len, char *format, va_list args);
unsigned long int embfmt(char *str, unsigned long int len, char *format, ...);

#endif //EMBFORMAT_EMBFORMAT_H
