#ifndef _NLSOCK_H
#define _NLSOCK_H

int nlsock_open(void);
void nlsock_close(int sock);
int nlsock_read(int sock, char *buf, size_t len);

#endif // _NLSOCK_H
