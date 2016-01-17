# a minimalistic Makefile to build bpi_ledset on Linux

CC=gcc

bpi_ledset: bpi_ledset.c
	$(if $(CROSS_COMPILE),$(CROSS_COMPILE)-)$(CC) -o $@ $<
