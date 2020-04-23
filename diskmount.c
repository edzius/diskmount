
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "evsock.h"
#include "util.h"

static char *buf = NULL;
static int size = 0;
static int used = 0;

static void append(char *line)
{
	int len = strlen(line) + 1;

	if (size > used + len)
		goto insert;

	size += MAX(len, 1024);
	buf = realloc(buf, size);
	if (!buf)
		die("realloc() failed");

insert:
	used += sprintf(buf + used, "%s\n", line);
}

int main(int argc, char *argv[])
{
	char **env;
	int evsock;

	evsock = evsock_connect();

	for (env = environ; *env; ++env)
		append(*env);

	if (evsock_write(evsock, (char *)&used, sizeof(used)))
		die("Cannot write event size\n");
	if (evsock_write(evsock, buf, used))
		die("Cannot write event content\n");

	evsock_disconnect(evsock);

	return 0;
}
