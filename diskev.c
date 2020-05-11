
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#ifdef WITH_BLKID
#include <blkid/blkid.h>
#endif

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
	struct diskev *tmp;
	struct diskev *evt = NULL;

	ts = time(NULL);

	list_for_each_entry(tmp, &event_queue, list) {
		vdebug("Checking event %p, time %li/%li", tmp, tmp->ts, ts);
		evt = tmp;
		if (tmp->ts <= ts)
			break;
		evt = NULL;
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
	struct diskev *tmp;

	list_for_each_entry(tmp, &event_queue, list) {
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

	return 0;
}

int ev_validate(struct diskev *evt)
{
	if (!evt->action || !evt->device) {
		verror("Missing action or device");
		return 1;
	}

	if (!evt->serial && !evt->label &&
	    !evt->fsuuid && !evt->partuuid)
		warn("Unknown disk '%s' properties", evt->device);

	if (!evt->filesys)
		warn("Unknown disk '%s' FS type", evt->device);

	return 0;
}

void ev_sanitize(struct diskev *evt)
{
	const char *val;
#ifdef WITH_BLKID
	blkid_probe pr;
#endif

	if (!evt->device)
		return;

	if (evt->fsuuid && evt->partuuid && evt->filesys)
		return;

	if (!evt->fsuuid) {
		val = get_disk_uuid(evt->device);
		if (val) {
			evt->fsuuid = strdup(val);
			vdebug("Updated FS UUID %s, device %s", evt->fsuuid, evt->device);
		} else {
			vwarn("Failed to update by-FS UUID, device %s", evt->device);
		}
	}
	if (!evt->partuuid) {
		val = get_disk_partuuid(evt->device);
		if (val) {
			evt->partuuid = strdup(val);
			vdebug("Updated PART UUID %s, device %s", evt->partuuid, evt->device);
		} else {
			vwarn("Failed to update by-PART UUID, device %s", evt->device);
		}
	}

#ifdef WITH_BLKID
	if (evt->fsuuid && evt->partuuid && evt->filesys)
		return;

	pr = blkid_new_probe_from_filename(evt->device);
	if (!pr)
		return;

	blkid_do_probe(pr);

	if (!evt->fsuuid) {
		val = NULL;
		blkid_probe_lookup_value(pr, "UUID", &val, NULL);
		if (val) {
			evt->fsuuid = strdup(val);
			vdebug("Updated FS UUID %s, device %s", evt->fsuuid, evt->device);
		} else {
			vwarn("Failed to update FS UUID, device %s", evt->device);
		}
	}

	if (!evt->partuuid) {
		val = NULL;
		blkid_probe_lookup_value(pr, "PARTUUID", &val, NULL);
		if (val) {
			evt->partuuid = strdup(val);
			vdebug("Updated PART UUID %s, device %s", evt->partuuid, evt->device);
		} else {
			vwarn("Failed to update PART UUID, device %s", evt->device);
		}
	}

	if (!evt->filesys) {
		val = NULL;
		blkid_probe_lookup_value(pr, "TYPE", &val, NULL);
		if (val) {
			evt->filesys = strdup(val);
			vdebug("Updated FS type %s, device %s", evt->filesys, evt->device);
		} else {
			vwarn("Failed to update FS type, device %s", evt->device);
		}
	}

	blkid_free_probe(pr);
#endif
}
