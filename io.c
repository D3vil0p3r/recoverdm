#define _LARGEFILE64_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "io.h"
#include "error.h"
#include "gen.h"

ssize_t READ(int fd, char *whereto, size_t len)
{
	ssize_t cnt=0;

	while(len>0)
	{
		ssize_t rc;

		rc = read(fd, whereto, len);

		if (rc == -1)
		{
			if (errno != EINTR)
			{
				if (errno != EIO)
					error_exit("unexpected error while reading: %s (%d)\n", strerror(errno), errno);

				return -1;
			}
		}
		else if (rc == 0)
		{
			break;
		}
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

ssize_t WRITE(int fd, char *whereto, size_t len)
{
	ssize_t cnt=0;

	while(len>0)
	{
		ssize_t rc;

		rc = write(fd, whereto, len);

		if (rc == -1)
		{
			if (errno != EINTR)
			{
				fprintf(stderr, "WRITE::write: %d\n", errno);
				return -1;
			}
		}
		else if (rc == 0)
		{
			break;
		}
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

ssize_t phantom_write(int fd, char *in, size_t nbytes)
{
	ssize_t cnt = 0;
	size_t move_a_little;
	off64_t curpos;
	int dummy_rc;

	/* cannot create phantom blocks if in O_APPEND mode */
	dummy_rc = fcntl(fd, F_GETFL);
	if (dummy_rc == -1)
	{
		fprintf(stderr, "phantom_write::fnctl: %d\n", errno);
		return -1;	/* error */
	}
	if ((dummy_rc & O_APPEND) == O_APPEND)
		return WRITE(fd, in, nbytes); /* in append you cannot create phantom blocks */

	curpos = lseek64(fd, (off64_t)0, SEEK_CUR);
	/* could not determine current location, bail out */
	if (curpos == -1)
	{
		fprintf(stderr, "phantom_write::lseek error: %d\n", errno);
		return -1;
	}

	/* determine how many bytes to seek to get at the start of
	 * a sector
	 */
	move_a_little = 512 - (curpos & 511);
	if (move_a_little == 512)
		move_a_little = 0;
	else
		move_a_little = min(nbytes, move_a_little);

	while(nbytes > 0)
	{
		int rc;

		/* first, move to the start of sector. otherwhise a lseek()
		 * will not have the effect of creating phantom blocks
		 */
		if (move_a_little > 0)
		{
			if ((rc = WRITE(fd, in, move_a_little)) <= 0)
			{
				fprintf(stderr, "phantom_write::WRITE: error\n");
				return rc;
			}

			move_a_little -= rc;
		}
		/* if more then 512 bytes to write, try to create phantom
		 * blocks
		 */
		else if (nbytes >= 512)
		{
			int loop = 0;
			char nulls = 1;

			for(loop=0; loop<512; loop++)
			{
				if (in[loop])
				{
					nulls = 0;
					break;
				}
			}

			if (nulls)
			{
				if (lseek64(fd, (off64_t)512, SEEK_CUR) == -1)
				{
					fprintf(stderr, "phantom_write::lseek: %d\n", errno);
					return -1;
				}

				rc = 512;
			}
			else
			{
				if ((rc = WRITE(fd, in, 512)) <= 0)
				{
					fprintf(stderr, "phantom_write::WRITE: error: %d\n", errno);
					return rc;
				}
			}
		}
		else
		{
			if ((rc = WRITE(fd, in, nbytes)) <= 0)
			{
				fprintf(stderr, "phantom_write::WRITE: error: %d\n", errno);
				return rc;
			}
		}

		in += rc;
		cnt += rc;
		nbytes -= rc;
	}

	return cnt;
}
