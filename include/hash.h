
#ifndef _HASH_H
#define _HASH_H

#include <stdbool.h>

#include "wqueue.h"

struct hentry {
	char *objname;
	unsigned int len;
	bool locked;
	int fd;
	struct wqentry *wq;
	struct hentry *next;
};

void hash_lock_chain(char *objname);
void hash_unlock_chain(char *objname);

void hash_init();
struct hentry *hash_lookup(char *objname, int create);
int hash_del(char *objname);


#endif

