
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hash.h"
#include "wqueue.h"
#include "net.h"


/* gets a lock identified by s for the given fd; if the lock is granted
 * returns 1, if not, it puts fd in the waitqueue and returns 0 */
int lock_acquire(char *s, int fd) {
	int rv;
	struct hentry *h;

	h = hash_lookup(s, 1);
	
	if (h->locked == 1) {
		h->wq = wq_add(h->wq, fd);
		rv = 0;
	} else {
		h->locked = 1;
		h->fd = fd;
		rv = 1;
	}

	return rv;
}


/* tries to get a lock, returns 1 if the lock is granted, 0 if it's not */
int lock_trylock(char *s, int fd) {
	int rv;
	struct hentry *h;

	h = hash_lookup(s, 1);

	if (h->locked == 1) {
		rv = 0;
	} else {
		h->locked = 1;
		h->fd = fd;
		rv = 1;
	}

	return rv;
}


/* releases a lock and wakes up the first waiter; returns 1 if the lock was
 * released properly or 0 if it wasn't (ie. it didn't exist) */
int lock_release(char *s) {
	int rv;
	struct hentry *h;
	
	h = hash_lookup(s, 0);
	
	if (h) {
		h->locked = 0;
		rv = h->fd;
		if (h->wq != NULL)
			net_wakeup(h);
		else
			/* if noone is waiting on this, destroy it */
			hash_del(s);
	} else {
		rv = 0;
	}
	
	return rv;
}

/* sets a lock's fd, return the previous fd */
int lock_set_fd(char *s, int fd) {
	int rv;
	struct hentry *h;
	
	h = hash_lookup(s, 0);

	if (h) {
		rv = h->fd;
		h->fd = fd;
	} else {
		rv = -1;
	}

	return rv;
}

