
#include <stdlib.h>
#include <string.h>

#include "list.h"

/* generic list handling */

struct list *list_add(struct list *head, char *name) {
	struct list *l;
	
	l = (struct list *) malloc(sizeof(struct list));
	l->len = strlen(name) + 1;
	l->name = (char *) malloc(l->len);
	memcpy(l->name, name, l->len);
	l->next = head;

	return l;
}

struct list *list_mult_add(struct list *head, struct list *new) {
	new->next = head;
	return new;
}

struct list *list_remove(struct list *head, char *name) {
	struct list *p, *prev;

	prev = NULL;
	for (p = head; p != NULL; p = p->next) {
		if (strncmp(name, p->name, p->len) == 0) {
			if (prev == NULL)
				head = p->next;
			else
				prev->next = p->next;
			free(p->name);
			free(p);
			return head;
		}
		prev = p;
	}
	/* item not in list, we return the original one */
	return head;
}

struct list *list_lookup(struct list *head, char *name) {
	struct list *p;
	
	for (p = head; p != NULL; p = p->next)
		if (strncmp(name, p->name, p->len) == 0)
			return p;
	return NULL;
}



