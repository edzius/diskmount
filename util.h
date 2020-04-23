#ifndef _UTIL_H
#define _UTIL_H

#include <stdio.h>

#define __noreturn __attribute__((noreturn))
#define __print_format(x, y) __attribute__((format(printf, x, y)))

#ifndef MAX
#define MAX(a, b) (a) > (b) ? (a) : (b)
#endif

#define LOG_DEBUG 7
#define LOG_INFO 5
#define LOG_WARN 3
#define LOG_ERROR 2

#define debug(fmt, ...) log_print(LOG_DEBUG, fmt, ## __VA_ARGS__)
#define info(fmt, ...) log_print(LOG_INFO, fmt, ## __VA_ARGS__)
#define warn(fmt, ...) log_print(LOG_WARN, fmt, ## __VA_ARGS__)
#define error(fmt, ...) log_print(LOG_ERROR, fmt, ## __VA_ARGS__)

#define vdebug(fmt, ...) log_verbose(LOG_DEBUG, fmt, ## __VA_ARGS__)
#define vinfo(fmt, ...) log_verbose(LOG_INFO, fmt, ## __VA_ARGS__)
#define vwarn(fmt, ...) log_verbose(LOG_WARN, fmt, ## __VA_ARGS__)
#define verror(fmt, ...) log_verbose(LOG_ERROR, fmt, ## __VA_ARGS__)

void __noreturn die(const char *msg, ...) __print_format(1, 2);
void log_print(int lvl, char *fmt, ...) __print_format(2, 3);
void log_level(int lvl);
void log_verbose(int lvl, char *fmt, ...) __print_format(2, 3);
void log_debug(int st);
int is_debug(void);

#endif // _UTIL_H
