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
#include "utils.h"
#include "error.h"

#define N_RETRIES 6
#define N_CD_RAW_RETRIES 6
#define DEFAULT_CD_SPEED 1

void version(void)
{
	printf("recoverdm v" VERSION ", (c) 2003-2004 by folkert@vanheusden.com\n\n");
}

void usage(void)
{
	fprintf(stderr, "Usage: recoverdm -t <type> -i <file/device-in> -o <fileout> [-l <sectorsfile>] [-n # retries]\n");
	fprintf(stderr, "                 [-s rotation speed (CD-ROM etc.)] [-r # CD/DVD RAW read retries]\n");
	fprintf(stderr, "                 [-b start offset] [-p skip blocks count]\n");
	fprintf(stderr, "Number of retries defaults to %d. For CD-ROMs it's advised to use 1.\n", N_RETRIES);
	fprintf(stderr, "Number of CD/DVD RAW read retries defaults to %d. It is advised to use at least 3.\n", N_CD_RAW_RETRIES);
	fprintf(stderr, "CD-ROM (and DVD) speed defaults to %d.\n", DEFAULT_CD_SPEED);
	fprintf(stderr, "Skip blocks count is how many sectors are skipped after non-read one. Use\n");
	fprintf(stderr, "more to speed-up the recover process. Default is 1.\n");
	fprintf(stderr, "Type can be:\n");
	fprintf(stderr, "\tFILE\t\t1\n");
	fprintf(stderr, "\tFLOPPY\t\t10\n");
	fprintf(stderr, "\tFLOPPY_IDE\t11\n");
	fprintf(stderr, "\tFLOPPY_SCSI\t12\n");
	fprintf(stderr, "\tCDROM_IDE\t20\n");
	fprintf(stderr, "\tCDROM_SCSI\t21\n");
	fprintf(stderr, "\tDVD_IDE\t\t30\n");
	fprintf(stderr, "\tDVD_SCSI\t31\n");
	fprintf(stderr, "\tDISK_IDE\t40\n");
	fprintf(stderr, "\tDISK_SCSI\t41\n");
	fprintf(stderr, "-l generates a mapfile containing checksums and a list of badsectors. This map-\n");
	fprintf(stderr, "   file can then be used with `mergebad' to create one correct image from\n");
	fprintf(stderr, "   several damaged images\n");
}

void lba_to_msf(off64_t lba, unsigned char *minute, unsigned char *second, unsigned char *frame)
{
	if (lba >= -150)
	{
		*minute = (lba + 150) / (60 * 75);
		lba -= (*minute) * 60 * 75;
		*second = (lba + 150) / 75;
		lba -= (*second) * 75;
		*frame = (lba + 150);
	}
	else
	{
		*minute = (lba + 450150) / (60 * 75);
		lba -= (*minute) * 60 * 75;
		*second = (lba + 450150) / 75;
		lba -= (*second) * 75;
		*frame = (lba + 450150);
	}
}

int create_sector(unsigned char **sectors, int n_sectors, size_t block_size, char **psector_out)
{
	short n_times_ok[n_sectors], max_n_ok=0;
	char selection[block_size];
	char *sector_out = (char *)mymalloc(block_size, "sector");
	int loop, max_n_ok_index=0;

	*psector_out = sector_out;

	memset(n_times_ok, 0x00, sizeof(n_times_ok));
	memset(selection, 0x00, sizeof(selection));

	/* do for every byte */
	for(loop=0; loop<block_size; loop++)
	{
		int loop2;
		short chosen_byte = -1, chosen_byte_count=1; /* 1!! they're all at least 1 time used
							      * so this way chosen_byte will stay -1
							      * if none of them is > 1 times used */
		int count[256];

		memset(count, 0x00, sizeof(count));

		/* select byte -> chosen_byte
		 * or -1 if all different
		 */
		/* first, count how many times every byte occured */
		for(loop2=0; loop2<n_sectors; loop2++)
		{
			count[sectors[loop2][loop]]++;
		}
		/* then, find out which byte occured the most */
		for(loop2=0; loop2<256; loop2++)
		{
			if (count[loop2] > chosen_byte_count)
			{
				chosen_byte_count = count[loop2];
				chosen_byte = loop2;
			}
			/* what to do with '== chosen_byte_counter'? ... */
		}

		/* found a byte occuring frequently? then remember this byte
		 * and remember how many times a sector "had it right"
		 */
		if (chosen_byte != -1)
		{
			/* keep track of how many tims a sector "had it right" */
			for(loop2=0; loop2<n_sectors; loop2++)
			{
				if (sectors[loop2][loop] == chosen_byte)
					n_times_ok[loop2]++;
			}

			/* remember byte */
			sector_out[loop] = chosen_byte;
			/* and remember that we've put in a byte here */
			selection[loop] = 1;
		}
	}

	/* now find out which sector was selected the most */
	for(loop=0; loop<n_sectors; loop++)
	{
		if (n_times_ok[loop] > max_n_ok)
		{
			max_n_ok = n_times_ok[loop];
			max_n_ok_index = loop;
		}
	}

	/* then, for every byte that was not filled in, pick the byte from
	 * the previously selected sector
	 */
	for(loop=0; loop<block_size; loop++)
	{
		if (selection[loop] == 0)
		{
			sector_out[loop] = sectors[max_n_ok_index][loop];
		}
	}

	return 0;
}

int get_raw_cd(int fd, int dev_type, off64_t cur_pos, int n_raw_retries, char *whereto)
{
	int rc = 0;
	unsigned char minute, second, frame;
	size_t block_size = 2048; /* size of sector on a DVD/CD */

	lba_to_msf(cur_pos / 2048, &minute, &second, &frame);

	if (n_raw_retries == 2)
		n_raw_retries = 1;

	if (n_raw_retries >= 3)
	{
		char **sector_list = (char **)mymalloc(sizeof(char *) * n_raw_retries, "sectorlist");
		int loop;
		char *dummy = NULL;

		for(loop=0; loop<n_raw_retries; loop++)
			sector_list[loop] = (char *)mymalloc(block_size, "sector");

		for(loop=0; loop<n_raw_retries; loop++)
		{
			if ((rc = read_raw_cd(fd, minute, second, frame, sector_list[loop])) == -1)
				break;
		}

		if (rc == 0)
		{
			if (create_sector(sector_list, n_raw_retries, block_size, &dummy) == -1)
				rc = -1;
		}

		for(loop=0; loop<n_raw_retries; loop++)
			free(sector_list[loop]);
		free(sector_list);

		if (dummy)
		{
			memcpy(whereto, dummy, block_size);
			free(dummy);
		}
	}
	else
	{
		rc = read_raw_cd(fd, minute, second, frame, whereto);
	}

	return rc;
}

int main(int argc, char *argv[])
{
	int fdin, fdout;
	off64_t prevpos, curpos=(off64_t)-1, lastok=(off64_t)-1, the_end;
	off64_t start_offset = 0;
	int skip_value = 1; // in blocks
	int n=0;
	size_t block_size;
	int c;
	char dev_type=-1, cd_speed=DEFAULT_CD_SPEED;
	char n_retries = N_RETRIES;
	char n_raw_cd_retries = N_CD_RAW_RETRIES;
	FILE *dsecfile=NULL;
	char *file_in=NULL, *file_out=NULL, *file_list=NULL;
	char *buffer, *buffer2;

	version();

	if (argc == 1)
	{
		usage();
		return -1;
	}

	while((c = getopt(argc, argv, "i:o:l:t:p:n:s:r:b:Vh")) != -1)
	{
		switch(c)
		{
		case 't':
			dev_type = atoi(optarg);
			if (dev_type != DT_FILE &&
			    dev_type != DT_FLOPPY && dev_type != DT_FLOPPY_IDE && dev_type != DT_FLOPPY_SCSI &&
			    dev_type != DT_CDROM_IDE && dev_type != DT_CDROM_SCSI &&
			    dev_type != DT_DVD_IDE && dev_type != DT_DVD_SCSI &&
			    dev_type != DT_DISK_IDE && dev_type != DT_DISK_SCSI)
			{
				usage();
				return -1;
			}
			break;
		case 'i':
			file_in = strdup(optarg);
			break;
		case 'o':
			file_out = strdup(optarg);
			break;
		case 'l':
			file_list = strdup(optarg);
			break;
		case 'n':
			n_retries = atoi(optarg);
			break;
		case 'p':
			skip_value = atoi(optarg);
			break;
		case 's':
			cd_speed = atoi(optarg);
			break;
		case 'r':
			n_raw_cd_retries = atoi(optarg);
			break;
		case 'V':
			return 0;
		case 'b':
			start_offset = atoll(optarg);
			break;
		case '?':
		case 'h':
			usage();
			return 0;
		default:
			usage();
			return 0;
		}
	}

	if (file_in == NULL || file_out == NULL)
	{
		usage();
		return -1;
	}

	if (start_offset & 511)
	{
		fprintf(stderr, "Start offset must be a multiple of 512!\n");
		return -1;
	}
	
	if (skip_value <= 0)
	{
		fprintf(stderr, "Skip value must be positive!\n");
		return -1;
	}

	if (dev_type == DT_CDROM_IDE || dev_type == DT_CDROM_SCSI)
		block_size = 2048;
	else if (dev_type == DT_DVD_IDE || dev_type == DT_DVD_SCSI)
		block_size = 2048;
	else
		block_size = 512;

	buffer = (char *)mymalloc(block_size, "sectorbuffer");
	buffer2 = (char *)mymalloc(block_size, "sectorbuffer for retries");

	fdout = open64(file_out, O_WRONLY | O_CREAT | O_EXCL | O_SYNC, S_IRUSR | S_IWUSR);
	if (fdout == -1)
	{
		printf("Cannot create file %s! (%s)\n", file_out, strerror(errno));
		return -1;
	}
#if (_XOPEN_VERSION >= 600)
	(void)posix_fadvise(fdout, 0, 0, POSIX_FADV_SEQUENTIAL); // or POSIX_FADV_NOREUSE?
#endif

	fdin = open64(file_in, O_RDONLY);
	if (fdin == -1)
	{
		printf("Cannot open file %s: this is beyond the capabilities\n", file_in);
		printf("of this program.\n");
		return -1;
	}
#if (_XOPEN_VERSION >= 600)
	(void)posix_fadvise(fdin, 0, 0, POSIX_FADV_SEQUENTIAL); // or POSIX_FADV_NOREUSE?
#endif

	if (file_list != NULL)
	{
		dsecfile = fopen(file_list, "w");
		if (!dsecfile)
		{
			printf("Cannot create file %s.\n", file_list);
			return -1;
		}
#if (_XOPEN_VERSION >= 600)
	(void)posix_fadvise(fileno(dsecfile), 0, 0, POSIX_FADV_SEQUENTIAL); // or POSIX_FADV_NOREUSE?
#endif
	}

	(void)init_device(fdin, dev_type, cd_speed);

	the_end = lseek64(fdin, (off64_t)0, SEEK_END);
	
	if (lseek64(fdin, start_offset, SEEK_SET) == -1)
	{
		printf("Problem seeking to start of file/device.\n");
		return -1;
	}

	for(;;)
	{
		ssize_t rc;

		prevpos = curpos;
		curpos = lseek64(fdin, (off64_t)0, SEEK_CUR);

		printf("%lld\r", curpos);
		fflush(stdout);

		rc = READ(fdin, buffer, block_size);

		if (rc == 0) /* end of file/device */
			break;

		if (rc > 0)
		{
			n = 0;

			if (lseek64(fdout, curpos, SEEK_SET) == -1)
			{
				fprintf(stderr, "Problem seeking in outputfile! [%d]\n", errno);
				break;
			}
			if (phantom_write(fdout, buffer, (size_t)rc) <= 0)
			{
				fprintf(stderr, "Error writing to file! [%d]\n", errno);
				break;
			}
		}
		else if (rc == -1)
		{
			n++;

			printf("error at %lld bytes, retrying: %d\n", curpos, n);
			fflush(stdout);

			if (n > 1)
			{
				if (prevpos != curpos)
				{
					fprintf(stderr, "Confused! Continuing...\n");
				}
			}

			/* try seeking */
			switch(n)
			{
			case 2:		/* seek to last known good */
				if (lastok != (off64_t)-1)
				{
					printf("seek to last known good...\n");
					(void)lseek64(fdin, lastok, SEEK_SET);
					(void)clear_buffers(fdin, dev_type);
					(void)READ(fdin, buffer2, block_size);
				}
				break;

			case 3:		/* seek to end */
				if (the_end != (off64_t)-1)
				{
					printf("seek to end...\n");
					(void)lseek64(fdin, the_end, SEEK_SET);
					(void)clear_buffers(fdin, dev_type);
					(void)READ(fdin, buffer2, block_size);
				}
				break;

			case 4:		/* reset device */
				printf("resetting device...\n");
				(void)reset_device(fdin, dev_type);
				break;

			case 5:		/* reset controller */
				printf("resetting controller...\n");
				(void)reset_controller(fdin, dev_type);
				break;
			}

			if (lseek64(fdin, curpos, SEEK_SET) == -1)
			{
				printf("Problem seeking in input-file! [%d] (current sector)\n", errno);
			}

			if (n == n_retries)
			{
				char ok = 1;

				/* always mark the sector as tricky */
				if (dsecfile)
				{
					fprintf(dsecfile, "%lld %ld\n", curpos, block_size*skip_value);
					fflush(dsecfile);
				}

				n = 0;

				if (dev_type == DT_CDROM_IDE || dev_type == DT_CDROM_SCSI ||
				    dev_type == DT_DVD_IDE || dev_type == DT_DVD_SCSI)
				{
					char raw_buffer[2048];

					fprintf(stderr, "Trying RAW read...\n");

					if (get_raw_cd(fdin, dev_type, curpos, n_raw_cd_retries, raw_buffer) == 0)
					{
						if (lseek64(fdout, curpos, SEEK_SET) == -1)
						{
							fprintf(stderr, "Problem seeking in outputfile! [%d]\n", errno);
							ok = 0;
						}
						else if (phantom_write(fdout, raw_buffer, (size_t)2048) <= 0)
						{
							fprintf(stderr, "Error writing to file! [%d]\n", errno);
							ok = 0;
						}
					}
					else
					{
						ok = 0;
					}
				}

				printf("Tried reading %d times, failed doing that. Skiping %d blocks...\n", n_retries, skip_value);

				if (lseek64(fdin, curpos + (off64_t)block_size*skip_value, SEEK_SET) == -1)
				{
					fprintf(stderr, "Problem seeking in input-file! [%d] (sector %d after current)\n", errno, skip_value);
				}
			}
		}
	}

	close(fdout);
	close(fdin);

	if (dsecfile)
		fclose(dsecfile);

	printf("Done\n");

	return 0;
}
