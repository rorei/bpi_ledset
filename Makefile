# a minimalistic Makefile to build bpi_ledset on Linux

CC=$(CROSS_COMPILE)gcc

bpi_ledset: bpi_ledset.c
	$(CC) -o $@ $<
