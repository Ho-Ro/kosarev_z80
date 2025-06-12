## benchmark

Emulates a CP/M program using the i8080 or z80 CPU engine.

```
usage: benchmark [-i|-z] <program.com>
         -i  i8080 emulation
         -z  z80 emulation (default)
```

Default emulation mode is Z80:

```
$ ./benchmark zexall.com
Z80all instruction exerciser
<adc,sbc> hl,<bc,de,hl,sp>....  OK
add hl,<bc,de,hl,sp>..........  OK
...
ld (<ix,iy>+1),a..............  OK
ld (<bc,de>),a................  OK
Tests complete
ticks: 46734977142
```

Command line option `-i` switches to i8080 mode:

```
$ ./benchmark cputest.com

DIAGNOSTICS II V1.2 - CPU TEST
COPYRIGHT (C) 1981 - SUPERSOFT ASSOCIATES

ABCDEFGHIJKLMNOPQRSTUVWXYZ
CPU IS Z80
BEGIN TIMING TEST
END TIMING TEST
CPU TESTS OK
ticks: 240549477

$ ./benchmark -i cputest.com

DIAGNOSTICS II V1.2 - CPU TEST
COPYRIGHT (C) 1981 - SUPERSOFT ASSOCIATES

ABCDEFGHIJKLMNOPQRSTUVWXYZ
CPU IS 8080/8085
BEGIN TIMING TEST
END TIMING TEST
CPU TESTS OK
ticks: 255691677
```

These BDOS functions are simulated, all other functions execute just a `RET` opcode.

- 0 (exit) - jump to address 0x0000
- 2 (console out) - output char
- 6 (direct console io) - output char, returns A = 0 (no char)
- 9 (print string) - output string terminated by '$'
- 12 (get CP/M version) - returns HL = 0x0022 (CP/M 2.2)
- 14 (select disk) - returns A = 0 (ok)
- 17 (search first) - returns A = 0xFF (not found)
- 18 (search next) - returns A = 0xFF (not found)
- 25 (get current disk) - returns A = 0 (A:)
- 26 (set DMA)
- 29 (get R/O disk) - returns HL = 0 (no R/O disks)
- 32 (set get user)

