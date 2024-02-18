/* $Id: dev.c,v 0.14 2003/01/27 18:32:26 folkert Exp folkert $
 * $Log: dev.c,v $
 * Revision 0.14  2003/01/27 18:32:26  folkert
 * added RAW-mode for CD/DVD
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifdef __linux__
	#include <linux/fd.h>
	#include <linux/cdrom.h>
	#include <linux/hdreg.h>
	#include <scsi/sg.h>
	#include <scsi/scsi_ioctl.h>
	#include <linux/cdrom.h>
#endif

#include "dev.h"


int init_device(int fd, int dev_type, int rotation_speed)
{
#ifdef __linux__
	switch(dev_type)
	{
	case DT_FLOPPY:
		{
			struct floppy_max_errors fme;
			if (ioctl(fd, FDGETMAXERRS, &fme, sizeof(fme)) == -1)
			{
				perror("FDGETMAXERRS");
				return -1;
			}
			fme.reset = 2;
			fme.recal = 3;
			if (ioctl(fd, FDSETMAXERRS, &fme, sizeof(fme)) == -1)
			{
				perror("FDSETMAXERRS");
				return -1;
			}
		}
		break;
	case DT_CDROM_IDE:
	case DT_CDROM_SCSI:
	case DT_DVD_IDE:
	case DT_DVD_SCSI:
		{
			int speed = rotation_speed;
			if (ioctl(fd, CDROM_SELECT_SPEED, speed) == -1)
			{
				perror("CDROM_SELECT_SPEED");
				return -1;
			}
		}
		break;
	}
#endif

	return 0;
}

int clear_buffers(int fd, int dev_type)
{
#ifdef __linux__
	switch(dev_type)
	{
	case DT_FLOPPY:
		if (ioctl(fd, FDFLUSH) == -1)
		{
			perror("FDFLUSH");
			return -1;
		}
		break;
	}
#endif

	return 0;
}

int reset_controller(int fd, int dev_type)
{
#ifdef __linux__
	switch(dev_type)
	{
	case DT_FLOPPY:
		if (ioctl(fd, FDRESET) == -1)
		{
			perror("FDRESET");
			return -1;
		}
		break;

	case DT_DISK_SCSI:
		if (ioctl(fd, SG_SCSI_RESET) == -1)
		{
			perror("SG_SCSI_RESET");
			return -1;
		}
		break;
	}
#endif

	return 0;
}

int reset_device(int fd, int dev_type)
{
#ifdef __linux__
	switch(dev_type)
	{
	case DT_DISK_IDE:
#if 0 /* tricky! */
		if (ioctl(fd, HDIO_DRIVE_RESET) == -1)
		{
			perror("HDIO_DRIVE_RESET");
			return -1;
		}
#endif
		break;

	case DT_DISK_SCSI:
#if 0 /* tricky! */
		if (ioctl(fd, SCSI_IOCTL_STOP_UNIT) == -1)
		{
			perror("SCSI_IOCTL_STOP_UNIT");
			return -1;
		}

		sleep(1);

		if (ioctl(fd, SCSI_IOCTL_START_UNIT) == -1)
		{
			perror("SCSI_IOCTL_START_UNIT");
			return -1;
		}
#endif
		break;
	case DT_CDROM_IDE:
	case DT_CDROM_SCSI:
	case DT_DVD_IDE:
	case DT_DVD_SCSI:
		if (ioctl(fd, CDROMRESET) == -1)
		{
			perror("CDROMRESET");
			return -1;
		}
		break;
	}
#endif

	return 0;
}

int read_raw_cd(int fd, unsigned char minute, unsigned char second, unsigned char frame, char *whereto)
{
#ifdef __linux__
	char *pframe = (char *)malloc(CD_FRAMESIZE_RAWER);
	struct cdrom_msf *msf = (struct cdrom_msf *)pframe;

	msf -> cdmsf_min0   = msf -> cdmsf_min1   = minute;
	msf -> cdmsf_sec0   = msf -> cdmsf_sec1   = second;
	msf -> cdmsf_frame0 = msf -> cdmsf_frame1 = frame;

	if (ioctl(fd, CDROMREADRAW, msf) == -1)
	{
		if (errno != EIO)
			return -1;
	}

	memcpy(whereto, &pframe[12 + 4], 2048);

	return 0;
#else
	return -1;
#endif
}
