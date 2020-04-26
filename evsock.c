
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "util.h"

#ifndef RUN_PATH
#define RUN_PATH "/var/run/"
#endif

#define SOCKET_FILE RUN_PATH"diskmount.sock"

static void set_coe(int fd)
{
        int  result;
        long oflags;

	vdebug("Setting COE on socket %u", fd);

        oflags = fcntl(fd, F_GETFD);
        result = fcntl(fd, F_SETFD, oflags | FD_CLOEXEC);
        if (result < 0)
                die("cannot set FD_CLOEXEC flag.\n");
}

static void set_nio(int fd)
{
        int  result;
        long oflags;

	vdebug("Setting NIO on socket %u", fd);

        oflags = fcntl(fd, F_GETFL);
        result = fcntl(fd, F_SETFL, oflags | O_NONBLOCK);
        if (result < 0)
                die("cannot set O_NONBLOCK flags.\n");
}

int evsock_open(void)
{
	int sock;
	socklen_t srv_len;
	struct sockaddr_un srv_addr;

	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sun_family = AF_UNIX;
	strncpy(srv_addr.sun_path, SOCKET_FILE, sizeof(srv_addr.sun_path) - 1);
	srv_len = sizeof(srv_addr);

	/* remove previous socket */
	unlink(SOCKET_FILE);

	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
		die("socket(AF_UNIX) failed");

	vdebug("Opened socket %u at '%s'", sock, SOCKET_FILE);

	set_nio(sock);
	set_coe(sock);

	if (bind(sock, (struct sockaddr *)&srv_addr, srv_len) < 0)
		die("bind(%s) failed", SOCKET_FILE);

	return sock;
}

void evsock_close(int sock)
{
	close(sock);
	unlink(SOCKET_FILE);
	vdebug("Closed socket %u", sock);
}

int evsock_connect(void)
{
	int sock;
	socklen_t srv_len;
	struct sockaddr_un srv_addr;

	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sun_family = AF_UNIX;
	strncpy(srv_addr.sun_path, SOCKET_FILE, sizeof(srv_addr.sun_path) - 1);
	srv_len = sizeof(srv_addr);

	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
		die("socket(AF_UNIX) failed");

	vdebug("Opened socket %u at '%s'", sock, SOCKET_FILE);

	set_nio(sock);
	set_coe(sock);

	if (connect(sock, (struct sockaddr *)&srv_addr, srv_len) < 0)
		die("connect(%s) failed", SOCKET_FILE);

	return sock;
}

void evsock_disconnect(int sock)
{
	close(sock);
	vdebug("Closed socket %u", sock);
}

int evsock_read(int sock, char *buf, size_t *len)
{
	ssize_t cnt;
	size_t size = *len;

	memset(buf, 0, size);
	*len = 0;

	while (1) {
		cnt = recv(sock, buf + *len, size - *len, 0);
		if (cnt < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN)
				break;
			error("failed receive on socket %i, err: %s (%i)\n",
			      sock, strerror(errno), errno);
			return -1;
		} else if (cnt == 0) {
			if (*len != 0)
				break;
			debug("empty response on socket %i, closing\n", sock);
			close(sock);
			return -1;
		}

		*len += cnt;
	}

	//trace("recv: socket %i; len %zu", sock, *len);

	return 0;
}

int evsock_write(int sock, char *buf, size_t len)
{
	ssize_t cnt;
	size_t sent = 0;

	while (1) {
		cnt = send(sock, buf + sent, len - sent, 0);
		if (cnt < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN)
				/* WUH? */
				continue;
			warn("failed send on socket %i, cnt %zu/%zu err: %s (%i)\n",
			     sock, len, sent, strerror(errno), errno);
			close(sock);
			return -1;
		} else if (cnt == 0) {
			if (sent != 0)
				break;
			debug("cannot send on socket %i, closing\n", sock);
			close(sock);
			return -1;
		}

		sent += cnt;
	}

	//trace("send: socket %i; cnt %zu/%zu", sock, len, sent);

	return 0;
}
