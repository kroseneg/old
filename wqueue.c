
/* wait queues, used to queue file descriptors with requests for a lock */

#include <stdlib.h>
#include "wqueue.h"


/* adds a fd to the given waitqueue; always to the tail */
struct wqentry *wq_add(struct wqentry *wq, int fd) {
	struct wqentry *new;
	struct wqentry *aux;

	new = (struct wqentry *) malloc(sizeof(struct wqentry));
	new->fd = fd;
	new->next = NULL;

	/* if wq is NULL, it means it's empty, so we just replace it with our
	 * new one */
	if (wq == NULL) {
		return new;
	} else {
		/* go to the last one */
		for(aux = wq; aux->next != NULL; aux = aux->next)
			;

		aux->next = new;
	}
	return wq;
}

/* removes a given fd from the waitqueue */
struct wqentry *wq_del(struct wqentry *wq, int fd) {
	struct wqentry *p, *prev;
	prev = NULL;

	/* go through the list looking */
	for(p = wq; p != NULL; p = p->next) {
		if (p->fd == fd) {
			if (prev == NULL) {
				/* the first one is the match, advance wq */
				wq = p->next;
			} else {
				prev->next = p->next;
			}
			free(p);
			return wq;
		}
		prev = p;
	}

	/* we couldn't find a match, return the same queue */
	return wq;
}


