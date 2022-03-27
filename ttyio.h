#ifndef __TTYIO_H__
#define __TTYIO_H__

#include <stdarg.h>
#include <string.h>

va_list PRINTF(const char *format, ...);

#define doputchar(sendByte)    putch(sendByte)

#endif