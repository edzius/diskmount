
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <arpa/inet.h>

#include "util.h"

#define NL_SOCKET_BUFSZ		32768

int nlsock_open(int subscribe)
{
	int sndbuf = NL_SOCKET_BUFSZ;
	int rcvbuf = NL_SOCKET_BUFSZ;
	int nlsock;
	struct sockaddr_nl addr;
	socklen_t addrlen;

	nlsock = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (nlsock < 0)
		die("socket(AF_NETLINK) failed");

	vdebug("Opened netlink socket %u, subscribe %i", nlsock, subscribe);

	set_nio(nlsock);
	set_coe(nlsock);

	if (setsockopt(nlsock, SOL_SOCKET, SO_SNDBUF,
		       &sndbuf, sizeof(sndbuf)) < 0)
		die("setsockopt(SO_SNDBUF) failed");

	if (setsockopt(nlsock, SOL_SOCKET, SO_RCVBUF,
		       &rcvbuf, sizeof(rcvbuf)) < 0)
		die("setsockopt(SO_RCVBUF) failed");

	addrlen = sizeof(addr);
	memset(&addr, 0, addrlen);

	addr.nl_family = AF_NETLINK;
	addr.nl_groups = subscribe;

	if (bind(nlsock, (struct sockaddr*)&addr, addrlen)) {
		die("bind() failed ion netlink socket");
	}

	if (getsockname(nlsock, (struct sockaddr*) &addr, &addrlen) < 0)
		die("getsockname() failed");

	if (addrlen != sizeof(addr) ||
	    addr.nl_family != AF_NETLINK)
		die("whatever ...");

	return nlsock;
}

void nlsock_close(int sock)
{
	close(sock);
	vdebug("Closed netlink socket %u", sock);
}

int nlsock_recv(int sock, char *buf, size_t len)
{
	int size;	/* received reply length */
	struct sockaddr_nl nladdr;
	struct iovec iov;
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};

	do {
		iov.iov_len = len;
		iov.iov_base = buf;
		size = recvmsg(sock, &msg, 0);
	} while (size < 0 && (errno == EINTR  || errno == ENOBUFS));
	// ENOBUFS is raised when NL socket buffer is full

	if (size < 0) {
		if (errno == EAGAIN)
			return 0;
		verror("failed receive on socket %i, err: %s (%i)\n",
		      sock, strerror(errno), errno);
		return -1;
	} else if (size == 0) {
		vdebug("empty responce on socket %i, closing\n", sock);
		close(sock);
		return 0;
	}

	// FIXME(edzius): This could cause NL events caching,
	// depending on FD poll method used. E.g. when used
	// with edge triggered poll it could lead to caching.
	if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
		warn("truncated message (%x), discard", msg.msg_flags);
		return 0;
	}

	return size;
}
