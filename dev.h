/* $Id: dev.h,v 0.14 2003/01/27 18:32:40 folkert Exp folkert $
 * $Log: dev.h,v $
 * Revision 0.14  2003/01/27 18:32:40  folkert
 * *** empty log message ***
 *
 */

#define DT_FILE		1
#define DT_FLOPPY	10
#define DT_FLOPPY_IDE	11
#define DT_FLOPPY_SCSI	12
#define DT_CDROM_IDE	20
#define DT_CDROM_SCSI	21
#define DT_DVD_IDE	30
#define DT_DVD_SCSI	31
#define DT_DISK_IDE	40
#define DT_DISK_SCSI	41

int init_device(int fd, int dev_type, int rotation_speed);
int clear_buffers(int fd, int dev_type);
int reset_controller(int fd, int dev_type);
int reset_device(int fd, int dev_type);
int read_raw_cd(int fd, unsigned char minute, unsigned char second, unsigned char frame, char *whereto);
