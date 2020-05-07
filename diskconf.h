#ifndef _DISKCONF_H
#define _DISKCONF_H

#include "diskev.h"

int conf_load(void);
void conf_print(void);
int conf_find(struct diskev *evt, char **mpoint, char **mfs, char **mopts);
int conf_has_mount(char *point);

#endif // _DISKCONF_H
