#ifndef _NLSOCK_H
#define _NLSOCK_H

#define UEVENT_KERNEL 1
#define UEVENT_UDEV 2

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

int nlsock_open(int subscribe);
void nlsock_close(int sock);
int nlsock_recv(int sock, char *buf, size_t len);

#endif // _NLSOCK_H
