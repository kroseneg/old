
#ifndef _NET_H
#define _NET_H


#include <stdint.h>
#include "hash.h"

/* some useful typedefs */
typedef uint8_t 	u8;
typedef uint16_t 	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;


/* network operations and structures */

/* request opcodes */
#define REQ_ACQ_LOCK		1
#define REQ_REL_LOCK		2
#define REQ_TRY_LOCK		3
#define REQ_PING		4
#define REQ_ADOPT		5
#define REQ_SYNC		6

/* reply opcodes */
#define REP_LOCK_ACQUIRED	128
#define REP_LOCK_WBLOCK		129
#define REP_LOCK_RELEASED	130
#define REP_PONG		131
#define REP_ACK			132
#define REP_ERR			133
#define REP_SYNC		134

/* max payload size, (2^20 - 1) */
#define MAX_PAYLOAD (1048575)

/* main command structure */
struct net_cmd {
	u32 len;
	u8 ver;
	u8 op;
	char *payload;
};

/* public functions */
int net_init(int nthreads);
void net_select_loop(int nthreads);
void *net_proc_loop(void *tno);
int net_wakeup(struct hentry *h);

#endif

