
#ifndef _LIST_H
#define _LIST_H

struct list {
	char *name;
	unsigned int len;
	struct list *next;
};

struct list *list_add(struct list *head, char *name);
struct list *list_mult_add(struct list *head, struct list *new);
struct list *list_remove(struct list *head, char *name);
struct list *list_lookup(struct list *head, char *name);

#endif

