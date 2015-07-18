/*		
 * 	Configure the blue led on BananaPi
 * 
 * based on Roman Reichel's code, which itself is
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


static int verbose = 1;
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

enum {
	FUNC_10MBPS,
	FUNC_100MBPS,
	FUNC_1000MBPS,
	FUNC_ACTIVITY,
	FUNC_COUNT
};

#define CFG_10MBPS      (1 << FUNC_10MBPS)
#define CFG_100MBPS     (1 << FUNC_100MBPS)
#define CFG_1000MBPS    (1 << FUNC_1000MBPS)
#define CFG_ACTIVITY    (1 << FUNC_ACTIVITY)

enum {
	LED_BLUE,
	LED_YELLOW,
	LED_GREEN,
	LED_COUNT
};

typedef struct {
	int reg26_clear;
	int reg26_set;
	int reg28_clear;
	int reg28_set;
} led_config_t;

typedef struct {
	int func_mask;
	int func_value;
} func_config_t;

static const char *led_name[] = {
	[LED_BLUE] = "blue",
	[LED_YELLOW] = "yellow",
	[LED_GREEN] = "green",
};

static const char *func_name[] = {
	[FUNC_10MBPS]   = "10Mbps",
	[FUNC_100MBPS]  = "100Mbps",
	[FUNC_1000MBPS] = "1000Mbps",
	[FUNC_ACTIVITY] = "Active (Tx/Rx)",
};

static const led_config_t led_config[LED_COUNT][FUNC_COUNT] = {
	[LED_BLUE] = {
		[FUNC_10MBPS]   = {.reg26_set = 0     , .reg28_set = 1 << 8},
		[FUNC_100MBPS]  = {.reg26_set = 0     , .reg28_set = 1 << 9},
		[FUNC_1000MBPS] = {.reg26_set = 0     , .reg28_set = 1 << 10},
		[FUNC_ACTIVITY] = {.reg26_set = 1 << 6, .reg28_set = 0},
	},
	[LED_YELLOW] = {
		[FUNC_10MBPS]   = {.reg26_set = 0     , .reg28_set = 1 << 4},
		[FUNC_100MBPS]  = {.reg26_set = 0     , .reg28_set = 1 << 5},
		[FUNC_1000MBPS] = {.reg26_set = 0     , .reg28_set = 1 << 6},
		[FUNC_ACTIVITY] = {.reg26_set = 1 << 5, .reg28_set = 0},
	},
	[LED_GREEN] = {
		[FUNC_10MBPS]   = {.reg26_set = 0     , .reg28_set = 1 << 0},
		[FUNC_100MBPS]  = {.reg26_set = 0     , .reg28_set = 1 << 1},
		[FUNC_1000MBPS] = {.reg26_set = 0     , .reg28_set = 1 << 2},
		[FUNC_ACTIVITY] = {.reg26_set = 1 << 4, .reg28_set = 0},
	},
};

static void led_config_apply(led_config_t *lcp, int set, const led_config_t *lf) {
	if (set) {
		lcp->reg26_clear |= lf->reg26_clear;
		lcp->reg26_set   |= lf->reg26_set;
		lcp->reg28_clear |= lf->reg28_clear;
		lcp->reg28_set   |= lf->reg28_set;
	} else {
		lcp->reg26_clear |= lf->reg26_set;
		lcp->reg26_set   |= lf->reg26_clear;
		lcp->reg28_clear |= lf->reg28_set;
		lcp->reg28_set   |= lf->reg28_clear;
	}
}

static void led_mask_val(led_config_t *lcp, int led, const func_config_t *cfg) {
	if (cfg->func_mask & CFG_10MBPS)   led_config_apply(lcp, !!(cfg->func_value & CFG_10MBPS), &led_config[led][FUNC_10MBPS]);
	if (cfg->func_mask & CFG_100MBPS)  led_config_apply(lcp, !!(cfg->func_value & CFG_100MBPS), &led_config[led][FUNC_100MBPS]);
	if (cfg->func_mask & CFG_1000MBPS) led_config_apply(lcp, !!(cfg->func_value & CFG_1000MBPS), &led_config[led][FUNC_1000MBPS]);
	if (cfg->func_mask & CFG_ACTIVITY) led_config_apply(lcp, !!(cfg->func_value & CFG_ACTIVITY), &led_config[led][FUNC_ACTIVITY]);
}

static void led_show(int reg26, int reg28) {
	int led, func;
	const led_config_t *lf;
	int f;
	const char *name;

	if (verbose >= 3)
		printf("[reg26 = 0x%04x, reg28 = 0x%04x]\n", reg26, reg28);
	for (led = 0; led < LED_COUNT; led++) {
		name = led_name[led];
		printf("%s:%*c", name, 8 - strlen(name), ' ');
		f = 0;
		for (func = 0; func < FUNC_COUNT; func++) {
			lf = &led_config[led][func];
			if ((reg26 & (lf->reg26_clear | lf->reg26_set)) == lf->reg26_set &&
			    (reg28 & (lf->reg28_clear | lf->reg28_set)) == lf->reg28_set) {
				printf("%s%s", f ? " | " : "", func_name[func]);
				f = 1;
			}
		}
		printf(f ? "\n" : "disabled\n");
	}
}

static void led_set(const func_config_t *blue, const func_config_t *yellow, const func_config_t *green) {
	led_config_t lc;
	int reg26_old, reg28_old;
	int reg26_new, reg28_new;

	/* Parse configuration request */
	lc.reg26_clear = 0;
	lc.reg26_set = 0;
	lc.reg28_clear = 0;
	lc.reg28_set = 0;
	led_mask_val(&lc, LED_BLUE, blue);
	led_mask_val(&lc, LED_YELLOW, yellow);
	led_mask_val(&lc, LED_GREEN, green);

	/* Configure the phy */
	mdio_write(skfd, 0x1f, 0x0007);
	mdio_write(skfd, 0x1e, 0x002c);

	reg26_old = mdio_read(skfd, 26);
	reg26_new = (reg26_old & ~lc.reg26_clear) | lc.reg26_set;
	if (reg26_new != reg26_old)
		mdio_write(skfd, 26, reg26_new);

	reg28_old = mdio_read(skfd, 28);
	reg28_new = (reg28_old & ~lc.reg28_clear) | lc.reg28_set;
	if (reg28_new != reg28_old)
		mdio_write(skfd, 28, reg28_new);

	mdio_write(skfd, 0x1f, 0x0000);

	/* Show results */
	if (reg26_new != reg26_old || reg28_new != reg28_old) {
		/* Configuration has been changed */
		if (verbose >= 2) {
			printf("Old phy led configuration:\n");
			led_show(reg26_old, reg28_old);
			printf("\n");
		}
		if (verbose >= 1)
			printf("Phy led configuration changed\n");
		if (verbose >= 2) {
			printf("\nNew phy led configuration:\n");
			led_show(reg26_new, reg28_new);
		}
	} else {
		/* No configuration change */
		if (lc.reg26_clear || lc.reg26_set || lc.reg28_clear || lc.reg28_set) {
			/* Request matches current config */
			if (verbose >= 1)
				printf("Phy led configuration already set\n");
			if (verbose >= 2) {
				printf("\nPhy led configuration:\n");
				led_show(reg26_new, reg28_new);
			}
		} else {
			/* No request: show current config */
			if (verbose >= 1) {
				printf("Phy led configuration:\n");
				led_show(reg26_new, reg28_new);
			}
		}
	}
}

#if 0
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
#endif

int main(int argc, char** argv) {
	func_config_t blue, yellow, green, *ledp;
	int val;
	int argn;
	const char *p;

	if (argc < 2) {
		fprintf(stderr, "Usage: bpi_ledset <if> [qv] [blmha] [ylmha] [glmha]\n"
			"   q    Quiet (no informational messages)\n"
			"   v    Verbose (extra informational messages)\n"
			"\n"
			"   b    Blue led configuration\n"
			"   y    Yellow led configuration\n"
			"   g    Green led configuration\n"
			"\n"
			"   l    Switch on when linked at 10Mbps\n"
			"   m    Switch on when linked at 100Mbps\n"
			"   h    Switch on when linked at 1000Mbps\n"
			"   a    Blink with activity (Tx/Rx)\n"
			"\n"
			"\n"
			"Example1: show phy leds status\n"
			"   bpi_ledset eth0\n"
			"\n"
			"Example2: disable all phy leds\n"
			"   bpi_ledset eth0 b y g\n"
			"\n"
			"Example3: disable blue led, yellow led for 1000Mbps, green led for link and activity\n"
			"   bpi_ledset eth0 b yh glmha\n"
			"\n"
			"Example4: disable blue led\n"
			"   bpi_ledset eth0 b\n"
			);
		return 1;
	}

	blue.func_mask = blue.func_value = 0;
	yellow.func_mask = yellow.func_value = 0;
	green.func_mask = green.func_value = 0;
	for (argn =2; argn < argc; argn++) {
		ledp = NULL;
		for (p = argv[argn]; *p; p++)
			switch (*p) {
				case 'q': verbose = 0; break;
				case 'v': verbose = verbose < 3 ? verbose + 1 : 3; break;
				case 'b': ledp = &blue; ledp->func_mask = ~0; ledp->func_value = 0; break;
				case 'y': ledp = &yellow; ledp->func_mask = ~0; ledp->func_value = 0; break;
				case 'g': ledp = &green; ledp->func_mask = ~0; ledp->func_value = 0; break;
				case 'l': if (ledp) {ledp->func_mask |= CFG_10MBPS; ledp->func_value |= CFG_10MBPS;} break;
				case 'm': if (ledp) {ledp->func_mask |= CFG_100MBPS; ledp->func_value |= CFG_100MBPS;} break;
				case 'h': if (ledp) {ledp->func_mask |= CFG_1000MBPS; ledp->func_value |= CFG_1000MBPS;} break;
				case 'a': if (ledp) {ledp->func_mask |= CFG_ACTIVITY; ledp->func_value |= CFG_ACTIVITY;} break;
			}
	}

	if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);

	if (ioctl(skfd, SIOCGMIIPHY, &ifr) < 0) {
		fprintf(stderr, "SIOCGMIIPHY on %s failed: %s\n", argv[1], strerror(errno));
		close(skfd);
		return 1;
	}

	mdio_write(skfd, 0x1f, 0x0000);

	val = mdio_read(skfd, 2);
	if ((val & 0xFFFF) != 0x001c) {
		close(skfd);
		fprintf(stderr, "unexpected PHYID1: 0x%x\n", val);
		return 1;
	}

	val = mdio_read(skfd, 3);
	if ((val & 0xFC00) != 0xc800) {
		close(skfd);
		fprintf(stderr, "unexpected PHYID2: 0x%x\n", val);
		return 1;
	}

	led_set(&blue, &yellow, &green);

	close(skfd);
	return 0;
}
