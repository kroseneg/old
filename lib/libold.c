
/*
 * libold - a C library for old, the Open Lock Daemon
 * Alberto Bertogli (albertogli@telpin.com.ar)
 *
 * It's documented in the manual page inside the doc/ directory
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "compiler.h"
#include "old.h"

/* FIXME: these two are only for little endian, make them generic */
int send_cmd(int fd, unsigned int op, char *payload, int len) {
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
	
	if (unlikely(write(fd, header, 4) != 4)) {
		return 0;
	}
	
	if (unlikely(write(fd, payload, ulen) != ulen)) {
		return 0;
	}
	
	return 1;
}

struct net_cmd *get_cmd(int fd) {
	ssize_t s;
	struct net_cmd *cmd = NULL;
	char buf[4];
	
	/* first get the header (which is 4 bytes) */
	s = read(fd, buf, 4);
	if (unlikely(s != 4)) {
		return NULL;
	}
		
	/* parse it */
	cmd = (struct net_cmd *) malloc(sizeof(struct net_cmd));
	if (unlikely(cmd == NULL))
		return NULL;

	memset(cmd, 0, sizeof(struct net_cmd));
	
	cmd->ver = buf[0] >> 4;
	cmd->op = ((buf[0] & 0x0F ) << 4) + ((buf[1] & 0xF0) >> 4);
	cmd->len = ( ((int) buf[1] & 0x0F) << 16) +
		((int) buf[2] << 8) + ((int) buf[3]);

	if (unlikely(cmd->ver != 1 || cmd->len > MAX_PAYLOAD)) {
		free(cmd);
		return NULL;
	}
	
	if (likely(cmd->len)) {
		cmd->payload = (char *) malloc(cmd->len);
		if (unlikely(cmd->payload == NULL)) {
			free(cmd);
			return NULL;
		}
		memset(cmd->payload, 0, cmd->len);
	} else {
		cmd->payload = NULL;
	}

	/* and now, with the header done, read the payload */
	
	if (unlikely(cmd->len == 0)) {
		/* a command without payload, just return it */
		return cmd;
	}
	
	s = read(fd, cmd->payload, cmd->len);
	if (unlikely(s != cmd->len)) {
		free(cmd);
		return NULL;
	}
	
	/* the command is complete! */
	return cmd;
}

int old_lock(int fd, char *s) {
	/* gets a lock */
	struct net_cmd *cmd;
	
	send_cmd(fd, REQ_ACQ_LOCK, s, -1);
	cmd = get_cmd(fd);
	
	if (unlikely(cmd == NULL))
		return -1;
	
	if (cmd->op == REP_ACK)
		/* if we get an ACK, just wait for a definitive answer */
		cmd = get_cmd(fd);

	 if (unlikely(cmd == NULL))
		 return -1;

	if (cmd->op == REP_LOCK_ACQUIRED)
		return 1;
	
	return 0;
}

int old_unlock(int fd, char *s) {
	/* releases a lock */
	struct net_cmd *cmd;
	
	send_cmd(fd, REQ_REL_LOCK, s, -1);
	cmd = get_cmd(fd);
	
	if (unlikely(cmd == NULL))
		return -1;
	
	if (cmd->op == REP_LOCK_RELEASED)
		return 1;

	return 0;
}

int old_trylock(int fd, char *s) {
	/* tries to get a lock */
	struct net_cmd *cmd;
	
	send_cmd(fd, REQ_TRY_LOCK, s, -1);
	cmd = get_cmd(fd);
	
	if (unlikely(cmd == NULL))
		return -1;
	
	if (cmd->op == REP_LOCK_ACQUIRED)
		return 1;

	return 0;
}

int old_connect(char *host, int port) {
	int fd;
	int rv;
	struct hostent *hinfo;
	struct sockaddr_in sa;

	hinfo = gethostbyname(host);
	if (hinfo == NULL) {
		return -1;
	}
	
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr = *((struct in_addr *) hinfo->h_addr_list[0]);
	
	fd = socket(PF_INET, SOCK_STREAM, 0);

	rv = 1;
	if (setsockopt(fd, SOL_TCP, TCP_NODELAY, &rv, sizeof(rv)) < 0 ) {
		return -1;
	}
			
	
	rv = connect(fd, (struct sockaddr *) &sa, sizeof(sa));
	if (rv != 0) {
		return -1;
	}

	return fd;
	
}



