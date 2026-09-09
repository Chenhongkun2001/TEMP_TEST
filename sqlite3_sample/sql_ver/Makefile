TARGET=test_sqlite
CROSS_COMPILE=~/work/OK62xx-linux-sdk/external-toolchain-dir/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC 	= $(CROSS_COMPILE)gcc
AS    = $(CROSS_COMPILE)as
LD    = $(CROSS_COMPILE)ld
CXX    = $(CROSS_COMPILE)g++
AR    = $(CROSS_COMPILE)ar
NM    = $(CROSS_COMPILE)nm  
STRIP    = $(CROSS_COMPILE)strip  
OBJCOPY    = $(CROSS_COMPILE)objcopy
OBJDUMP    = $(CROSS_COMPILE)objdump 
export AS LD CXX AR NM
export STRIP OBJCOPY OBJDUMP
CFLAGS+=-c -Wall -g

# mydir=$(shell pwd)/../..
CFLAGS += -I/usr/include/ \
--sysroot=/home/forlinx/work/OK62xx-linux-sdk/OK62xx-linux-fs/rootfs
LDFLAGS+= -L/usr/lib -lsqlite3 \
--sysroot=/home/forlinx/work/OK62xx-linux-sdk/OK62xx-linux-fs/rootfs


Source := $(wildcard ./*.c)
Objs := $(patsubst %.c,%.o,$(Source))
$(TARGET): $(Objs)
	$(CC)  $^ -o $@  $(LDFLAGS)
%.o:%.c                  
	$(CC) $(CFLAGS)  $^  -o $@  
clean:
	$(RM) *.o $(TARGET) -r
