
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "util.h"
#include "diskconf.h"
#include "diskev.h"

struct diskdef {
	char *device;
	char *serial;
	char *fs_label;
	char *fs_uuid;
	char *part_uuid;
	char *mount_point;
	char *mount_fs;
	char *mount_opts;
	struct list_head list;
};

LLIST_HEAD(mount_conf);

static int conf_update_device(struct diskdef *def, char *src)
{
	char *key;
	char *val;

	if (!src) {
		verror("Missing device declaration");
		return 1;
	}

	key = strdup(src);

	if (*src == '/') {
		def->device = key;
		return 0;
	}

	val = strchr(key, '=');
	if (!val) {
		vwarn("Invalid device declaration: '%s'", key);
		free(key);
		return 1;
	}

	if (!strlen(val+1)) {
		vwarn("Invalid device resource: '%s'", key);
		free(key);
		return 1;
	}

	*val = '\0';
	val++;

	if (!strcmp(key, "DEV"))
		def->device = val;
	else if (!strcmp(key, "SERIAL"))
		def->serial = val;
	else if (!strcmp(key, "LABEL"))
		def->fs_label = val;
	else if (!strcmp(key, "UUID"))
		def->fs_uuid = val;
	else if (!strcmp(key, "PARTUUID"))
		def->part_uuid = val;
	else {
		vwarn("Invalid device definition: '%s'", key);
		return 1;
	}

	return 0;
}

static int conf_update_mount(struct diskdef *def, char *src)
{
	if (!src) {
		verror("Missing device mount point");
		return 1;
	}

	if (src[0] != '/') {
		vwarn("Incorrect mount point: '%s'", src);
		return 1;
	}

	def->mount_point = strdup(src);
	return 0;
}

static void conf_add_entry(struct mntent *ent)
{
	struct diskdef *def;

	def = calloc(1, sizeof(*def));
	if (!def)
		die("malloc() failed");

	if (conf_update_device(def, ent->mnt_fsname))
		goto fail;

	if (conf_update_mount(def, ent->mnt_dir))
		goto fail;

	if (!strlen(ent->mnt_type))
		goto done;
	def->mount_fs = strdup(ent->mnt_type);

	if (!strlen(ent->mnt_opts))
		goto done;
	def->mount_opts = strdup(ent->mnt_opts);

done:
	list_add_tail(&def->list, &mount_conf);
	vinfo("Stored mount: '%s' -> '%s'",
	      ent->mnt_fsname, ent->mnt_dir);
	return;

fail:
	warn("Skipped invalid mount: '%s' -> '%s'",
	     ent->mnt_fsname, ent->mnt_dir);
	free(def);
}

static int conf_load_file(const char *file_name)
{
	FILE *fp;
	struct mntent *ent;

	fp = setmntent(file_name, "r");
	if (!fp) {
		debug("Cannot read: '%s'", file_name);
		return -1;
	}

	vinfo("Loading config: '%s'", file_name);

	while (NULL != (ent = getmntent(fp))) {
		conf_add_entry(ent);
	}
	endmntent(fp);

	conf_print();

	return 0;
}

int conf_load(void)
{
	if (!conf_load_file("/etc/disktab"))
		return 0;

	if (!conf_load_file("diskmount.conf"))
		return 0;

	die("No disk config found");
	return -1;
}

int conf_find(struct diskev *evt, char **mpoint, char **mfs, char **mopts)
{
	struct diskdef *def, *n;

	list_for_each_entry_safe(def, n, &mount_conf, list) {
		if (def->device) {
			if (evt->device && !strcmp(def->device, evt->device))
				break;
		} else if (def->serial) {
			if (evt->serial && !strcmp(def->serial, evt->serial))
				break;
		} else if (def->fs_label) {
			if (evt->label && !strcmp(def->fs_label, evt->label))
				break;
		} else if (def->fs_uuid) {
			if (evt->fsuuid && !strcmp(def->fs_uuid, evt->fsuuid))
				break;
		} else if (def->part_uuid) {
			if (evt->partuuid && !strcmp(def->part_uuid, evt->partuuid))
				break;
		}
		def = NULL;
	}

	*mpoint = '\0';
	*mfs = '\0';
	*mopts = '\0';

	if (!def) {
		vwarn("No mount config for device: '%s'",
		      evt->device ? evt->device : "N/A");
		return 1;
	}

	if (def->mount_point)
		*mpoint = def->mount_point;
	if (def->mount_fs)
		*mfs = def->mount_fs;
	if (def->mount_opts)
		*mopts = def->mount_opts;

	return 0;
}

void conf_print(void)
{
	struct diskdef *def, *n;
	char buffer[128];
	int total = sizeof(buffer);
	int size = 0;

	debug("Loaded config:");

	list_for_each_entry_safe(def, n, &mount_conf, list) {
		if (def->device) {
			size += snprintf(buffer + size, total - size, "DEV = %s\t\t", def->device);
		} else if (def->serial) {
			size += snprintf(buffer + size, total - size, "SERIAL = %s\t\t", def->serial);
		} else if (def->fs_label) {
			size += snprintf(buffer + size, total - size, "LABEL = %s\t\t", def->fs_label);
		} else if (def->fs_uuid) {
			size += snprintf(buffer + size, total - size, "UUID = %s\t\t", def->fs_uuid);
		} else if (def->part_uuid) {
			size += snprintf(buffer + size, total - size, "PARTUUID = %s\t\t", def->part_uuid);
		}

		if (def->mount_point)
			size += snprintf(buffer + size, total - size, "%s\t\t", def->mount_point);
		if (def->mount_fs)
			size += snprintf(buffer + size, total - size, "%s\t\t", def->mount_fs);
		if (def->mount_opts)
			size += snprintf(buffer + size, total - size, "%s\t\t", def->mount_opts);

		debug("%s", buffer);
	}
}
