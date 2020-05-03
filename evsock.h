#ifndef _EVSOCK_H
#define _EVSOCK_H

#include "diskev.h"

#ifndef EVHEAD_MAGIC
#define EVHEAD_MAGIC 0
#endif

#define EVTYPE_GROUP 1
#define EVTYPE_DONE 2
#define EVTYPE_ACTION 3
#define EVTYPE_DEVICE 4
#define EVTYPE_FS 5
#define EVTYPE_LABEL 6
#define EVTYPE_SERIAL 7
#define EVTYPE_FSUUID 8
#define EVTYPE_PARTUUID 9

struct evtlv {
	short type;
	short length;
	char value[];
};

int evsock_open(void);
void evsock_close(int sock);
int evsock_connect(void);
void evsock_disconnect(int sock);
int evsock_read(int sock, char *buf, size_t *len);
int evsock_write(int sock, char *buf, size_t len);
int evev_parse(struct diskev *evt, char *data, int size);
int evev_build(char *data, int size, struct diskev *evt);

#endif // _EVSOCK_H
