# Target and Path config
# sql target 
MQTT=mqtt_op 
INC_DIR := 	mqtt\
			middleware/protobuf \
			middleware/nanopb_protocol \
			libs/paho.mqtt.c/include \
			middleware/Datawrap \
			middleware/common \
			middleware/http   \
			middleware/ipc_mbs \
			libs/cJSON-master/usr/local/include/cjson \
			middleware/json \
			libs/libell/inc \
			libx/include/libusb-1.0

SRC_DIR := 	middleware/protobuf \
			middleware/nanopb_protocol\
			middleware/Datawrap  \
			middleware/http  \
			middleware/common \
			middleware/ipc_mbs \
			middleware/json 

	
LIB_DIR := libs/paho.mqtt.c/lib libx/

SRC_DIRS := $(patsubst %,$(ROOT_DIR)/%,$(SRC_DIR))
LIB_DIRS := $(patsubst %,$(ROOT_DIR)/../%,$(LIB_DIR))
INC_DIRS := $(patsubst %,$(ROOT_DIR)/%,$(INC_DIR))
Objs := $(foreach n,$(SRC_DIRS),$(patsubst %.c,%.o,$(wildcard $(n)/*.c))) 
CFLAGS:=-c -Wall -Os -g -DGW_PRO=${GW_PRO} -DENABLE_MODBUS_FEATURE=${ENABLE_MODBUS_FEATURE}
# Please comment the below line if 1.0.x is compiled
CFLAGS +=-D STATUS_SENSING_FEATURE_ENABLE=1

LDFLAGS+= $(patsubst %, -L%, $(LIB_DIRS)) 
export CFLAGS += $(patsubst %, -I%, $(INC_DIRS)) 
LDFLAGS+= -L../tmp_libs/libs/ -lpaho-mqtt3c \
		  -L../tmp_libs/libs/ -lcjson \
		  -L../tmp_libs/libs/ -lell \
		  -L../tmp_libs/libx/ -lusb-1.0 -ludev
# Main part
Source := $(wildcard ./*.c)
local_objs := $(patsubst %.c,%.o,$(Source))
.PHONY : clean all build_obj $(MQTT)
all: clean  $(local_objs) build_obj $(MQTT) 

$(MQTT): $(Objs) $(local_objs)
	$(CC) $^ -o $@ $(LDFLAGS)
%.o:%.c                  
	$(CC) $(CFLAGS) -c $^ 
ifneq ($(SRC_DIRS),)
build_obj:
	@for n in $(SRC_DIRS); do make -C $$n ; done
endif
clean:
	@rm -rf $(local_objs) $(Objs) $(MQTT)
