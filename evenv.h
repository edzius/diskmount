#ifndef _EVENV_H
#define _EVENV_H

struct kv {
	char *key;
	char *val;
};

struct evenv {
	char *data;
	size_t count;
	size_t size;
	struct kv *list;
};

struct evenv *env_init(char *data, int size);
void env_free(struct evenv *env);
char *env_lookup(struct evenv *env, const char *prop);

#endif // _EVENV_H
