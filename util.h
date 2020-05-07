#ifndef _UTIL_H
#define _UTIL_H

#include <stdio.h>

#define __noreturn __attribute__((noreturn))
#define __print_format(x, y) __attribute__((format(printf, x, y)))

#ifndef MAX
#define MAX(a, b) (a) > (b) ? (a) : (b)
#endif

#define LL_DEBUG 7
#define LL_INFO 5
#define LL_WARN 3
#define LL_ERROR 2

#define debug(fmt, ...) log_print(LL_DEBUG, fmt, ## __VA_ARGS__)
#define info(fmt, ...) log_print(LL_INFO, fmt, ## __VA_ARGS__)
#define warn(fmt, ...) log_print(LL_WARN, fmt, ## __VA_ARGS__)
#define error(fmt, ...) log_print(LL_ERROR, fmt, ## __VA_ARGS__)

#define vdebug(fmt, ...) log_verbose(LL_DEBUG, fmt, ## __VA_ARGS__)
#define vinfo(fmt, ...) log_verbose(LL_INFO, fmt, ## __VA_ARGS__)
#define vwarn(fmt, ...) log_verbose(LL_WARN, fmt, ## __VA_ARGS__)
#define verror(fmt, ...) log_verbose(LL_ERROR, fmt, ## __VA_ARGS__)

void set_coe(int fd);
void set_nio(int fd);

char *strfdup(const char *format, ... ) __print_format(1, 2);
void __noreturn die(const char *msg, ...) __print_format(1, 2);

void log_print(int lvl, char *fmt, ...) __print_format(2, 3);
void log_level(int lvl);
void log_verbose(int lvl, char *fmt, ...) __print_format(2, 3);
void log_debug(int st);
int is_debug(void);
void syslog_open(void);
void syslog_close(void);
char *get_disk_uuid(const char *disk);
char *get_disk_partuuid(const char *disk);

#endif // _UTIL_H
