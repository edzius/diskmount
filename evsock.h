#ifndef _EVSOCK_H
#define _EVSOCK_H

int evsock_open(void);
void evsock_close(int sock);
int evsock_connect(void);
int evsock_disconnect(int sock);
int evsock_read(int sock, char *buf, size_t *len);
int evsock_write(int sock, char *buf, size_t len);

#endif // _EVSOCK_H
