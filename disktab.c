
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "util.h"
#include "diskconf.h"

struct diskent {
	char *mount_device;
	char *mount_point;
	struct list_head list;
};

LLIST_HEAD(mount_tab);

void tab_del(const char *devfile)
{
	struct diskent *tmp;
	struct diskent *ent = NULL;

	list_for_each_entry(tmp, &mount_tab, list) {
		ent = tmp;
		if (!strcmp(tmp->mount_device, devfile))
			break;
		ent = NULL;
	}

	if (!ent)
		return;

	list_del(&ent->list);
	free(ent->mount_device);
	free(ent->mount_point);
	free(ent);
}

void tab_add(const char *devfile, const char *mntfile)
{
	struct diskent *def;

	if (!strlen(devfile) || !strlen(mntfile)) {
		vwarn("Skipped invalid mount entry: '%s' -> '%s'", devfile, mntfile);
		return;
	}

	if (devfile[0] != '/') {
		vinfo("Skipped incorrect mount entry: '%s' -> '%s'", devfile, mntfile);
		return;
	}

	def = calloc(1, sizeof(*def));
	if (!def)
		die("malloc() failed");

	def->mount_device = strdup(devfile);
	def->mount_point = strdup(mntfile);
	list_add_tail(&def->list, &mount_tab);
	vinfo("Added mount entry: '%s' -> '%s'", devfile, mntfile);
}

void tab_load(void)
{
	FILE *fp;
	struct mntent *ent;

	fp = setmntent("/etc/mtab", "r");
	if (!fp)
		fp = setmntent("/proc/mounts", "r");
	if (!fp) {
		warn("Cannot load mounts");
		return;
	}

	vinfo("Loading mounts");

	while (NULL != (ent = getmntent(fp))) {
		if (!conf_has_mount(ent->mnt_dir)) {
			vinfo("Skipped non-configed mount entry: '%s' -> '%s'",
			      ent->mnt_fsname, ent->mnt_dir);
			continue;
		}

		tab_add(ent->mnt_fsname, ent->mnt_dir);
	}
	endmntent(fp);
}

char *tab_find(const char *devfile)
{
	struct diskent *ent;

	list_for_each_entry(ent, &mount_tab, list) {
		if (!strcmp(ent->mount_device, devfile))
			return ent->mount_point;
	}

	return NULL;
}
