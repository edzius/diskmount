
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
	if (strcmp(ent->mnt_type, "-"))
		def->mount_fs = strdup(ent->mnt_type);

	if (!strlen(ent->mnt_opts))
		goto done;
	if (strcmp(ent->mnt_opts, "-"))
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

int conf_has_mount(char *point)
{
	struct diskdef *def;

	list_for_each_entry(def, &mount_conf, list) {
		if (!strcmp(def->mount_point, point))
			return 1;
	}

	return 0;
}

int conf_find(struct diskev *evt, char **mpoint, char **mfs, char **mopts)
{
	struct diskdef *tmp;
	struct diskdef *def = NULL;

	list_for_each_entry(tmp, &mount_conf, list) {
		def = tmp;
		if (tmp->device) {
			if (evt->device && !strcmp(tmp->device, evt->device))
				break;
		} else if (tmp->serial) {
			if (evt->serial && !strcmp(tmp->serial, evt->serial))
				break;
		} else if (tmp->fs_label) {
			if (evt->label && !strcmp(tmp->fs_label, evt->label))
				break;
		} else if (tmp->fs_uuid) {
			if (evt->fsuuid && !strcmp(tmp->fs_uuid, evt->fsuuid))
				break;
		} else if (tmp->part_uuid) {
			if (evt->partuuid && !strcmp(tmp->part_uuid, evt->partuuid))
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

void conf_dump(FILE *fp)
{
	struct diskdef *def;

	fprintf(fp, "Mount config:\n");
	list_for_each_entry(def, &mount_conf, list) {
		if (def->device) {
			fprintf(fp, "DEV=%-28s", def->device);
		} else if (def->serial) {
			fprintf(fp, "SERIAL=%-25s", def->serial);
		} else if (def->fs_label) {
			fprintf(fp, "LABEL=%-26s", def->fs_label);
		} else if (def->fs_uuid) {
			fprintf(fp, "UUID=%-27s", def->fs_uuid);
		} else if (def->part_uuid) {
			fprintf(fp, "PARTUUID=%-23s", def->part_uuid);
		}

		if (def->mount_point)
			fprintf(fp, "%-16s", def->mount_point);
		if (def->mount_fs)
			fprintf(fp, "%-8s", def->mount_fs);
		if (def->mount_opts)
			fprintf(fp, "%s", def->mount_opts);

		fprintf(fp, "\n");
	}
}
