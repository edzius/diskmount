
#include <stdlib.h>
#include <string.h>

#include "evenv.h"
#include "util.h"

#define ENV_STORE_INC 50

static void env_expand(struct evenv *env)
{
	struct kv *tmp;
	size_t tmpsz;

	if (env->size > env->count)
		return;

	tmpsz = env->size + ENV_STORE_INC;
	vdebug("Expanding ENV: %u/%u -> %u", env->count, env->size, tmpsz);
	tmp = realloc(env->list, tmpsz * sizeof(*tmp));
	if (!tmp)
		die("realloc() failed");

	env->list = tmp;
	env->size = tmpsz;
}

static int env_append(struct evenv *env, char *line)
{
	char *pos;

	vdebug("Processing EVN line: '%s'", line);

	pos = strchr(line, '=');
	if (!pos) {
		vwarn("Anomalous EVN parameter: '%s'", line);
		return -1;
	}
	*pos = '\0';

	env_expand(env);
	env->list[env->count].key = line;
	env->list[env->count].val = pos + 1;
	env->count++;

	vinfo("Included EVN param, %u: '%s' = '%s'", env->count, line, pos+1);

	return 0;
}

struct evenv *env_init(char *data)
{
	struct evenv *env;
	char *token;
	const char *sep = "\n";

	if (!data) {
		debug("Missing ENV data");
		return NULL;
	}

	env = malloc(sizeof(*env));
	if (!env)
		die("malloc() failed");
	memset(env, 0, sizeof(*env));

	env->data = data;

	token = strtok(data, sep);
	while (token != NULL) {
		env_append(env, token);
		token = strtok(NULL, sep);
	}

	vinfo("Initialized ENV data: %p", env);

	return env;
}

void env_free(struct evenv *env)
{
	if (!env)
		return;

	if (env->list)
		free(env->list);

	free(env->data);
	free(env);

	vinfo("Released ENV data: %p", env);
}

char *env_lookup(struct evenv *env, const char *prop)
{
	int i;

	if (!prop) {
		verror("Missing lookup param: '%s'", prop);
		return NULL;
	}

	for (i = 0; i < env->count; i++)
		if (!strcmp(env->list[i].key, prop))
			return env->list[i].val;
	return NULL;
}
