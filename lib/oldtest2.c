
/*
 * Small testing application for the old lock server
 * It opens N locks simultaneously, and then releases them.
 * Alberto Bertogli (albertogli@telpin.com.ar)
 */

#include <stdlib.h>
#include <stdio.h>

#include "compiler.h"
#include "old.h"


int main(int argc, char **argv) {
	int fd, rv = 0, i, max;
	char obj[32];

	if (argc < 3) {
		printf("Use: oldtest2 host number_of_locks\n");
		return 1;
	}

	fd = old_connect(argv[1], 2626);
	if (fd < 0) {
		perror("Error in connect");
		return 1;
	}

	max = atoi(argv[2]);
	printf("Number of locks to held simultaneously: %d\n", max);
	
	for (i = 0; i < max; i++) {
		sprintf(obj, "LNUM %d", i);
		
		rv = old_lock(fd, obj);
		if (unlikely(rv < 0))
			break;
	}

	for (i = 0; i < max; i++) {
		sprintf(obj, "LNUM %d", i);
		
		rv = old_unlock(fd, obj);
		if (unlikely(rv < 0))
			break;
	}
	if (rv < 0)
		perror("ERROR");

	return 0;
}

