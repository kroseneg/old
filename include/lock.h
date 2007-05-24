#ifndef _LOCK_H
#define _LOCK_H

int lock_acquire(char *s, int fd);
int lock_trylock(char *s, int fd);
int lock_release(char *s);
int lock_set_fd(char *s, int fd);
int lock_remove_fd(char *s, int fd);

#endif
