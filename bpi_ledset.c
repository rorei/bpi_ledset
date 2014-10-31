/*		
 * 	Configure the blue led on BananaPi
 * 
 * based on mii-tool from David A. Hinds <dhinds@pcmcia.sourceforge.org>
 * 
 * http://sourceforge.net/projects/net-tools
 * 
 * which itself is based on mii-diag by Donald Becker <becker@scyld.com>
 * 
 * Copyright (C) 2014 Roman Reichel <roman.reichel@posteo.de>
 * 
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation.
 * 
 * */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <time.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/mii.h>

struct option longopts[] = {
	{"set", 1, 0, 's'},
	{0, 0, 0, 0}
};

static int skfd = -1;
static struct ifreq ifr;

static int mdio_read(int skfd, int location) {
	
	struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&ifr.ifr_data;
	mii->reg_num = location;
	if (ioctl(skfd, SIOCGMIIREG, &ifr) < 0) {
		fprintf(stderr, "SIOCGMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
		return -1;
	}
	return mii->val_out;
}

static void mdio_write(int skfd, int location, int value) {
	
	struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&ifr.ifr_data;
	mii->reg_num = location;
	mii->val_in = value;
	if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0) {
		fprintf(stderr, "SIOCGMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
	}
}

static void led_on() {
	
	int reg, val;
	
	reg = 26;
	val = mdio_read(skfd, reg);
	//printf("reg %d is: 0x%x\n", reg, val);
	mdio_write(skfd, reg, val | (0x0040));
	
	reg = 28;
	val = mdio_read(skfd, reg);
	//printf("reg %d is: 0x%x\n", reg, val);
	mdio_write(skfd, reg, val | (0x0700));
	
	printf("phy led enabled\n"); 
}

static void led_off() {
	
	int reg, val;
	
	reg = 26;
	val = mdio_read(skfd, reg);
	//printf("reg %d is: 0x%x\n", reg, val);
	mdio_write(skfd, reg, val & ~(0x0040));
	
	reg = 28;
	val = mdio_read(skfd, reg);
	//printf("reg %d is: 0x%x\n", reg, val);
	mdio_write(skfd, reg, val & ~(0x0700)); 
	
	printf("phy led disabled\n");
}
	

int main(int argc, char** argv) {
	
	int on_off, reg, val;
		
	if (argc != 3) {
		fprintf(stderr, "Usage: bpi_ledset <if> [1|0]\n");
		return -1;
	}
	
	on_off = atoi(argv[2]);
		
	
	if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	}
	
	struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&ifr.ifr_data;
	strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);
	
	if (ioctl(skfd, SIOCGMIIPHY, &ifr) < 0) {
		fprintf(stderr, "SIOCGMIIPHY on %s failed: %s\n", argv[1], strerror(errno));
		return -1;
	}
	
	mdio_write(skfd, 0x1f, 0x0000);
	
	reg = 2;
	val = mdio_read(skfd, reg);
	if ((val & 0xFFFF) != 0x001c) {
		fprintf(stderr, "unexpected PHYID1: 0x%x\n", val);
		mdio_write(skfd, 0x1f, 0x0000);
		return -1;
	}
	
	reg = 3;
	val = mdio_read(skfd, reg);
	if ((val & 0xFC00) != 0xc800) {
		fprintf(stderr, "unexpected PHYID2: 0x%x\n", val);
		mdio_write(skfd, 0x1f, 0x0000);
		return -1;
	}
	
	mdio_write(skfd, 0x1f, 0x0007);
	mdio_write(skfd, 0x1e, 0x002c);
	
	if (on_off != 0) 
		led_on();
	else
		led_off();
	
	mdio_write(skfd, 0x1f, 0x0000);
	
	close(skfd);
	return 0;
}
