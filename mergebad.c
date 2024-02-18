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


char verbose = 0;

typedef struct
{
	off64_t	offset;
	off64_t block_size;
} badblock;

typedef struct
{
	badblock *bb_list;
	int bb_list_size;
	int fd;	/* fd of image */
	char *filename;
	off64_t	size; /* size of image */
} image;

void version(void)
{
	printf("mergebad v" VERSION ", (c) 2004 by folkert@vanheusden.com\n\n");
}

int find_image_without_badblock(image *imgs, int n_imgs, off64_t offset)
{
	int img_index, badblock_index;

	if (verbose >= 3)
		printf("> find_image_without_badblock(%p %d %lld)\n", imgs, n_imgs, offset);

	/* for each image... */
	for(img_index=0; img_index<n_imgs; img_index++)
	{
		char ok = 1;

		/* for each badblock in an image... */
		for(badblock_index=0; badblock_index<imgs[img_index].bb_list_size; badblock_index++)
		{
			off64_t cur_offset = imgs[img_index].bb_list[badblock_index].offset;
			off64_t cur_bb_end = cur_offset + imgs[img_index].bb_list[badblock_index].block_size;

			if (verbose >= 4)
				printf("= %d/%d, %lld - %lld\n", img_index, badblock_index, cur_offset, cur_bb_end);

			/* see if the current offset is somewhere in the current bad block */
			if (offset >= cur_offset && offset < cur_bb_end)
			{
				/* yes, offset is in a badblock for image with index 'img_index' and its badblockindex
				 * is 'badblock_index', so we're not going to use this image
				 */
				ok = 0;
				break;
			}
			/* see if the offset of the current badblock is beyond the current offset: we can then
			 * stop searching for this image: since this image seems not to have any badblocks for
			 * offset 'offset', we can use this image
			 */
			else if (cur_offset > offset)
			{
				break;
			}
		}

		/* is this an image we can use? */
		if (ok)
		{
			if (verbose >= 3)
				printf("< find_image_without_badblock: %d\n", img_index);

			return img_index;
		}
	}

	if (verbose >= 3)
		printf("< find_image_without_badblock: %d\n", img_index);

	/* no image found */
	return -1;
}

void find_smallest_current_badblock(image *imgs, int n_imgs, off64_t offset, int *selected_image, int *selected_badblock)
{
	off64_t smallest_block_size = (off64_t)1 << ((sizeof(off64_t) * 8) - 2); /* better not be > 2^62 bytes! */
	int img_index, badblock_index;

	if (verbose >= 3)
		printf("> find_smallest_current_badblock(%p %d %lld %p %p)\n", imgs, n_imgs, offset, selected_image, selected_badblock);

	*selected_image = -1;
	*selected_badblock = -1;

	/* for each image... */
	for(img_index=0; img_index<n_imgs; img_index++)
	{
		/* find badblock (if any) containing a badblock containing the current offset */
		for(badblock_index=0; badblock_index<imgs[img_index].bb_list_size; badblock_index++)
		{
			off64_t cur_offset = imgs[img_index].bb_list[badblock_index].offset;
			off64_t cur_bb_end = cur_offset + imgs[img_index].bb_list[badblock_index].block_size;

			if (verbose >= 4)
				printf("= %d/%d, %lld - %lld\n", img_index, badblock_index, cur_offset, cur_bb_end);

			/* see if the current offset is somewhere in the current bad block */
			if (offset >= cur_offset && offset < cur_bb_end)
			{
				off64_t block_size_left = cur_bb_end - offset;

				/* # bytes left for current badblock relative to current offset: we
				 * might have been able to get correct bytes (from another image) that
				 * (partially) overlaps the current badblock
				 */
				if (verbose >= 4)
					printf("= # bytes left: %lld\n", block_size_left);

				/* is the end of this badblock nearer then the previous found one? (if any) */
				if (block_size_left < smallest_block_size)
				{
					if (verbose >= 4)
						printf("! block selected\n");

					*selected_image = img_index;
					*selected_badblock = badblock_index;
					smallest_block_size = block_size_left;
					break;
				}
			}
		}
	}

	if (verbose >= 3)
		printf("> find_smallest_current_badblock: %d %d (%lld)\n", *selected_image, *selected_badblock, smallest_block_size);
}

int find_badblock_after_offset(badblock *bb_list, int bb_list_size, off64_t offset)
{
	int badblock_index;

	/* search from start of list (the lowest offset) upto the end */
	for(badblock_index=0; badblock_index<bb_list_size; badblock_index++)
	{
		if (bb_list[badblock_index].offset >= offset)
			return badblock_index;
	}

	return -1;
}

int read_mapfile(char *filename, badblock **pbb, int *nbb)
{
	int n_bb = 0;
	badblock *bbs = NULL;
	FILE *fh;

	if (verbose >= 2)
		printf("Reading mapfile from %s\n", filename);

	fh = fopen(filename, "rb");
	if (!fh)
	{
		fprintf(stderr, "Problem opening %s for read! %s (%d)\n", filename, strerror(errno), errno);
		return -1;
	}
#if (_XOPEN_VERSION >= 600)
	(void)posix_fadvise(fileno(fh), 0, 0, POSIX_FADV_SEQUENTIAL); // or POSIX_FADV_NOREUSE?
#endif

	while(!feof(fh))
	{
		off64_t	offset;
		int block_size;

		fscanf(fh, "%lld %d", &offset, &block_size);

		bbs = (badblock *)myrealloc(bbs, sizeof(badblock) * (n_bb + 1), "badblocks list");
		bbs[n_bb].offset = offset;
		bbs[n_bb].block_size = block_size;

		if (verbose >= 4)
			printf("%d] %lld %lld\n", n_bb, offset, block_size);

		n_bb++;
	}

	*pbb = bbs;
	*nbb = n_bb;

	return 0;
}

char select_most_occuring_byte(unsigned char *bytes, int n_imgs)
{
	char selected_byte = 0;
	int loop;
	int counters[256];
	int count_used = -1;

	memset(counters, 0x00, sizeof(counters));

	/* count frequency for byte in each file */
	for(loop=0; loop<n_imgs; loop++)
	{
		counters[bytes[loop]]++;
	}

	/* find most used byte */
	for(loop=0; loop<256; loop++)
	{
		if (counters[loop] > count_used)
		{
			count_used = counters[loop];
			selected_byte = loop;
		}
		/* what to do with '== count_used'? ... */
	}

	/* no bytes? that is definately an error :-) */
	if (count_used == -1)
		error_exit("Internal error: no byte found!");

	return selected_byte;
}

void usage(void)
{
	fprintf(stderr, "-i <mapfile> <imagefile>\n");
	fprintf(stderr, "		With -i one selects a mapfile+imagefile to read.\n");
	fprintf(stderr, "-o <outputfile>\n");
	fprintf(stderr, "		File to write output to.\n");
	fprintf(stderr, "-l <mapfile>	In case all inputimages had a badblock on the same\n");
	fprintf(stderr, "		place, this file will list those blocks.\n");
	fprintf(stderr, "-s <size>	Limit or extend the size of the outputimage.\n");
	fprintf(stderr, "-v		Be verbose.\n");
	fprintf(stderr, "-h		This help.\n");
}

int main(int argc, char *argv[])
{
	image *imgs = NULL;
	int n_imgs = 0;
	int loop;
	int fd_out = -1;
	off64_t offset = 0, length = 0;
	char *map_file = NULL;
	FILE *fh_map_file = NULL;

	version();

	for(loop=1; loop<argc; loop++)
	{
		if (strcmp(argv[loop], "-i") == 0)
		{
			imgs = myrealloc(imgs, sizeof(image) * (n_imgs + 1), "image structure");

			if (read_mapfile(argv[++loop], &imgs[n_imgs].bb_list, &imgs[n_imgs].bb_list_size) == -1)
				return 1;

			imgs[n_imgs].fd = open64(argv[++loop], O_RDONLY);
			if (imgs[n_imgs].fd == -1)
			{
				fprintf(stderr, "Failed to open file %s: %s (%d)\n", argv[loop], strerror(errno), errno);
				return 2;
			}
#if (_XOPEN_VERSION >= 600)
	(void)posix_fadvise(imgs[n_imgs].fd, 0, 0, POSIX_FADV_SEQUENTIAL); // or POSIX_FADV_NOREUSE?
#endif

			/* remember filename */
			imgs[n_imgs].filename = argv[loop];
			/* get size of this image */
			imgs[n_imgs].size = get_filesize(argv[loop]);

			if (verbose)
				printf("Adding image: %s/%lld number of badblocks: %d\n", imgs[n_imgs].filename, imgs[n_imgs].size, imgs[n_imgs].bb_list_size);

			n_imgs++;
		}
		else if (strcmp(argv[loop], "-o") == 0)
		{
			fd_out = open64(argv[++loop], O_WRONLY | O_CREAT | O_EXCL | O_SYNC, S_IRUSR | S_IWUSR);
			if (fd_out == -1)
			{
				fprintf(stderr, "Failed to create file %s: %s (%d)\n", argv[loop], strerror(errno), errno);
				return 3;
			}
#if (_XOPEN_VERSION >= 600)
	(void)posix_fadvise(fd_out, 0, 0, POSIX_FADV_SEQUENTIAL); // or POSIX_FADV_NOREUSE?
#endif

			if (verbose)
				printf("Writing output to: %s\n", argv[loop]);
		}
		else if (strcmp(argv[loop], "-l") == 0)
		{
			map_file = argv[++loop];
		}
		else if (strcmp(argv[loop], "-s") == 0)
		{
			length = atoll(argv[++loop]);

			if (verbose)
				printf("Length set to: %d\n", length);
		}
		else if (strcmp(argv[loop], "-v") == 0)
		{
			verbose++;
		}
		else if (strcmp(argv[loop], "-h") == 0)
		{
			usage();
			return 0;
		}
		else
		{
			fprintf(stderr, "Parameter '%s' is not recognized!\n", argv[loop]);
			return 9;
		}
	}

	if (n_imgs < 2)
	{
		fprintf(stderr, "Not enough input-images selected! (at least 2 required)\n");
		return 10;
	}

	if (map_file)
	{
		fh_map_file = fopen(map_file, "w");
		if (!fh_map_file)
		{
			fprintf(stderr, "Problem creating mapfile %s: %s (%d)\n", map_file, strerror(errno), errno);
			return 11;
		}
#if (_XOPEN_VERSION >= 600)
	(void)posix_fadvise(fileno(fh_map_file), 0, 0, POSIX_FADV_SEQUENTIAL); // or POSIX_FADV_NOREUSE?
#endif
	}

	if (length == (off64_t)0)
	{
		printf("No filelength given, using length of file %s: %lld\n", imgs[0].filename, (length = get_filesize(imgs[0].filename)));
	}

	for(offset=0; offset<length;)
	{
		int img_index, badblock_index;

		if (verbose >= 2)
			printf("Current offset: %lld\n", offset);

		/* find image which has no badblock at the current offset */
		img_index = find_image_without_badblock(imgs, n_imgs, offset);

		/* image found? then copy upto the next badblock */
		if (img_index != -1)
		{
			/* find next badblock */
			int next_badblock = find_badblock_after_offset(imgs[img_index].bb_list, imgs[img_index].bb_list_size, offset);
			off64_t n_bytes_to_copy;

			/* no more badblocks in this image: copy image upto its end */
			if (next_badblock == -1)
			{
				n_bytes_to_copy = min(imgs[img_index].size, length) - offset;
			}
			else
			{
				n_bytes_to_copy = imgs[img_index].bb_list[next_badblock].offset - offset;
			}

			if (verbose)
				printf("Will copy %lld bytes from file %s\n", n_bytes_to_copy, imgs[img_index].filename);

			/* seek in inputfile to location to read from */
			myseek(imgs[img_index].fd, offset);

			/* copy the correct data! */
			if (copy_block(imgs[img_index].fd, fd_out, n_bytes_to_copy) == -1)
			{
				fprintf(stderr, "There was a problem copying %lld bytes of data from file %s to the outputfile: %s (%d)\n", n_bytes_to_copy, imgs[img_index].filename, strerror(errno), errno);
				return 16;
			}

			offset += n_bytes_to_copy;
		}
		else
		{
			off64_t cur_offset, cur_bb_end;
			off64_t n_to_guess;
			char *output_buffer;
			int output_buffer_loop;
			unsigned char *guess_bytes;

			if (verbose)
				printf("No image without badblocks for current offset (%lld) found: \"guessing\" one or more bytes.\n", offset);

			guess_bytes = (unsigned char *)mymalloc(n_imgs, "temp buffer for bytes from each image to select from");

			/* see what the smallest badblock is we're currently in */
			find_smallest_current_badblock(imgs, n_imgs, offset, &img_index, &badblock_index);

			if (img_index == -1 || badblock_index == -1)
				error_exit("Internal error: could not find the badblock \"we're in\".\n");

			/* see where this block ends */
			cur_offset = imgs[img_index].bb_list[badblock_index].offset;
			cur_bb_end = cur_offset + imgs[img_index].bb_list[badblock_index].block_size;

			/* now that we known where the smalles badblock ends, we know how many bytes to 'guess' */
			n_to_guess = cur_bb_end - offset;

			/* update mapfile */
			if (fh_map_file)
			{
				fprintf(fh_map_file, "%lld %lld\n", offset, n_to_guess);
				fflush(fh_map_file);
			}

			if (verbose)
			{
				printf("\"Guessing\" %lld bytes\n", n_to_guess);
				printf("%s is the next file to be used\n", imgs[img_index].filename);
			}
			if (n_to_guess <= 0)
				error_exit("Number of \"bytes to guess\" less then 1!\n");

			/** should be made 64bit safe: **/
			/* it is not because if the badblock is too large for the memory, things
			 * will fail here
			 */

			/* allocate block of memory: better not be larger then memory available :-) */
			output_buffer = mymalloc(n_to_guess, "guessed bytes");

			/* generate block */
			for(output_buffer_loop=0; output_buffer_loop<n_to_guess; output_buffer_loop++)
			{
				/* read from each image one byte */
				for(loop=0; loop<n_imgs; loop++)
				{
					/* seek in inputfile to location to read from */
					myseek(imgs[loop].fd, offset + (off64_t)output_buffer_loop);

					if (READ(imgs[loop].fd, (char *)&guess_bytes[loop], 1) != 1)
					{
						fprintf(stderr, "Read-error from file %s: %s (%d)\n", imgs[loop].filename, strerror(errno), errno);
						return 17;
					}
				}

				/* select the most occuring byte */
				output_buffer[output_buffer_loop] = select_most_occuring_byte(guess_bytes, n_imgs);
			}

			/* write to outputfile */
			if (phantom_write(fd_out, output_buffer, n_to_guess) != n_to_guess)
			{
				fprintf(stderr, "Failed to write to outputfile: %s (%d)\n", strerror(errno), errno);
				return 20;
			}

			free(output_buffer);

			free(guess_bytes);

			offset += n_to_guess;
		}
	}

	close(fd_out);

	if (verbose)
		printf("Finished\n");

	return 0;
}
