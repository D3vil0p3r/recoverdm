#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "io.h"
#include "dev.h"
#include "gen.h"
#include "utils.h"
#include "error.h"

void * mymalloc(size_t size, char *what)
{
	void *dummy = malloc(size);
	if (!dummy)
		error_exit("failed to allocate %d bytes for %s\n", size, what);

	return dummy;
}

void * myrealloc(void *oldp, size_t newsize, char *what)
{
	void *dummy = realloc(oldp, newsize);
	if (!dummy)
		error_exit("failed to reallocate to %d bytes for %s\n", newsize, what);

	return dummy;
}

off64_t get_filesize(char *filename)
{
	struct stat64 finfo;

	if (stat64(filename, &finfo) == -1)
		error_exit("Failed to retrieve length of file %s: %s (%d)\n", filename, strerror(errno), errno);

	return finfo.st_size;
}

int copy_block(int fd_in, int fd_out, off64_t block_size)
{
	char buffer[512];

	while (block_size > 0)
	{
		size_t sector_size = min(block_size, 512);

		/* read from inputfile */
		if (READ(fd_in, buffer, sector_size) != sector_size)
		{
			fprintf(stderr, "Failed to read %d bytes from inputfile: %s (%d)\n", sector_size, strerror(errno), errno);
			return -1;
		}

		/* write to outputfile */
		if (phantom_write(fd_out, buffer, sector_size) != sector_size)
		{
			fprintf(stderr, "Failed to write to outputfile: %s (%d)\n", strerror(errno), errno);
			return -1;
		}

		/* decrease number of bytes left */
		block_size -= sector_size;
	}

	return 0;
}

void myseek(int fd, off64_t offset)
{
	/* seek in inputfile to correct offset */
	if (lseek64(fd, offset, SEEK_SET) == -1)
		error_exit("Failed to seek in file to offset %lld: %s (%d)\n", offset, strerror(errno), errno);
}
