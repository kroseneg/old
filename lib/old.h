
/*
 * Header for the old (Open Lock Daemon) library
 */

#ifndef _OLD_H
#define _OLD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* some useful typedefs */
typedef uint8_t         u8;
typedef uint16_t        u16;
typedef uint32_t        u32;
typedef uint64_t        u64;


/* network operations and structures */

/* request opcodes */
#define REQ_ACQ_LOCK            1
#define REQ_REL_LOCK            2
#define REQ_TRY_LOCK            3
#define REQ_PING                4
#define REQ_ADOPT               5
#define REQ_SYNC                6

/* reply opcodes */
#define REP_LOCK_ACQUIRED       128
#define REP_LOCK_WBLOCK         129
#define REP_LOCK_RELEASED       130
#define REP_PONG                131
#define REP_ACK                 132
#define REP_ERR                 133
#define REP_SYNC                134

/* max payload size, (2^20 - 1) */
#define MAX_PAYLOAD (1048575)

/* standard old port */
#define OLD_PORT 2626

/* main command structure */
struct net_cmd {
	u32 len;
	u8 ver;
	u8 op;
	char *payload;
};

/* send and receive command, used mostly internally but exported just in case
 * anyone finds them useful */
int send_cmd(int fd, unsigned int op, char *payload, int len);
struct net_cmd *get_cmd(int fd);

/* these are the main functions; first you always use old_connect to get a
 * file descriptor to the server; and the other three represent the basic lock
 * operations, the first parameter is the file descriptor you got from
 * old_connect(), and the second is a string to identify the resource to
 * operate on */
int old_connect(char *host, int port);
int old_lock(int fd, char *s);
int old_unlock(int fd, char *s);
int old_trylock(int fd, char *s);


#ifdef __cplusplus
} /* from extern "C" avobe */
#endif

#endif

