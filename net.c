
/* network stuff */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>

#include "wqueue.h"
#include "hash.h"
#include "net.h"
#include "lock.h"
#include "config.h"
#include "compiler.h"
#include "list.h"


/*
 * shared structures
 */

/* listen file descriptor */
static int lfd;

/* active file descriptors */
static bool active_fd[MAXCONN];

/* orphan locks */
static struct list *orphans;

/* buffers for partial command reading */
struct part_buf {
	struct net_cmd *cmd;
	char tmp[4];
	int read_so_far;
};
static struct part_buf bufs[MAXCONN];

/* owned locks per fd */
static struct list *olocks[MAXCONN];


/*
 * thread structures
 */

/* thread busy indicators */
static bool thread_busy[MAXTHREADS];

/* fd busy indicator */
static bool fd_busy[MAXCONN];

/* temp space used to tell threads which fd to process */
static unsigned int fd_to_process[MAXTHREADS];

/* lock array to 'wake up' idle threads */
static pthread_mutex_t thread_lock[MAXTHREADS];


/* 
 * private functions prototypes (publics are defined in net.h) 
 */

static int net_send_cmd(int fd, unsigned int op, char *payload, int len);
static struct net_cmd *net_get_cmd(int fd);
static int net_close(int fd);
static int net_parse(int fd, struct net_cmd *cmd);
static void clean_orphans();
static void clean_buffer(int fd);


/*
 * functions
 */

/* initialize network */
int net_init(int nthreads) {
	int i, rv;
	struct sockaddr_in addr;
	pthread_mutexattr_t attr;

	/* initialize per-thread info */
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
	for (i = 0; i < nthreads; i++) {
		pthread_mutex_init(&thread_lock[i], &attr);
		pthread_mutex_lock(&thread_lock[i]);
		thread_busy[i] = 0;
		fd_to_process[i] = 0;
	}
		
	/* build the listening socket */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = INADDR_ANY;
	
	lfd = socket(PF_INET, SOCK_STREAM, 0);
	rv = bind(lfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
	if (rv != 0) {
		perror("Error binding");
		return 0;
	}
	
	/* disable nagle algorithm, otherwise as we often handle small amounts
	 * of data it can make i/o quite slow */
	rv = 1;
	if (setsockopt(lfd, SOL_TCP, TCP_NODELAY, &rv, sizeof(rv)) < 0 ) {
		perror("Error setting socket options");
		return 0;
	}

	/* make it nonblocking */
	if (fcntl(lfd, F_SETFL, O_NONBLOCK) != 0) {
		perror("Error in enabling nonblocking I/O");
		return 0;
	}

	listen(lfd, SOMAXCONN);
	active_fd[lfd] = 1;

	return 1;
}	

/* main select loop */
void net_select_loop(int nthreads) {
	int rv, connfd, maxfd;
	unsigned int i, j, busycount;
	bool workdone;
	fd_set readfds;
	struct timeval tv;
	
	tv.tv_sec = ORPHAN_TIMEOUT;
	tv.tv_usec = 0;

	/* renice ourselves to low priority, to avoid stealing time from
	 * workers when looping looking for an idle thread */
	nice(20);
	
	/* build initially the fd sets for select */
	maxfd = lfd;
	FD_ZERO(&readfds);
	FD_SET(lfd, &readfds);

	for (;;) {
		rv = select(maxfd + 1, &readfds, NULL, NULL, &tv);
		if (unlikely(rv == 0)) {
			/* FIXME: this is linux-only, most other systems won't
			 * touch tv, so we need an alternative method for
			 * portable alarms */
			clean_orphans();
			tv.tv_sec = ORPHAN_TIMEOUT;
			tv.tv_usec = 0;
		} 
		
		if (unlikely(rv < 0)) {
			perror("Error in select");
			goto rebuild;
		}
		
		workdone = 0;
		for (i = lfd + 1; i <= maxfd; i++) {
			/* skip the ones not in the set or busy */
			if (!FD_ISSET(i, &readfds) || fd_busy[i])
				continue;
			
			/* loop looking for an idle thread; see
			 * doc/multithread */
			busycount = 0;
			for (j = 0; j < nthreads; j++) {
				if (!thread_busy[j]) {
					/* we found one, so mark it busy, tell
					 * the fd and unlock its lock, that
					 * will wake it up */
					thread_busy[j] = 1;
					fd_busy[i] = 1;
					fd_to_process[j] = i;
					pthread_mutex_unlock(&thread_lock[j]);
					workdone = 1;
					break;
				}
				busycount++;
			}
			
			/* if everybody is busy, yield the cpu */
			if (unlikely(busycount == nthreads)) {
				sched_yield();
			}
			
			/* if we didn't find any idle thread to assign the
			 * job, just loop and go back to select */
		}
		
		/* handle new connections */
		if (FD_ISSET(lfd, &readfds)) {
			connfd = accept(lfd, (struct sockaddr *) NULL, 0);
			
			if (connfd < 0) {
				perror("Error in accept");
				goto rebuild;
			}
			
			if (fcntl(connfd, F_SETFL, O_NONBLOCK) != 0) {
				perror("Error in enabling nonblocking I/O");
				close(connfd);
				goto rebuild;
			}
			
			if (connfd > maxfd)
				maxfd = connfd;
			
			active_fd[connfd] = 1;
			workdone = 1;
		}

		/* if we didn't do any work, yield the cpu */
		if (unlikely(!workdone)) {
			sched_yield();
		}
		
		/* rebuild the fd sets for select */
rebuild:
		FD_ZERO(&readfds);
		for (i = lfd; i <= maxfd; i++) {
			if (unlikely(active_fd[i] == 1)) {
				FD_SET(i, &readfds);
			}
		}
	}
	
	return;
	
}

/* processing loop, it's started always as a thread; see doc/multithread */
void *net_proc_loop(void *tno) {
	int fd, tid;
	struct net_cmd *cmd = NULL;
	
	tid = (int) tno;

	for (;;) {
		/* this lock gets unlocked by net_select_loop when we have an
		 * fd to process */
		pthread_mutex_lock(&thread_lock[tid]);
		
		fd = fd_to_process[tid];
		
		cmd = net_get_cmd(fd);
		if (cmd == NULL)
			goto end_loop;
		
		//printf("GOT CMD %d: %p\n", cmd->op, cmd->payload);
		net_parse(fd, cmd);
		
		if (cmd != NULL) {
			if (cmd->payload != NULL)
				free(cmd->payload);
			free(cmd);
			cmd = NULL;
		}
		
end_loop:
		/* mark the fd idle */
		fd_busy[fd] = 0;

		/* mark the thread idle */
		thread_busy[tid] = 0;
	}
	return NULL;
}

/* FIXME: these two are only for little endian, make them generic */
static int net_send_cmd(int fd, unsigned int op, char *payload, int len) {
	char header[4];
	char ver = 1;
	unsigned int ulen;

	/* just to simplify the code, if the lenght is lower than 0 it means
	 * that it's a string and we should measure its lenght (we include the
	 * ending \0)*/
	if (unlikely(len < 0)) {
		if (payload != NULL)
			ulen = strlen(payload) + 1;
		else
			ulen = 0;
	} else
		ulen = len;
	
	header[0] = ((ver << 4) & 0xF0) + (op >> 4);
	header[1] = ((op << 4) & 0xF0) + ((ulen & 0xFF0000) >> 16);
	header[2] = (ulen & 0x00FF00) >> 8;
	header[3] = ulen & 0x0000FF;
							
	//printf(">>> %hhu %hhu %hhu %hhu\n", header[0], header[1], header[2], header[3]);
	
	if (unlikely(write(fd, header, 4) != 4)) {
		clean_buffer(fd);
		net_close(fd);
		return 0;
	}
	
	if (unlikely(write(fd, payload, ulen) != ulen)) {
		clean_buffer(fd);
		net_close(fd);
		return 0;
	}
	
	return 1;
}

static struct net_cmd *net_get_cmd(int fd) {
	int missing;
	ssize_t s;
	struct net_cmd *cmd = NULL;
	struct part_buf *b = &bufs[fd];

	
	if (b->cmd == NULL) {
		/* we have no command filled yet, so we start by reading the
		 * header */
		
		/* do the reading into the 4-byte buffer */
		missing = 4 - b->read_so_far;
		s = read(fd, b->tmp + b->read_so_far, missing);
		if (unlikely(s <= 0)) {
			if (s == 0 || (errno != EAGAIN && errno != EINTR)) {
				clean_buffer(fd);
				net_close(fd);
			}
			return NULL;
		}
		
		b->read_so_far += s;
		
		if (unlikely(b->read_so_far < 4)) {
			/* still not enough, so just wait */
			return NULL;
		}

		/* ok, we got our header, so build it */
		cmd = (struct net_cmd *) malloc(sizeof(struct net_cmd));
		memset(cmd, 0, sizeof(struct net_cmd));
		
		cmd->ver = b->tmp[0] >> 4;
		cmd->op = ((b->tmp[0] & 0x0F ) << 4) + ((b->tmp[1] & 0xF0) >> 4);
		cmd->len = ( ((int) b->tmp[1] & 0x0F) << 16) +
			((int) b->tmp[2] << 8) + ((int) b->tmp[3]);
		
		//printf("VER: %d, LEN: %d\n", cmd->ver, cmd->len);
		//printf("<<< %hhu %hhu %hhu %hhu\n", b->tmp[0], b->tmp[1], b->tmp[2], b->tmp[3]);
		if (unlikely(cmd->ver != 1 || cmd->len > MAX_PAYLOAD)) {
			free(cmd);
			clean_buffer(fd);
			net_close(fd);
			return NULL;
		}
		
		if (likely(cmd->len)) {
			cmd->payload = (char *) malloc(cmd->len);
			memset(cmd->payload, 0, cmd->len);
		} else {
			cmd->payload = NULL;
		}

		b->cmd = cmd;
		
		/* put read_so_far to 0, it now refers to how much of
		 * the _payload_ we've read */
		b->read_so_far = 0;

		/* and empty the 4-byte buffer */
		memset(b->tmp, 0, 4);

		/* we don't return here, just keep going on reading
		 * the payload */			
		
	}

	/* we have the entire header, now we read the payload (or what's left
	 * of it) */

	if (unlikely(b->cmd->len == 0)) {
		/* a command without payload, just return it */
		cmd = b->cmd;
		cmd->payload = NULL;
		b->read_so_far = 0;
		b->cmd = NULL;
		return cmd;
	}
	
	missing = b->cmd->len - b->read_so_far;
	
	s = read(fd, b->cmd->payload + b->read_so_far, missing);
	if (unlikely(s <= 0)) {
		if (s == 0 || (errno != EAGAIN && errno != EINTR)) {
			clean_buffer(fd);
			net_close(fd);
		}
		return NULL;
	}
	b->read_so_far += s;
	
	if (likely(b->read_so_far == b->cmd->len)) {
		/* the command is complete! */
		cmd = b->cmd;
		b->read_so_far = 0;
		b->cmd = NULL;
		return cmd;
	} else {
		/* we are still missing some of the payload, just return NULL
		 * and wait for some more to come */
		return NULL;
	}
}

/* closes a fd */
static int net_close(int fd) {
	struct list *p;
	
	active_fd[fd] = 0;

	/* move open locks to the orphan list */
	if (olocks[fd] != NULL) {
		/* first, mark its open locks as orphans */
		for (p = olocks[fd]; p != NULL; p = p->next)
			lock_set_fd(p->name, -1);
			
		/* and then move them to the orphan list */
		orphans = list_mult_add(orphans, olocks[fd]);
		olocks[fd] = NULL;
	}
	
	return close(fd);
}


/* parse commands */
static int net_parse(int fd, struct net_cmd *cmd) {
	char *data = cmd->payload;
	unsigned int op = cmd->op;
	int hfd, send_op = 0;
	
	if (unlikely( data != NULL && (*(data + (cmd->len - 1)) != '\0' ) )) {
		/* the payload doesn't end in '\0' */
		//printf("BROKEN STRING! ");
		//printf("%hhu - %u\n", *(data + (cmd->len - 1)), cmd->len);
		net_close(fd);
		return 1;
	}
	
	switch (op) {
		case REQ_ACQ_LOCK:
			if (unlikely(cmd->len == 0))
				goto return_error;
			hash_lock_chain(data);
			if (lock_acquire(data, fd)) {
				olocks[fd] = list_add(olocks[fd], data);
				send_op = REP_LOCK_ACQUIRED;
			} else {
				send_op = REP_ACK;
			}
			hash_unlock_chain(data);
			net_send_cmd(fd, send_op, data, cmd->len);
			break;
			
		case REQ_REL_LOCK:
			if (unlikely(cmd->len == 0))
				goto return_error;
			hash_lock_chain(data);
			hfd = lock_release(data);
			if (hfd > 0) {
				olocks[hfd] = list_remove(olocks[hfd], data);
				send_op = REP_LOCK_RELEASED;
			} else if (hfd < 0) {
				/* it's an orphan */
				orphans = list_remove(orphans, data);
				send_op = REP_LOCK_RELEASED;
			} else {
				send_op = REP_ERR;
			}
			hash_unlock_chain(data);
			net_send_cmd(fd, send_op, data, cmd->len);
			break;
			
		case REQ_TRY_LOCK:
			if (unlikely(cmd->len == 0))
				goto return_error;
			hash_lock_chain(data);
			if (lock_trylock(data, fd)) {
				olocks[fd] = list_add(olocks[fd], data);
				send_op = REP_LOCK_ACQUIRED;
			} else {
				send_op = REP_LOCK_WBLOCK;
			}
			hash_unlock_chain(data);
			net_send_cmd(fd, send_op, data, cmd->len);
			break;

		case REQ_ADOPT:
			if (unlikely(cmd->len == 0))
				goto return_error;
			hash_lock_chain(data);
			if (list_lookup(orphans, data) == NULL) {
				send_op = REP_ERR;
			} else {
				lock_set_fd(data, fd);
				orphans = list_remove(orphans, data);
				olocks[fd] = list_add(olocks[fd], data);
				send_op = REP_ACK;
			}
			hash_unlock_chain(data);
			net_send_cmd(fd, send_op, data, cmd->len);
			break;

		case REQ_PING:
			net_send_cmd(fd, REP_PONG, data, cmd->len);
			break;

		default:
			net_send_cmd(fd, REP_ERR, data, cmd->len);
			break;
	}
	return 1;

return_error:
	net_send_cmd(fd, REP_ERR, NULL, 0);
	return 1;
}


/* wakes up the first fd waiting on the given hash entry */
int net_wakeup(struct hentry *h) {
	int fd;
	struct wqentry *p;
	
	/* get the fd from the first of the list */
	fd = h->wq->fd;
	active_fd[fd] = 1;

	/* then remove it from the waitqueue */
	p = h->wq->next;
	free(h->wq);
	h->wq = p;
	h->fd = fd;

	/* of course, mark it locked */
	h->locked = 1;
	
	/* add the open lock; the caller will take care to remove from the
	 * other queue */
	olocks[fd] = list_add(olocks[fd], h->objname);
		
	/* and send the reply */
	net_send_cmd(fd, REP_LOCK_ACQUIRED, h->objname, h->len);
	return 1;
}


/* clean the orphan lists by releasing the locks - the code is taken from the
 * command parser */
static void clean_orphans() {
	struct list *p;
	
	for (p = orphans; p != NULL; p = p->next) {
		hash_lock_chain(p->name);
		lock_release(p->name);
		hash_unlock_chain(p->name);
		/* this goes down here, otherwise we would free(p->name) */
		orphans = list_remove(orphans, p->name);
	}
}


/* cleans the temporary buffer for a given fd */
static void clean_buffer(int fd) {
	struct part_buf *b = &bufs[fd];
	b->read_so_far = 0;
	if (b->cmd != NULL) {
		if (b->cmd->payload != NULL)
			free(b->cmd->payload);
		free(b->cmd);
		b->cmd = NULL;
	}
	memset(b->tmp, 0, 4);
	/*
	if (b->tmp != NULL) {
		free(b->tmp);
		b->tmp = NULL;
	}
	*/
}	


