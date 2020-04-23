
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

static int level = LOG_INFO;
static int use_verbose;

void __noreturn die(const char *format, ... )
{
	int saved = errno;

	if (format != NULL) {
		va_list ap;

		va_start(ap, format);
		vfprintf(stderr, format, ap);
		va_end(ap);
	}

	if (saved != 0)
		fprintf(stderr, ": %s (%d)\n", strerror(saved), saved);
	else
		fprintf(stderr, "\n");

	exit(1);
}

void log_write(int lvl, const char *fmt, va_list ap)
{
	switch (lvl) {
	case LOG_DEBUG:
		vprintf(fmt, ap);
		break;
	case LOG_INFO:
		vprintf(fmt, ap);
		break;
	case LOG_WARN:
		vprintf(fmt, ap);
		break;
	case LOG_ERROR:
		vprintf(fmt, ap);
		break;
	}
	printf("\n");
}

void log_print(int lvl, char *fmt, ...)
{
	va_list ap;

	if (level < lvl)
		return;

	va_start(ap, fmt);
	log_write(lvl, fmt, ap);
	va_end(ap);
}

void log_verbose(int lvl, char *fmt, ...)
{
	va_list ap;

	if (!use_verbose)
		return;

	if (level < lvl)
		return;

	va_start(ap, fmt);
	log_write(lvl, fmt, ap);
	va_end(ap);
}

void log_level(int lvl)
{
	level = lvl;
}

void log_debug(int st)
{
	use_verbose = st;
}

int is_debug(void)
{
	return use_verbose;
}
