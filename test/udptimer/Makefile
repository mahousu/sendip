CFLAGS= -O3

EXES= udptimer udpsimpletimer

all:	$(EXES)

udptimer:	udptimer.c
	$(CC) $(CFLAGS) -o udptimer udptimer.c -lm

udpsimpletimer:	udpsimpletimer.c
	$(CC) $(CFLAGS) -o udpsimpletimer udpsimpletimer.c -lm

clean:
	-rm -f $(EXES) *.o

install: all
	cp -p $(EXES) /usr/local/bin/
