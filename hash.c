#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "hash.h"
#include "config.h"


/* Hash Tables
 * Based on the ones from "The Practise of Programming" by Kernighan and Pike
 *
 * We use a single list to store the locks.
 */


/* the hash table */
static struct hentry *hash_table[HLEN];

/* and its locks */
static pthread_mutex_t hash_lock[HLEN];


void hash_init() {
	int i;
	for (i = 0; i < HLEN; i++) {
		hash_table[i] = NULL;
		pthread_mutex_init(&hash_lock[i], NULL);
	}
}


/* the hash function */
#define HASH_MULTIPLIER 37
unsigned int hash(char *s)
{
	unsigned int h;
	unsigned char *p;

	h = 0;
	for ( p = (unsigned char *)s; *p != '\0'; p++) {
		h = HASH_MULTIPLIER * h + *p;
	}
	return h % HLEN;
}

/* these are used to lock the chain for a given object, in order to assure
 * atomicity in the old_*_lock() calls */
void hash_lock_chain(char *objname) {
	int h;
	h = hash(objname);
	pthread_mutex_lock(&hash_lock[h]);
}

void hash_unlock_chain(char *objname) {
	int h;
	h = hash(objname);
	pthread_mutex_unlock(&hash_lock[h]);
}


/* from now on is just boring hash manipulation functions */
/* TODO: support multiple hash tables */

/* find a name in the table, with optional create */
struct hentry *hash_lookup(char *objname, int create)
{
	int h;
	struct hentry *match;

	h = hash(objname);

	for (match = hash_table[h]; match != NULL; match = match->next)
		if (strncmp(objname, match->objname, match->len) == 0)
			goto exit;

	if (create) {
		match = (struct hentry *) malloc(sizeof(struct hentry));
		match->len = strlen(objname) + 1;
		match->objname = (char *) malloc(match->len);
		memcpy(match->objname, objname, match->len);
		match->locked = 0;
		match->next = hash_table[h];
		match->wq = NULL;
		hash_table[h] = match;
	}
exit:
	return match;
}


/* removes an entry from the hash table; returns 1 if success or 0 if error
 * note that we don't care about waitqueues, we assume we don't have any to
 * free */
int hash_del(char *objname)
{
	int h;
	int rv = 0;
	struct hentry *p = NULL, *prev = NULL;

	h = hash(objname);

	for (p = hash_table[h]; p != NULL; p = p->next) {
		if (strncmp(objname, p->objname, p->len) == 0) {
			if (prev == NULL)
				hash_table[h] = p->next;
			else
				prev->next = p->next;
			free(p->objname);
			free(p);
			rv = 1;
			goto exit;
		}
		prev = p;
	}
exit:
	return rv;
}


