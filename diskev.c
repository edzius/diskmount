
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "list.h"
#include "util.h"
#include "diskev.h"

LLIST_HEAD(event_queue);

void ev_insert(struct diskev *evt, off_t delay)
{
	struct diskev *tmp;

	tmp = malloc(sizeof(*tmp));
	if (!tmp)
		die("malloc() failed");

	memcpy(tmp, evt, sizeof(*tmp));
	tmp->ts = time(NULL) + delay;
	list_add_tail(&tmp->list, &event_queue);
	vinfo("Scheduled event: %p, time %li", tmp, tmp->ts);
}

void ev_remove(struct diskev *evt)
{
	vinfo("Canceled event: %p", evt);
	list_del(&evt->list);
	ev_free(evt);
	free(evt);
}

struct diskev *ev_next(void)
{
	time_t ts;
	struct diskev *tmp, *n;
	struct diskev *evt = NULL;

	ts = time(NULL);

	list_for_each_entry_safe(tmp, n, &event_queue, list) {
		vdebug("Checking event %p, time %li/%li", tmp, tmp->ts, ts);
		if (tmp->ts <= ts) {
			evt = tmp;
			break;
		}
	}

	if (!evt) {
		vdebug("No scheduled events, time %li", ts);
		return NULL;
	}

	vinfo("Popping event: %p, time %li", evt, ts);
	list_del(&evt->list);
	return evt;
}

struct diskev *ev_find(struct diskev *evt)
{
	struct diskev *tmp, *n;

	list_for_each_entry_safe(tmp, n, &event_queue, list) {
		if (evt->partuuid && tmp->partuuid) {
			if (!strcmp(evt->partuuid, tmp->partuuid))
				return tmp;
		} else if (evt->fsuuid && tmp->fsuuid) {
			if (!strcmp(evt->fsuuid, tmp->fsuuid))
				return tmp;
		} else if (evt->serial && tmp->serial) {
			if (!strcmp(evt->serial, tmp->serial))
				return tmp;
		} else if (evt->label && tmp->label) {
			if (!strcmp(evt->label, tmp->label))
				return tmp;
		}
	}

	return NULL;
}

void ev_free(struct diskev *evt)
{
	if (evt->action)
		free(evt->action);
	if (evt->subsys)
		free(evt->subsys);
	if (evt->type)
		free(evt->type);
	if (evt->device)
		free(evt->device);
	if (evt->filesys)
		free(evt->filesys);
	if (evt->serial)
		free(evt->serial);
	if (evt->label)
		free(evt->label);
	if (evt->fsuuid)
		free(evt->fsuuid);
	if (evt->partuuid)
		free(evt->partuuid);
}

int ev_check(struct diskev *evt)
{
	if (!evt->subsys || strcmp(evt->subsys, "block"))
		return 1;
	if (!evt->type || strcmp(evt->type, "partition"))
		return 1;
	if (!evt->action || !evt->device) {
		verror("Missing action or device");
		return 1;
	}

	if (!evt->serial && !evt->label &&
	    !evt->fsuuid && !evt->partuuid) {
		verror("Missing serial or label or fsuuid or partuuid");
		return 1;
	}

	if (!evt->filesys)
		warn("Unknown FS type of '%s'", evt->device);

	return 0;
}
