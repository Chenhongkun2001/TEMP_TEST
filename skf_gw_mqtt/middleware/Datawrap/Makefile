Source := $(wildcard ./*.c)
Objs := $(patsubst %.c,%.o,$(Source))
all: $(Objs)
%.o:%.c                  
	$(CC) $(CFLAGS) -c $^  
