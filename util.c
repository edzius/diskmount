
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/types.h>

#include "util.h"

static int level = LL_INFO;
static int use_verbose;
static int use_syslog;

void set_coe(int fd)
{
        int  result;
        long oflags;

	vdebug("Setting COE on socket %u", fd);

        oflags = fcntl(fd, F_GETFD);
        result = fcntl(fd, F_SETFD, oflags | FD_CLOEXEC);
        if (result < 0)
                die("cannot set FD_CLOEXEC flag.\n");
}

void set_nio(int fd)
{
        int  result;
        long oflags;

	vdebug("Setting NIO on socket %u", fd);

        oflags = fcntl(fd, F_GETFL);
        result = fcntl(fd, F_SETFL, oflags | O_NONBLOCK);
        if (result < 0)
                die("cannot set O_NONBLOCK flags.\n");
}

char *strfdup(const char *format, ... )
{
	char buf[1024];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	return strdup(buf);
}

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
	case LL_DEBUG:
		if (use_syslog)
			vsyslog(LOG_DEBUG, fmt, ap);
		else
			vprintf(fmt, ap);
		break;
	case LL_INFO:
		if (use_syslog)
			vsyslog(LOG_INFO, fmt, ap);
		else
			vprintf(fmt, ap);
		break;
	case LL_WARN:
		if (use_syslog)
			vsyslog(LOG_WARNING, fmt, ap);
		else
			vprintf(fmt, ap);
		break;
	case LL_ERROR:
		if (use_syslog)
			vsyslog(LOG_ERR, fmt, ap);
		else
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

void syslog_open(void)
{
	openlog(NULL, LOG_PID, LOG_DAEMON);
	use_syslog = 1;
}

void syslog_close(void)
{
	closelog();
}

static char *get_disk_prop(const char *type, const char *disk)
{
	struct dirent *dp;
	DIR *dfd;
	int len;
	char link[128];
	char path[128];
	char real[128];
	char *file = path;
	char *res = NULL;
	char buf[64];

	file += sprintf(path, "/dev/disk/%s", type);

	dfd = opendir(path);
	if (dfd == NULL) {
		vwarn("Cannot open directory: '%s'", path);
		return 0;
	}

	while ((dp = readdir(dfd)) != NULL) {
		if (dp->d_name[0] == '.')
			continue;

		sprintf(file, "/%s", dp->d_name);
		len = readlink(path, link, sizeof(link));
		if (len == -1)
			continue;
		if (len == sizeof(link))
			continue;
		link[len] = 0;

		sprintf(file, "/%s", link);
		if (!realpath(path, real))
			continue;

		if (strcmp(real, disk))
			continue;

		strcpy(buf, dp->d_name);
		res = buf;
		break;
	}

	closedir(dfd);
	return res;
}

char *get_disk_uuid(const char *disk)
{
	return get_disk_prop("by-uuid", disk);
}

char *get_disk_partuuid(const char *disk)
{
	return get_disk_prop("by-partuuid", disk);
}
