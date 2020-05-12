#ifndef _DISKTAB_H
#define _DISKTAB_H

void tab_del(const char *devfile);
void tab_add(const char *devfile, const char *mntfile);
void tab_load(void);
char *tab_find(const char *devpath);
void tab_dump(FILE *fp);

#endif // _DISKTAB_H
