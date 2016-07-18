/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Android Boot image extraction tool                                        */
/*                                                                           */
/* Copyright (C) 2012,2016 Chris Simmonds                                    */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU          */
/* General Public License for more details.                                  */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* bootimg.h is copied from AOSP:
   Jelly Bean/4.2:
    system/core/mkbootimg/bootimg.h
   Earlier releases:
    bootable/bootloader/legacy/include/boot/bootimg.h
*/
#include "bootimg.h"

extern int optind;

static void usage(char *prog)
{
	printf("\nAndroid boot image extraction tool v1.1\n"
	       "Copyright (C)2012,2016 Chris Simmonds\n\n"
	       "Usage %s <-i> [boot or recovery image file]\n"
	       "    -i   information only: do not extract images\n", prog);
	exit(1);
}

int main(int argc, char **argv)
{
	int opt;
	int info_only = 0;
	int f;
	int fk;
	int fr;
	int n;
	char *buf;
	unsigned int flash_page_size;
	struct boot_img_hdr hdr;

	if (argc == 1)
		usage(argv[0]);

	while ((opt = getopt(argc, argv, "hi")) != -1) {
		switch (opt) {
		case 'i':
			info_only = 1;
			break;
		case 'h':
			usage(argv[0]);
		default:
			exit(1);
		}
	}
	printf("argc %d optind %d\n", argc, optind);

	if (optind == argc)
		usage(argv[0]);

	f = open(argv[optind], O_RDONLY);
	if (f == -1) {
		printf("Failed to open %s (%s)\n", argv[optind], strerror(errno));
		return 1;
	}

	n = read(f, &hdr, sizeof(hdr));
	if (n < sizeof(hdr)) {
		printf("read failed\n");
		return 1;
	}

	if (strncmp((const char *)hdr.magic, BOOT_MAGIC, BOOT_MAGIC_SIZE) != 0) {
		printf("Boot magic missing or corrupt: not a valid boot image\n");
		return 1;
	}

	printf("Boot header\n"
	       "  flash page size\t%d\n"
	       "  kernel size\t\t0x%x\n"
	       "  kernel load addr\t0x%x\n"
	       "  ramdisk size\t\t0x%x\n"
	       "  ramdisk load addr\t0x%x\n"
	       "  second size\t\t0x%x\n"
	       "  second load addr\t0x%x\n"
	       "  tags addr\t\t0x%x\n"
	       "  product name\t\t'%s'\n"
	       "  kernel cmdline\t'%s'\n\n",
	       hdr.page_size,
	       hdr.kernel_size, hdr.kernel_addr,
	       hdr.ramdisk_size, hdr.ramdisk_addr,
	       hdr.second_size, hdr.second_addr,
	       hdr.tags_addr, hdr.name, hdr.cmdline);

	if (info_only)
		return 0;

	flash_page_size = hdr.page_size;
	if (hdr.kernel_size > 0) {
		/* extract kernel */
		lseek(f, flash_page_size, SEEK_SET);
		fk = open("zImage", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fk < 0) {
			printf("Failed to create kernel file\n");
			return 1;
		}
		buf = malloc(hdr.kernel_size);
		if (buf == 0) {
			printf("malloc failed\n");
			return 1;
		}
		n = read(f, buf, hdr.kernel_size);
		if (n != hdr.kernel_size) {
			printf ("Error in read\n");
			return 1;
		}
		write(fk, buf, hdr.kernel_size);
		free(buf);
		close(fk);
		printf("zImage extracted\n");
	}

	if (hdr.ramdisk_size > 0) {
		int ramdisk_offset;

		/* extract ramdisk */
		ramdisk_offset =
		    (((hdr.kernel_size + flash_page_size -
		       1) / flash_page_size) + 1) * flash_page_size;
		printf("ramdisk offset %d (0x%x)\n", ramdisk_offset,
		       ramdisk_offset);

		lseek(f, ramdisk_offset, SEEK_SET);
		fr = open("ramdisk.cpio.gz", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fr < 0) {
			printf("Failed to create ramdisk file\n");
			return 1;
		}
		buf = malloc(hdr.ramdisk_size);
		if (buf == 0) {
			printf("malloc failed\n");
			return 1;
		}
		n = read(f, buf, hdr.ramdisk_size);
		if (n != hdr.ramdisk_size) {
			printf ("Error in read\n");
			return 1;
		}
		write(fk, buf, hdr.ramdisk_size);
		free(buf);
		close(fr);
		printf("ramdisk.cpio.gz extracted\n");
	}
	if (hdr.second_size > 0) {
		int second_offset;

		/* extract second binary */
		second_offset =
		    (((hdr.kernel_size + hdr.ramdisk_size + flash_page_size -
		       1) / flash_page_size) + 1) * flash_page_size;
		printf("Second binary at offset %d (0x%x)\n", second_offset,
		       second_offset);

		lseek(f, second_offset, SEEK_SET);
		fr = open("second.dtb", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fr < 0) {
			printf("Failed to create second (dtb) file\n");
			return 1;
		}
		buf = malloc(hdr.second_size);
		if (buf == 0) {
			printf("malloc failed\n");
			return 1;
		}
		n = read(f, buf, hdr.second_size);
		if (n != hdr.second_size) {
			printf ("Error in read\n");
			return 1;
		}
		write(fk, buf, hdr.second_size);
		free(buf);
		close(fr);
		printf("second.dtb extracted\n");
	}

	return 0;
}
