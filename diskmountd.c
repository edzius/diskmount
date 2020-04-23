
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef WITH_UGID
#include <grp.h>
#include <pwd.h>
#endif

#include <sys/mount.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "util.h"
#include "diskconf.h"
#include "evsock.h"
#include "evenv.h"
#include "evqueue.h"

#define EV_SCHED_TIME 1

struct diskmnt_ctx {
	int verbosity;
	int daemonize;
	int dryrun;
	int check;
	int debug;
#ifdef WITH_UGID
	int uid;
	int gid;
#endif
};

struct diskmnt_ctx ctx;

static int quit;
static void sigterm(int signo)
{
	vinfo("Got signal %u", signo);
	quit = 1;
}

static void process_mount(struct evenv *env)
{
	char *point, *fs, *opts;
	char *action;
	char *device;

	if (conf_find(env, &point, &fs, &opts)) {
		debug("No mount point found");
		return;
	}

	device = env_lookup(env, "DEVNAME");
	if (!fs)
		fs = env_lookup(env, "ID_FS_TYPE");
	if (!fs) {
		error("Cannot mount, no file system");
		return;
	}

	action = env_lookup(env, "ACTION");
	if (!strcmp(action, "add")) {
		if (ctx.dryrun) {
			printf("Mounting '%s' -> '%s' (%s, %s)", device, point, fs, opts);
			printf("\n");
			return;
		}

		info("Mounting '%s' -> '%s' (%s, %s)", device, point, fs, opts);

		if (mkdir(point, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
			verror("Failed to create create dir '%s'", point);
#ifdef WITH_UGID
		if (ctx.uid && ctx.gid && chown(point, ctx.uid, ctx.gid))
			verror("Failed to chown created dir '%s'", point);
#endif

		if (mount(device, point, fs, MS_NOATIME, opts))
			error("Failed to mount '%s' to '%s', type '%s', opts '%s': %u (%s)",
			      device, point, fs, opts, errno, strerror(errno));
	} else if (!strcmp(action, "remove")) {
		if (ctx.dryrun) {
			printf("Unmounting '%s' -> '%s' (%s, %s)", device, point, fs, opts);
			printf("\n");
			return;
		}

		info("Unmounting '%s' -> '%s' (%s, %s)", device, point, fs, opts);

		if (umount(point))
			error("Failed to unmount '%s' from '%s': %u (%s)",
			      device, point, errno, strerror(errno));
	} else {
		warn("Unknown event '%s' mounting '%s' -> '%s' (%s, %s)",
		     action, device, point, fs, opts);
	}
}

static void process_events(void)
{
	struct evenv *env;

	while ((env = evq_pop())) {
		vdebug("Processing event queue entry");

		process_mount(env);

		env_free(env);
	}
}

static int validate_event(struct evenv *env)
{
	char *subsys, *devtype, *action, *device;

	action = env_lookup(env, "ACTION");
	subsys = env_lookup(env, "SUBSYSTEM");
	devtype = env_lookup(env, "DEVTYPE");
	device = env_lookup(env, "DEVNAME");
	if (!subsys || !devtype || !device || !action) {
		verror("Missing event params SUBSYSTEM/DEVTYPE");
		return 1;
	}

	if (strcmp(subsys, "block") ||
	    strcmp(devtype, "partition")) {
		verror("Incorrect event params, SUBSYSTEM %s, DEVTYPE %s", subsys, devtype);
		return 1;
	}

	return 0;
}

static int schedule_event(struct evenv *env)
{
	struct evenv *last;

	if (validate_event(env)) {
		warn("Invalid event params, skipping");
		return -1;
	}

	last = evq_peek(env);
	if (!last) {
		debug("Scheduling new event");
		evq_add(env, EV_SCHED_TIME);
		return 0;
	}

	/* Implies add or remove action. */
	if (strcmp(env_lookup(env, "ACTION"), env_lookup(last, "ACTION"))) {
		debug("Inverted event already in queue, removing");
		evq_del(last);
	} else {
		debug("Similar event already in queue, ignoring");
		/* Shall we refresh event time stamp? */
	}

	return 1;
}

static void handle_event(int sock)
{
	struct evenv *env;
	size_t size;
	size_t len;
	char *buf;

	len = sizeof(size);
	if (evsock_read(sock, (char *)&size, &len)) {
		error("Cannot read event size");
		return;
	}

	buf = malloc(size);
	if (!buf)
		die("malloc() failed");
	memset(buf, 0, size);

	len = size;
	if (evsock_read(sock, buf, &len)) {
		error("Cannot read event content");
		free(buf);
		return;
	}

	vinfo("Read event, size %u/%u", size, len);

	env = env_init(buf);
	if (!env) {
		error("Invalid event data, size %u", size);
		free(buf);
		return;
	}

	if (ctx.dryrun) {
		int i;

		printf("Received event:\n");
		printf("---------------\n");
		for (i = 0; i < env->count; i++) {
			printf("%s = %s\n", env->list[i].key, env->list[i].val);
		}
		printf("==============\n");
	}

	if (schedule_event(env))
		env_free(env);
}

#ifdef WITH_UGID
static int user2uid(const char *name)
{
	struct passwd *pwd = getpwnam(name);
	if (!pwd)
		die("User '%s' does not exist", name);
	return pwd->pw_uid;
}

static int group2gid(const char *name)
{
	struct group *grp = getgrnam(name);
	if (!grp)
		die("Group '%s' does not exist", name);
	return grp->gr_gid;
}
#endif

static void
print_usage(void)
{
	fprintf(stderr, "Usage: diskmountd <options>\n"
		"  -b, --background    Run as daemon.\n"
		"  -n, --dry-run       No real actions.\n"
		"  -d, --debug         Debug mode.\n"
		"  -v, --verbose       Increase verbosity.\n"
		"  -c, --check         Check config.\n"
#ifdef WITH_UGID
		"  -u, --user <name>   User name.\n"
		"  -g, --group <name>  Group name.\n"
#endif
		"  -h, --help          Show this help.\n"
	       );
}

static struct option long_options[] =
{
	{ "background",	no_argument,       0, 'b' },
	{ "check",	no_argument,       0, 'c' },
	{ "debug",	no_argument,       0, 'd' },
	{ "help",	no_argument,       0, 'h' },
	{ "dry-run",	no_argument,       0, 'n' },
	{ "verbose",	no_argument,       0, 'v' },
#ifdef WITH_UGID
	{ "user",	no_argument,       0, 'u' },
	{ "group",	no_argument,       0, 'g' },
#endif
	{ 0, 0, 0, 0 }
};

static void
parse_options(int argc, char *argv[])
{
	int opt, index;

	ctx.verbosity = 2;

	while ((opt = getopt_long(argc, argv, "bcdg:hnu:v", long_options, &index)) != -1) {
		switch(opt) {
		case 'b':
			ctx.daemonize = 1;
			break;
		case 'c':
			ctx.check = 1;
			break;
		case 'd':
			ctx.debug = 1;
			break;
		case 'n':
			ctx.dryrun = 1;
			break;
		case 'v':
			ctx.verbosity++;
			break;
#ifdef WITH_UGID
		case 'u':
			ctx.uid = user2uid(optarg);
			break;
		case 'g':
			ctx.gid = group2gid(optarg);
			break;
#endif
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
		default:
			print_usage();
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char *argv[])
{
	int sock;

	parse_options(argc, argv);

	log_debug(ctx.debug);
	log_level(ctx.verbosity);

	conf_load();

	if (ctx.check) {
		conf_print();
		return 0;
	}

	if (ctx.daemonize && daemon(0, 0) == -1)
		die("daemon() failed\n");

	if (ctx.daemonize)
		syslog_open();

	signal(SIGTERM, sigterm);
	signal(SIGSEGV, sigterm);
	signal(SIGQUIT, sigterm);

	sock = evsock_open();

	while (!quit) {
		struct timeval timeout = { 0, 500*1000 };
		int maxfd = -1, n;
		fd_set rfds;

		FD_ZERO(&rfds);

		FD_SET(sock, &rfds);
		maxfd = MAX(maxfd, sock);

		n = select(maxfd + 1, &rfds, NULL, NULL, &timeout);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			die("select() failed");
			break;
		}

		//if (n == 0) {
		//	continue;
		//}

		if (FD_ISSET(sock, &rfds)) {
			handle_event(sock);
		}

		process_events();
	}

	evsock_close(sock);

	syslog_close();

	return 0;
}
