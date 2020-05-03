
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	struct diskdef *next;
};

struct diskdef *conf;

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

static int conf_update_fs(struct diskdef *def, char *src)
{
	if (!src) {
		vinfo("Missing mount file system");
		return 1;
	}

	def->mount_fs = strdup(src);
	return 0;
}

static int conf_update_opt(struct diskdef *def, char *src)
{
	if (!src) {
		vinfo("Missing mount options");
		return 1;
	}

	def->mount_opts = strdup(src);
	return 0;
}

static int conf_check_comment(char *line)
{
	while (*line == ' ') {
		line++;
	}
	return *line == '#';
}

static void conf_process_line(char *line)
{
	const char sep[] = "\t ";
	char *bak;
	char *token;
	struct diskdef *def;
	struct diskdef *old;

	if (conf_check_comment(line))
		return;

	bak = strdup(line);
	def = calloc(1, sizeof(*def));

	token = strtok(line, sep);
	if (conf_update_device(def, token))
		goto fail;

	token = strtok(NULL, sep);
	if (conf_update_mount(def, token))
		goto fail;

	token = strtok(NULL, sep);
	if (!token)
		goto done;
	conf_update_fs(def, token);

	token = strtok(NULL, sep);
	if (!token)
		goto done;
	conf_update_opt(def, token);

done:
	old = conf;
	conf = def;
	def->next = old;
	vinfo("Included config line: '%s'", bak);
	free(bak);
	return;

fail:
	warn("Skipped invalid config line: '%s'", bak);
	free(bak);
	free(def);
}

static int conf_parse_chunk(char *data, int len)
{
	char *pos;

	vdebug("Processing data chunk, size: '%u'", len);

	while (1) {
		pos = strchr(data, '\n');
		if (!pos)
			break;
		*pos = '\0';

		conf_process_line(data);
		data = pos + 1;
	}
	return strlen(data);
}

static int conf_load_file(const char *file_name)
{
	char buf[1024];
	int cnt, off;
	int err;
	FILE *fp;

	fp = fopen(file_name, "r");
	if (!fp) {
		debug("Cannot to open: '%s'", file_name);
		return -1;
	}

	vinfo("Loading config: '%s'", file_name);

	while (!feof(fp) && !ferror(fp)) {
		memset(buf, 0, sizeof(buf));
		cnt = fread(buf, sizeof(char), sizeof(buf), fp);
		off = conf_parse_chunk(buf, cnt);
		if (off != 0 && off < cnt) {
			vdebug("Seek back, offset: '%i'", off - cnt);
			fseek(fp, off - cnt, SEEK_CUR);
		}
	}
	err = ferror(fp);
	fclose(fp);

	conf_print();

	return err;
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
	struct diskdef *def = conf;

	while (def) {
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

		def = def->next;
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
	struct diskdef *def = conf;
	char buffer[128];
	int total = sizeof(buffer);
	int size = 0;

	debug("Loaded config:");

	while (def) {
		if (def->device) {
			size += snprintf(buffer, total - size, "DEV = %s\t\t", def->device);
		} else if (def->serial) {
			size += snprintf(buffer, total - size, "SERIAL = %s\t\t", def->serial);
		} else if (def->fs_label) {
			size += snprintf(buffer, total - size, "LABEL = %s\t\t", def->fs_label);
		} else if (def->fs_uuid) {
			size += snprintf(buffer, total - size, "UUID = %s\t\t", def->fs_uuid);
		} else if (def->part_uuid) {
			size += snprintf(buffer, total - size, "PARTUUID = %s\t\t", def->part_uuid);
		}

		if (def->mount_point)
			size += snprintf(buffer, total - size, "%s\t\t", def->mount_point);
		if (def->mount_fs)
			size += snprintf(buffer, total - size, "%s\t\t", def->mount_fs);
		if (def->mount_opts)
			size += snprintf(buffer, total - size, "%s\t\t", def->mount_opts);

		debug("%s", buffer);

		def = def->next;
	}
}
