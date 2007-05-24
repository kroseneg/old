
#ifndef _WQ_H
#define _WQ_H


/* an entry in the wait queue */
struct wqentry {
	int fd;
	struct wqentry *next;
};

struct wqentry *wq_add(struct wqentry *wq, int fd);
struct wqentry *wq_del(struct wqentry *wq, int fd);


#endif

