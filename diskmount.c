
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "diskev.h"
#include "evsock.h"

static void update(struct diskev *evt, char *line)
{
	char *pos;
	char *val;

	pos = strchr(line, '=');
	if (!pos)
		return;
	val = pos + 1;

	if (!strncmp(line, "ACTION", pos - line)) {
		evt->action = val;
	} else if (!strncmp(line, "SUBSYSTEM", pos - line)) {
		evt->subsys = val;
	} else if (!strncmp(line, "DEVTYPE", pos - line)) {
		evt->type = val;
	} else if (!strncmp(line, "DEVNAME", pos - line)) {
		evt->device = val;
	} else if (!strncmp(line, "ID_FS_TYPE", pos - line)) {
		evt->filesys = val;
	} else if (!strncmp(line, "ID_SERIAL_SHORT", pos - line)) {
		evt->serial = val;
	} else if (!strncmp(line, "ID_FS_LABEL", pos - line)) {
		evt->label = val;
	} else if (!strncmp(line, "ID_FS_UUID", pos - line)) {
		evt->fsuuid = val;
	} else if (!strncmp(line, "ID_PART_ENTRY_UUID", pos - line)) {
		evt->partuuid = val;
	} else {
		return;
	}

	vinfo("Added parameter line '%s'", line);

	return;
}

int main(int argc, char *argv[])
{
	char **env;
	int sock;
	size_t size = getpagesize() * 2;
	char dbuf[size];
	char *buf = dbuf;
	struct evtlv *ev;
	struct diskev evt;
	int magic = EVHEAD_MAGIC;

	if (argc > 1 && !strcmp(argv[1], "-d")) {
		log_debug(1);
		log_level(LL_DEBUG);
	}

	memset(&evt, 0, sizeof(evt));

	sock = evsock_connect();

	for (env = environ; *env; ++env)
		update(&evt, *env);

	if (!evt.subsys)
		evt.subsys = "block";
	if (!evt.type)
		evt.type = "partition";

	if (ev_check(&evt))
		die("Invalid event params");

	ev = (struct evtlv *)buf;
	ev->type = EVTYPE_GROUP;
	ev->length = evev_build(buf + sizeof(*ev), size - sizeof(*ev), &evt);
	if (!ev->length)
		die("Failed to build event");

	vinfo("Sending event, size %zu", ev->length + sizeof(*ev));

	if (evsock_write(sock, (char *)&magic, sizeof(magic)))
		die("Cannot write event magic");
	if (evsock_write(sock, buf, ev->length + sizeof(*ev)))
		die("Cannot write event data");

	evsock_disconnect(sock);

	return 0;
}
