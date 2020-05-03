#ifndef _DISKEV_H
#define _DISKEV_H

#include <time.h>
#include "list.h"

struct diskev {
	char *subsys;
	char *type;
	char *action;
	char *device;
	char *filesys;
	char *serial;
	char *label;
	char *fsuuid;
	char *partuuid;
	time_t ts;
	struct list_head list;
};

void ev_insert(struct diskev *evt, off_t delay);
void ev_remove(struct diskev *evt);
struct diskev *ev_next(void);
struct diskev *ev_find(struct diskev *evt);
void ev_free(struct diskev *evt);
int ev_check(struct diskev *evt);

#endif // _DISKEV_H
