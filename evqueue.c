
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "list.h"
#include "util.h"
#include "evenv.h"

struct evstash {
	time_t ts;
	struct evenv *env;
	struct list_head list;
};

LLIST_HEAD(event_queue);

static struct evstash *evq_find(struct evenv *env)
{
	int i;
	char *nkey, *okey;
	struct evstash *evs, *n;

	nkey = env_lookup(env, "ID_FS_UUID");
	if (!nkey)
		return NULL;

	list_for_each_entry_safe(evs, n, &event_queue, list) {
		okey = env_lookup(evs->env, "ID_FS_UUID");
		if (!okey)
			continue;

		if (!strcmp(nkey, okey))
			return evs;
	}

	return NULL;
}

void evq_add(struct evenv *env, off_t delay)
{
	struct evstash *evs;

	evs = malloc(sizeof(*evs));
	if (!evs)
		die("malloc() failed");

	evs->ts = time(NULL) + delay;
	evs->env = env;
	list_add_tail(&evs->list, &event_queue);
	vinfo("Scheduled event: %p, time %u", evs->env, evs->ts);
}

void evq_del(struct evenv *env)
{
	struct evstash *evs;

	evs = evq_find(env);
	if (!evs)
		return;

	list_del(&evs->list);
	env_free(evs->env);
	free(evs);
	vinfo("Canceled event: %p", evs->env);
}

struct evenv *evq_pop(void)
{
	time_t ts;
	struct evstash *evs, *n;
	struct evenv *env = NULL;

	ts = time(NULL);

	list_for_each_entry_safe(evs, n, &event_queue, list) {
		vdebug("Checking event %p, time %u/%u", evs->env, evs->ts, ts);
		if (evs->ts <= ts) {
			env = evs->env;
			break;
		}
	}

	if (!env) {
		vdebug("No scheduled events, time %u", ts);
		return NULL;
	}

	vinfo("Popping event: %p, time %u", evs->env, ts);
	list_del(&evs->list);
	free(evs);
	return env;
}

struct evenv *evq_peek(struct evenv *env)
{
	struct evstash *evs;

	evs = evq_find(env);
	if (!evs) {
		vinfo("No similar events to %p", env);
		return NULL;
	}
	return evs->env;

}
