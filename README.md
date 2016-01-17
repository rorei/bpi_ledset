bpi_ledset
==========

A utility to control the network (PHY-related) LEDs on Banana Pi
(original model, "BPi-M1"). This version has been updated to incorporate
changes that user _egamorena_ submitted on LeMaker's forum.

See:
http://forum.lemaker.org/thread-1057-1-1-switch_off_the_leds_.html,
http://linux-sunxi.org/Banana_Pi#LEDs


Compilation
-----------

compile with:
`gcc -o bpi_ledset bpi_ledset.c`

Alternatively, a simple Makefile is provided; so you may also just use

`make`

instead. The Makefile also supports cross-compilation,
given that you specify a proper toolchain prefix, e.g.

`make CROSS_COMPILE=armv7a-hardfloat-linux-gnueabi`


Installation
------------

Copy the resulting `bpi_ledset` binary to a location of your choice,
preferably a directory included in your `$PATH`.

(When on the target system, you may use something like e.g.
`install bpi_ledset /usr/local/sbin`)


Usage
-----

Note: Due to permissions required, you will usually have to run `bpi_ledset` as root.
```
Usage: bpi_ledset <if> [qv] [blmha] [ylmha] [glmha]
   q    Quiet (no informational messages)
   v    Verbose (extra informational messages)

   b    Blue led configuration
   y    Yellow led configuration
   g    Green led configuration

   l    Switch on when linked at 10Mbps
   m    Switch on when linked at 100Mbps
   h    Switch on when linked at 1000Mbps
   a    Blink with activity (Tx/Rx)


Example1: show phy leds status
   bpi_ledset eth0

Example2: disable all phy leds
   bpi_ledset eth0 b y g

Example3: disable blue led, yellow led for 1000Mbps, green led for link and activity
   bpi_ledset eth0 b yh glmha

Example4: disable blue led
   bpi_ledset eth0 b
```
