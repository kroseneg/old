/* 
 * Open Lock Daemon
 * 
 * Alberto Bertogli (albertogli@telpin.com.ar), 2003
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "config.h"
#include "lock.h"
#include "net.h"


void sighandler (int s) {
	exit(0);
}

int main(int argc, char **argv)
{
	int i, nthreads;
	pid_t pid;
	pthread_t *threads;

	/* parse the command line, we only have one optional parameter to tell
	 * the number of processing threads */
	if (argc == 1) {
		nthreads = 1;
	} else {
		nthreads = atoi(argv[1]);
		if (nthreads < 1) {
			printf("The number of threads must be at least 1\n");
			exit(1);
		} else if (nthreads > MAXTHREADS) {
			printf("The number of threads is greater than "
				"the configured maximum.\n"
				"Please either recompile to support more "
				"threads (MAXTHREADS in config.h),\n"
				"or change the number to be inside the "
				"allowed range (1 to %d).\n", MAXTHREADS);
			exit(1);
		}
	}


	/* ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	/* exit on SIGTERM */
	signal(SIGTERM, &sighandler);

	/* detach */
	pid = fork();
	if (pid > 0) {
		exit(0);
	} else if (pid < 0) {
		perror("Error forking:");
		exit(1);
	}

	/* pid == 0, we're the child now */

	/* setsid can't fail now, so we don't care */
	setsid();

	/* initialize network */
	if (!net_init(nthreads)) {
		exit(1);
	}
	
	/* create the threads */
	threads = malloc(nthreads * sizeof(pthread_t));
	for (i = 0; i < nthreads; i++) {
		/* we pass the thread number as the parameter pointer, which
		 * is used to index several arrays inside net.c */
		pthread_create(threads + i, NULL, &net_proc_loop, (void *) &i);
	}

	/* main select loop */
	net_select_loop(nthreads);
	
	/* wait for threads to complete */
	for (i = 0; i < nthreads; i++) {
		pthread_join(*(threads + i), NULL);
	}
	
	return 0;
}


