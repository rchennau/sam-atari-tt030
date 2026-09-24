/* t4lock — see t4lock.c. */
#ifndef T4LOCK_H
#define T4LOCK_H
/* 1 = the lock is ours, 0 = held by a live process (*why says which), -1 = could not create. */
int t4lock_take(const char **why);
void t4lock_release(void);                       /* only removes a lock this process holds */
void t4_loaded_set(const char *server);          /* record the server just booted (e.g. "compserv.btl") */
int t4_loaded_is(const char *server);            /* 1 SERVER is loaded · 0 another is · -1 unknown */
#endif
