
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
#include <arpa/inet.h>
#ifdef WITH_LIBMOUNT
#include <libmount/libmount.h>
#endif

#include "util.h"
#include "diskconf.h"
#include "disktab.h"
#include "diskev.h"
#include "nlsock.h"
#include "evsock.h"

#define EV_SCHED_TIME 1

struct diskmnt_ctx {
	int verbosity;
	int daemonize;
	int kevent;
	int monitor;
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

static int perform_mount(const char *device, const char *point,
			  const char *type, unsigned long flags, const char *opts)
{
#if defined(WITH_LIBMOUNT)
	struct libmnt_context *cxt;

	mnt_init_debug(0);
	cxt = mnt_new_context();
	if (!cxt) {
		verror("Failed to allocate MNT context");
		return -1;
	}

	if (mnt_context_enable_verbose(cxt, 1)) {
		verror("Failed to set MNT verbose");
	}

	if (mnt_context_append_options(cxt, opts ? opts : "rw")) {
		verror("Failed to set MNT 'rw' options '%s'", opts);
		goto fail;
	}

	if (mnt_context_set_fstype(cxt, type)) {
		verror("Failed to set MNT type '%s'", type);
		goto fail;
	}

	if (mnt_context_set_source(cxt, device)) {
		verror("Failed to set MNT source '%s'", device);
		goto fail;
	}

	if (mnt_context_set_target(cxt, point)) {
		verror("Failed to set MNT target '%s'", point);
		goto fail;
	}

	if (mnt_context_set_mflags(cxt, flags)) {
		verror("Failed to set MNT flags '%lu'", flags);
		goto fail;
	}

	if (mnt_context_is_restricted(cxt)) {
		vwarn("Mounting is restricted");
	}

	if (mnt_context_mount(cxt)) {
		goto fail;
	}

	return 0;
fail:
	mnt_free_context(cxt);
	return 1;
#elif defined(WITH_SHMOUNT)
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "mount -t '%s' -o '%s' %s %s",
		 type, opts ? opts : "rw", device, point);
	return system(cmd);
#else
	return mount(device, point, fs, flags, data);
#endif
}

static int perform_umount(const char *device, const char *point)
{
#if defined(WITH_SHMOUNT)
	char cmd[32];
	snprintf(cmd, sizeof(cmd), "umount %s", point);
	return system(cmd);
#else
	return umount(point);
#endif
}

static void process_mount(struct diskev *evt)
{
	char *point, *fs, *opts;
	char *device = evt->device;
	char *action = evt->action;

	vdebug("Processing mount event: '%s'", device);

	if (!strcmp(action, "add")) {
		/* Try to fill up missing event
		 * properties; required delayed
		 * sanitize for add event. */
		if (ev_sanitize(evt)) {
			warn("Skip mount, cannot sanitize mount: '%s'", device);
			return;
		}

		if (ctx.monitor) {
			ev_dump(stdout, evt);
			return;
		}

		if (conf_find(evt, &point, &fs, &opts)) {
			debug("Skip mount, no confiured mount: '%s'", device);
			return;
		}

		if (!fs)
			fs = evt->filesys;
		if (!fs) {
			error("Skip mount, unknown file system: '%s'", device);
			return;
		}

		info("Mounting '%s' -> '%s' (%s, %s)", device, point, fs, opts);

		if (mkdir(point, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
			verror("Failed to create create dir '%s'", point);
#ifdef WITH_UGID
		if (ctx.uid && ctx.gid && chown(point, ctx.uid, ctx.gid))
			verror("Failed to chown created dir '%s'", point);
#endif

		if (perform_mount(device, point, fs, MS_NOSUID | MS_NOATIME, opts))
			error("Failed to mount '%s' to '%s', type '%s', opts '%s': %u (%s)",
			      device, point, fs, opts, errno, strerror(errno));
		else
			tab_add(device, point);
	} else if (!strcmp(action, "remove")) {
		if (ctx.monitor) {
			ev_dump(stdout, evt);
			return;
		}

		point = tab_find(device);
		if (!point) {
			debug("Skip unmount, not mounted '%s'", device);
			return;
		}

		info("Unmounting '%s' -> '%s'", device, point);

		if (perform_umount(device, point))
			error("Failed to unmount '%s' from '%s': %u (%s)",
			      device, point, errno, strerror(errno));
		else
			tab_del(device);
	} else {
		warn("Unknown event '%s' mounting '%s' -> '%s' (%s, %s)",
		     action, device, point, fs, opts);
	}
}

static void process_events(void)
{
	struct diskev *tmp;

	while ((tmp = ev_next())) {
		/* Find mount point and do mount */
		process_mount(tmp);

		ev_free(tmp);
		free(tmp);
	}
}

static void schedule_event(struct diskev *evt)
{
	struct diskev *tmp;

	tmp = ev_find(evt);
	if (!tmp) {
		debug("Scheduling new event");
		ev_insert(evt, EV_SCHED_TIME);
		return;
	}

	/* Implies add or remove action. */
	if (strcmp(evt->action, tmp->action)) {
		debug("Inverse event already in queue, removing");
		ev_remove(tmp);
		return;
	} else {
		debug("Similar event already in queue, ignoring");
		/* Shall we refresh event time stamp? */
	}

	ev_free(evt);
}

static void handle_kobj_event(int sock)
{
	struct diskev evt;
	size_t descr;
	size_t len;
	size_t size = getpagesize() * 2;
	char dbuf[size];
	char *buf = dbuf;

	memset(buf, 0, size);

	len = nlsock_recv(sock, buf, size);
	if (len < 0) {
		warn("Failed kobject uevent receive");
		return;
	} else if (len == 0) {
		info("Empty kobject uevent message");
		return;
	}

	if (!strchr(buf, '@')) {
		warn("Invalid kobject uevent format, size %zu", len);
		return;
	}

	vinfo("Received kobject uevent, size %zu/%zu", size, len);

	/* Skip desriptive line. */
	descr = strlen(buf) + 1;
	len -= descr;
	buf += descr;

	if (nlev_parse(&evt, buf, len)) {
		debug("Invalid kobject uevent message, size %zu", len);
		return;
	}

	schedule_event(&evt);
}

static void handle_udev_event(int sock)
{
	struct diskev evt;
	struct udev_monitor_netlink_header *umh;
	size_t descr;
	size_t len;
	size_t size = getpagesize() * 2;
	char dbuf[size];
	char *buf = dbuf;

	memset(buf, 0, size);

	len = nlsock_recv(sock, buf, size);
	if (len < 0) {
		warn("Failed udev uevent receive");
		return;
	} else if (len == 0) {
		info("Empty udev uevent message");
		return;
	}

	umh = (struct udev_monitor_netlink_header *)buf;
	if (len < sizeof(*umh)) {
		warn("Short udev uevent size: %zu", len);
		return;
	}

	if (strcmp(umh->prefix, "libudev") || (ntohl(umh->magic) != UDEV_MONITOR_MAGIC)) {
		warn("Invalid udev uevent format, size: %zu", len);
		return;
	}

	vinfo("Received udev uevent, size %zu/%zu", size, len);

	/* Skip event header. */
	descr = sizeof(*umh);
	len -= descr;
	buf += descr;

	if (nlev_parse(&evt, buf, len)) {
		debug("Invalid udev uevent message, size %zu", len);
		return;
	}

	schedule_event(&evt);
}

static void handle_local_event(int sock)
{
	struct diskev evt;
	struct evtlv *evh;
	int magic;
	size_t len;
	size_t size = getpagesize() * 2;
	char dbuf[size];
	char *buf = dbuf;

	len = sizeof(magic);
	if (evsock_read(sock, (char *)&magic, &len)) {
		error("Failed event magic receive");
		return;
	}

	if (magic != EVHEAD_MAGIC) {
		info("Invalid event magic");
		return;
	}

	len = size;
	if (evsock_read(sock, buf, &len)) {
		error("Failed event header receive");
		return;
	}

	evh = (struct evtlv *)buf;
	if (evh->type != EVTYPE_GROUP || evh->length != (len - sizeof(*evh))) {
		error("Invalid event header, type %i, size %u/%zu",
		      evh->type, evh->length, len);
		return;
	}

	vinfo("Read local event, size %u/%zu", evh->length, len);

	if (evev_parse(&evt, evh->value, evh->length)) {
		warn("Invalid event message, size %u", evh->length);
		return;
	}

	schedule_event(&evt);
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
		"  -m, --monitor       Event monitoring.\n"
		"  -k, --kevent        Force kernel uevent.\n"
		"  -v, --verbose       Increase verbosity.\n"
		"  -d, --debug         Debug mode.\n"
#ifdef WITH_UGID
		"  -u, --user <name>   User name.\n"
		"  -g, --group <name>  Group name.\n"
#endif
		"  -h, --help          Show this help.\n"
	       );
}

static struct option long_options[] =
{
	{ "help",	no_argument,       0, 'h' },
	{ "background",	no_argument,       0, 'b' },
	{ "monitor",	no_argument,       0, 'm' },
	{ "kevent",	no_argument,       0, 'k' },
	{ "verbose",	no_argument,       0, 'v' },
	{ "debug",	no_argument,       0, 'd' },
#ifdef WITH_UGID
	{ "user",	required_argument, 0, 'u' },
	{ "group",	required_argument, 0, 'g' },
#endif
	{ 0, 0, 0, 0 }
};

static void
parse_options(int argc, char *argv[])
{
	int opt, index;

	ctx.verbosity = 2;

	while ((opt = getopt_long(argc, argv, "bdg:hkmu:v", long_options, &index)) != -1) {
		switch(opt) {
		case 'b':
			ctx.daemonize = 1;
			break;
		case 'k':
			ctx.kevent = 1;
			break;
		case 'm':
			ctx.monitor = 1;
			break;
		case 'v':
			ctx.verbosity++;
			break;
		case 'd':
			ctx.debug = 1;
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
	int kern_feed;
	int evsock;
	int nlsock;

	parse_options(argc, argv);

	log_debug(ctx.debug);
	log_level(ctx.verbosity);

	/* Load mount config */
	conf_load();
	/* Load configured mounts */
	tab_load();

	if (ctx.monitor) {
		conf_dump(stdout);
		tab_dump(stdout);
	}

	if (!ctx.monitor && ctx.daemonize) {
		if (daemon(0, 0) == -1)
			die("daemon() failed\n");

		syslog_open();
	}

	signal(SIGTERM, sigterm);
	signal(SIGQUIT, sigterm);

	if (!ctx.kevent && !access("/run/udev/control", F_OK)) {
		debug("Subscribed to udev events");
		kern_feed = 0;
	} else {
		debug("Subscribed to kobject events");
		kern_feed = 1;
	}

	if (kern_feed)
		nlsock = nlsock_open(UEVENT_KERNEL);
	else
		nlsock = nlsock_open(UEVENT_UDEV);

	evsock = evsock_open();

	while (!quit) {
		struct timeval timeout = { 0, 500*1000 };
		int maxfd = -1, n;
		fd_set rfds;

		FD_ZERO(&rfds);

		FD_SET(nlsock, &rfds);
		maxfd = MAX(maxfd, nlsock);

		FD_SET(evsock, &rfds);
		maxfd = MAX(maxfd, evsock);

		n = select(maxfd + 1, &rfds, NULL, NULL, &timeout);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			die("select() failed");
			break;
		}

		if (FD_ISSET(nlsock, &rfds)) {
			if (kern_feed)
				handle_kobj_event(nlsock);
			else
				handle_udev_event(nlsock);
		}

		if (FD_ISSET(evsock, &rfds)) {
			handle_local_event(evsock);
		}

		process_events();
	}

	nlsock_close(nlsock);
	evsock_close(evsock);

	syslog_close();

	return 0;
}
