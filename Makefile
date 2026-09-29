#
#
# Author: Teunis van Beelen
#
# email: teuniz@gmail.com
#
#

CC = gcc
CFLAGS = -Wall -Wextra -Wshadow -Wformat-nonliteral -Wformat-security -Wtype-limits -o2
ASMFLAGS = -S

objects = rs232.o

all: rx tx serial u center309

rx : $(objects) demo_rx.o
	$(CC) $(objects) demo_rx.o -o rx

tx : $(objects) demo_tx.o
	$(CC) $(objects) demo_tx.o -o tx
	$(CC) -S demo_tx.c -o demo_tx.asm

demo_rx.o : demo_rx.c rs232.h
	$(CC) $(CFLAGS) -c demo_rx.c -o demo_rx.o

demo_tx.o : demo_tx.c rs232.h
	$(CC) $(CFLAGS) -c demo_tx.c -o demo_tx.o

serial.o : serial.c rs232.h
	$(CC) $(CFLAGS) -c serial.c -o serial.o

u.o : u.c rs232.h
	$(CC) $(CFLAGS) -c u.c -o u.o

center309.o : center309.c
	$(CC) $(CFLAGS) -c center309.c -o center309.o

rs232.o : rs232.h rs232.c
	$(CC) $(CFLAGS) -c rs232.c -o rs232.o
	$(CC) -S rs232.c -o rs232.asm
  
clean :
	$(RM) rx tx $(objects) demo_rx.o demo_tx.o rs232.o *.asm serial.o u.o center309.o

#
#
#
#





