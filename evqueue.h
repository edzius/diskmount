#ifndef _EVQUEUE_H
#define _EVQUEUE_H

#include "evenv.h"

void evq_add(struct evenv *env, off_t delay);
void evq_del(struct evenv *env);
struct evenv *evq_pop(void);
struct evenv *evq_peek(struct evenv *env);

#endif // _EVQUEUE_H
