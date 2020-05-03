
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "util.h"
#include "diskev.h"
#include "evsock.h"

#ifndef RUN_PATH
#define RUN_PATH "/var/run/"
#endif

#define SOCKET_FILE RUN_PATH"diskmount.sock"

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

	vdebug("Opened event socket %u at '%s'", sock, SOCKET_FILE);

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
	vdebug("Closed event socket %u", sock);
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

	vdebug("Opened event socket %u at '%s'", sock, SOCKET_FILE);

	set_nio(sock);
	set_coe(sock);

	if (connect(sock, (struct sockaddr *)&srv_addr, srv_len) < 0)
		die("connect(%s) failed", SOCKET_FILE);

	return sock;
}

void evsock_disconnect(int sock)
{
	close(sock);
	vdebug("Closed event socket %u", sock);
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

	vdebug("recv data: socket %i, len %zu", sock, *len);

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

	vdebug("sent data: socket %i, cnt %zu/%zu", sock, len, sent);

	return 0;
}

int evev_parse(struct diskev *evt, char *data, int size)
{
	struct evtlv *tlv;
	int seek = 0;

	memset(evt, 0, sizeof(*evt));

	while (seek < size) {
		tlv = (struct evtlv *)data;
		seek += tlv->length + sizeof(*tlv);
		data += tlv->length + sizeof(*tlv);
		if (tlv->type == EVTYPE_DONE || tlv->length == 0 || seek > size)
			break;

		if (tlv->type == EVTYPE_ACTION)
			evt->action = strndup(tlv->value, tlv->length);
		else if (tlv->type == EVTYPE_DEVICE)
			evt->device = strndup(tlv->value, tlv->length);
		else if (tlv->type == EVTYPE_FS)
			evt->filesys = strndup(tlv->value, tlv->length);
		else if (tlv->type == EVTYPE_LABEL)
			evt->label = strndup(tlv->value, tlv->length);
		else if (tlv->type == EVTYPE_SERIAL)
			evt->serial = strndup(tlv->value, tlv->length);
		else if (tlv->type == EVTYPE_FSUUID)
			evt->fsuuid = strndup(tlv->value, tlv->length);
		else if (tlv->type == EVTYPE_PARTUUID)
			evt->partuuid = strndup(tlv->value, tlv->length);
		else
			vwarn("Unknown event IE type %u, length %u",
			      tlv->type, tlv->length);
	}

	/* We are sure event is for correct
	 * subsystem and device type, thus
	 * just forge them here. */
	evt->subsys = strdup("block");
	evt->type = strdup("partition");

	if (ev_check(evt)) {
		verror("Invalid event, size %u", size);
		ev_free(evt);
		return 1;
	}

	vinfo("Processed event, size %u", size);

	return 0;
}

static int evev_build_part(char *data, int type, char *line)
{
	struct evtlv *tlv;

	if (!line && type != EVTYPE_DONE)
		return 0;

	tlv = (struct evtlv *)data;
	tlv->type = type;
	tlv->length = 0;
	if (line)
		/* TLV size includes data (incl \0) and header length */
		tlv->length = sprintf(tlv->value, "%s", line) + 1;

	return tlv->length + sizeof(*tlv);
}

int evev_build(char *data, int size, struct diskev *evt)
{
	char *last = data + size;

	data += evev_build_part(data, EVTYPE_ACTION, evt->action);
	if (data >= last)
		return 0;
	data += evev_build_part(data, EVTYPE_DEVICE, evt->device);
	if (data >= last)
		return 0;
	data += evev_build_part(data, EVTYPE_FS, evt->filesys);
	if (data >= last)
		return 0;
	data += evev_build_part(data, EVTYPE_LABEL, evt->label);
	if (data >= last)
		return 0;
	data += evev_build_part(data, EVTYPE_SERIAL, evt->serial);
	if (data >= last)
		return 0;
	data += evev_build_part(data, EVTYPE_FSUUID, evt->fsuuid);
	if (data >= last)
		return 0;
	data += evev_build_part(data, EVTYPE_PARTUUID, evt->partuuid);
	if (data >= last)
		return 0;
	data += evev_build_part(data, EVTYPE_DONE, NULL);
	if (data >= last)
		return 0;

	return size - (last - data);
}
