
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <arpa/inet.h>

#include "util.h"

#define NL_SOCKET_BUFSZ		32768
#define NL_SOCKET_PROTO		NETLINK_KOBJECT_UEVENT
//  MONITOR_GROUP_KERNEL,
//  MONITOR_GROUP_UDEV,
#define NL_SOCKET_SUB		-1
#define UDEV_MONITOR_MAGIC	0xfeedcafe

struct udev_monitor_netlink_header {
	/* "libudev" prefix to distinguish libudev and kernel messages */
	char prefix[8];
	/*
	 * magic to protect against daemon <-> library message format mismatch
	 * used in the kernel from socket filter rules; needs to be stored in network order
	 */
	unsigned int magic;
	/* total length of header structure known to the sender */
	unsigned int header_size;
	/* properties string buffer */
	unsigned int properties_off;
	unsigned int properties_len;
	/*
	 * hashes of primary device properties strings, to let libudev subscribers
	 * use in-kernel socket filters; values need to be stored in network order
	 */
	unsigned int filter_subsystem_hash;
	unsigned int filter_devtype_hash;
	unsigned int filter_tag_bloom_hi;
	unsigned int filter_tag_bloom_lo;
};

int nlsock_open(void)
{
	int sndbuf = NL_SOCKET_BUFSZ;
	int rcvbuf = NL_SOCKET_BUFSZ;
	int subscribe = NL_SOCKET_SUB;
	int protocol = NL_SOCKET_PROTO;
	int nlsock;
	struct sockaddr_nl addr;
	socklen_t addrlen;

	nlsock = socket(AF_NETLINK, SOCK_RAW, protocol);
	if (nlsock < 0)
		die("socket(AF_NETLINK) failed");

	vdebug("Opened netlink socket %u, protocol %i", nlsock, protocol);

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

static int nlsock_recv(int sock, char *buf, size_t len)
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

int nlsock_read(int sock, char *buf, size_t len)
{
	struct udev_monitor_netlink_header *umh;
	int descr;
	size_t nllen;
	size_t nlsize = getpagesize() * 4;
	char dsbuf[nlsize]; /* 4 pages should be enough */
	char *nlbuf = dsbuf;

	nllen = nlsock_recv(sock, nlbuf, nlsize);
	if (nllen < 0) {
		debug("Failed to read uevent message");
		return -1;
	} else if (nllen == 0) {
		debug("Empty payload of uevent message");
		return 0;
	}

	umh = (struct udev_monitor_netlink_header *)nlbuf;
	if (nllen > sizeof(*umh)) {
		/* Discard udev monitor messages */
		if (!strcmp(umh->prefix, "libudev") && (ntohl(umh->magic) == UDEV_MONITOR_MAGIC)) {
			vinfo("Skipping udev monitor message");
			return 0;
		}
	}

	/* Skip first desriptive line if exist. */
	if (strchr(nlbuf, '@')) {
		descr = strlen(nlbuf) + 1;
		nllen -= descr;
		nlbuf += descr;
	}

	if (len <= nllen) {
		error("Too small NL payload buffer");
		return -2;
	}

	memcpy(buf, nlbuf, nllen);
	return nllen;
}
