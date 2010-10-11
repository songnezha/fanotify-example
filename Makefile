KERNEL_SOURCE := /lib/modules/$(shell uname -r)/source/usr

CC := gcc
CFLAGS := -g -Wall -W -O0 -I$(KERNEL_SOURCE)/include -D_GNU_SOURCE -DWITH_PID -DX86_64
LDFLAGS := -g -Wall -W
#CFLAGS := -g -Wall -m32 -W -O0 -I$(KERNEL_SOURCE)/include -D_GNU_SOURCE -DWITH_PID
#LDFLAGS := -g -Wall -W -m32

all: fanotify

fanotify: fanotify.o fanotify-syscalllib.o
fanotify.c: $(KERNEL_SOURCE)/include/linux/fanotify.h fanotify-syscalllib.h
fanotify_syscalllib.c: fanotify-syscalllib.h

clean:
	rm -f fanotify fanotify.o fanotify-syscalllib.o
