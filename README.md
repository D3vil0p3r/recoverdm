# recoverdm: recover damaged CD-ROMs/DVDs

This program will help you recover disks with bad sectors. You can recover files as well complete devices.
In case if finds sectors which simply cannot be recoverd, it writes an empty sector to the outputfile and continues. If you're recovering a CD or a DVD and the program cannot read the sector in "normal mode", then the program will try to read the sector in "RAW mode" (without error-checking etc.).
This toolkit also has a utility called 'mergebad': mergebad merges multiple images into one. This can be usefull when you have, for example, multiple CD's with the same data which are all damaged. In such case, you can then first use recoverdm to retrieve the data from the damaged CD's into image-files and then combine them into one image with mergebad.

## Usage

```
recoverdm -t type -i file/devicein -o fileout [-l list] [-n retries] [-s speed]

-t type	is 1 for files, 10 for floppy disks and 40 for IDE disks (try -h for a complete list)
-i file/devicein	is the device or file you want to recover
-o fileout	is the file where to write to. this file should not already exist!
-l list	this file will contain the offsets(!) of the bad blocks as well as the size of the badblock. This file can be used together with the image with the 'mergebad' utility.
-n retries	number of retries before going on with next sector, defaults to 6
-r RAW read retries	number of retries while reading in RAW mode before going on with next sector, defaults to 6
-s rotation speed	rotation speed of CD-ROM/DVD, defaults to 1
-p number of sectors	number of sectors to skip when a readproblem is detected
-h	gives the helptext
```

## Notes

For MacOS X users, add `-lcrypto` to `LDFLAGS` in Makefile.

After you have created an image of the damaged media, you might be able to write it to a new disk/cd/etc. but in some cases the directory-information (and such) is so much damaged that you're not able to mount it. In that case the [findfile](http://web.archive.org/web/20150922195525/http://www.vanheusden.com/findfile/) utility might be helpful.

This tool has been retrieved from the unavailable https://www.vanheusden.com website.
