
/*
 * Small testing application for the old lock server
 * Alberto Bertogli (albertogli@telpin.com.ar)
 */

#include <stdlib.h>
#include <stdio.h>

#include "compiler.h"
#include "old.h"


int main(int argc, char **argv) {
	int fd, rv = 0, i, max;

	if (argc < 3) {
		printf("Use: oldtest host iterations\n");
		return 1;
	}

	fd = old_connect(argv[1], 2626);
	if (fd < 0) {
		perror("Error in connect");
		return 1;
	}

	max = atoi(argv[2]);
	printf("Number of iterations: %d\n", max);
	
	/* unlock, just in case it's locked, this makes testing easier */
	old_unlock(fd, "ABCD");

	for (i = 0; i < max; i++) {
		rv = old_lock(fd, "ABCD");
		if (unlikely(rv < 0))
			break;
		
		rv = old_unlock(fd, "ABCD");
		if (unlikely(rv < 0))
			break;
	}
	if (rv < 0)
		perror("ERROR");

	return 0;
}


