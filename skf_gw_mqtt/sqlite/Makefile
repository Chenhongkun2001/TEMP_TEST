# Target and Path config
# sql target 
# SQL=insert_data
SQL=sql_op
INC_DIR :=sqlite  \
		  middleware/sh_mem    \
		  middleware/protobuf \
		  middleware/nanopb_protocol \
		  middleware/sqlite3 \
		  middleware/common \
		  middleware/ipc_mbs \
		  middleware/json    \
		  libs/cJSON-master/usr/local/include/cjson \
		  libs/libell/inc \
		  libx/include/libusb-1.0


SRC_DIR :=middleware/sqlite3 \
		  middleware/ipc_mbs \
		  middleware/common  \
		  middleware/json \
		  middleware/sh_mem
		  
LIB_DIR := libx

SRC_DIRS := $(patsubst %,$(ROOT_DIR)/%,$(SRC_DIR))
LIB_DIRS := $(patsubst %,$(ROOT_DIR)/../%,$(LIB_DIR))
INC_DIRS := $(patsubst %,$(ROOT_DIR)/%,$(INC_DIR)) /home/forlinx/work/OK62xx-linux-sdk/OK62xx-linux-fs/rootfs/usr/include
Objs := $(foreach n,$(SRC_DIRS),$(patsubst %.c,%.o,$(wildcard $(n)/*.c)))
CFLAGS:=-c -Wall -Os -g  -DENABLE_MODBUS_FEATURE=${ENABLE_MODBUS_FEATURE}
# Please comment the below line if 1.0.x is compiled
CFLAGS +=-DSTATUS_SENSING_FEATURE_ENABLE=1

LDFLAGS+= $(patsubst %, -L%, $(LIB_DIRS)) \
-L/usr/lib -lsqlite3 \
-L../tmp_libs/libs/ -lcjson \
-L../tmp_libs/libs/ -lell \
-L../tmp_libs/libx/ -lusb-1.0 -ludev \
--sysroot=/home/forlinx/work/OK62xx-linux-sdk/OK62xx-linux-fs/rootfs
export CFLAGS += $(patsubst %, -I%, $(INC_DIRS))
# LDFLAGS+= -lpaho-mqtt3c
# Main part
Source := $(wildcard ./*.c)
local_objs := $(patsubst %.c,%.o,$(Source))

.PHONY : clean all build_obj $(SQL)
all: clean $(local_objs) build_obj $(SQL)

$(SQL):$(Objs) $(local_objs)
	$(CC) $^ -o $@ $(LDFLAGS)
%.o:%.c                  
	$(CC) $(CFLAGS) -c $^ 
ifneq ($(SRC_DIRS),)
build_obj:
	@for n in $(SRC_DIRS); do make -C $$n ; done
endif
clean:
	@rm -rf $(Objs) $(local_objs) $(SQL)
