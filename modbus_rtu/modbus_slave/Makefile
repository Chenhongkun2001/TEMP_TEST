TARGET= skf_gw_modbus_rtu

OBJS= main.o app_mslave.o app_mslave_ipc.o util_tool.o\
	  ../app_modbus_common.o\
	  ../../skf_gw/source/src/common/util_dbg.o\
	  ../../skf_gw_mqtt/middleware/common/auxiliar.o

ifdef DISABLE_COLLECT_INSIGHT_T
  DEFINES+= -DCOLLECT_INSIGHT_T=0
else
  DEFINES+= -DCOLLECT_INSIGHT_T=1
endif

DEFINES+= -DENABLE_MODBUS_FEATURE=1

ifdef NO_SCANNING_DURING_CONNECTION
  DEFINES+= -DSCANNING_IN_A_CONNECTION=0
else
  DEFINES+= -DSCANNING_IN_A_CONNECTION=1
endif

ifdef DATA_RESERVED_TIME_S_CONF
  DEFINES+= -DSYS_DATA_RESERVED_TIME_S=$(DATA_RESERVED_TIME_S_CONF)
endif

CFLAGS+= -c -Wall -g 

#LDFLAGS+= -Wl,--rpath-link=monitor/lib_bluez -Llibell_for_OK62xx/src/ell/ell/.libs -Lmonitor/lib_bluez -lell -ldl -lrt -lshared-mainloop -lglib-2.0\
#          -lbluetooth-internal

LDFLAGS+= -L../libmodbus -lmodbus \
         -L../../skf_gw_mqtt/tmp_libs/libs -lell \
         -L../../skf_gw_mqtt/tmp_libs/libs -lcjson -lcjson_utils

#IFLAGS+= -Ilibell_for_OK62xx/src/ell/\
#         -I../../AppProtocols/tools/nanopb/\
#         -I../../AppProtocols/nanopb_protocol/\
#         -Icjson/\
#         -I../skf_gw_mqtt/middleware/common/\
#         -I../bluez/\
#         -Imonitor/lib_bluez/x86_64-linux-gnu/glib-2.0/include\
#         -Imonitor/lib_bluez/glib-2.0/\
#         -Imonitor/\
#         -Imonitor/lib_bluez/bluez\
#         -I./\
#         -Imonitor/lib_bluez/bluez/lib
PROJECT_ROOT := $(abspath ../..)

SKF_GW_DIR := $(PROJECT_ROOT)/skf_gw
SKF_GW_MQTT_DIR := $(PROJECT_ROOT)/skf_gw_mqtt

IFLAGS += -I$(SKF_GW_DIR)/source/include
IFLAGS += -I$(SKF_GW_DIR)/source/include/cjson
IFLAGS += -I$(SKF_GW_DIR)/source/include/common

IFLAGS += -I$(SKF_GW_MQTT_DIR)/middleware/protobuf
IFLAGS += -I$(SKF_GW_MQTT_DIR)/middleware/nanopb_protocol
IFLAGS += -I$(SKF_GW_MQTT_DIR)/middleware/common
IFLAGS += -I$(SKF_GW_MQTT_DIR)/middleware/sh_mem
IFLAGS += -I$(SKF_GW_MQTT_DIR)/middleware/json

IFLAGS += -I$(SKF_GW_MQTT_DIR)/libs/libell/inc
IFLAGS += -I$(SKF_GW_MQTT_DIR)/libs/cJSON-master/usr/local/include/cjson

IFLAGS += -I../libmodbus-3.1.11
IFLAGS += -I../libmodbus-3.1.11/src
IFLAGS += -I../libmodbus
IFLAGS += -I../
IFLAGS += -I./


$(TARGET):$(OBJS)
	$(CC) $^ -W -Xlinker -Map=./output.map  $(LDFLAGS) -pthread -o $@  

%.o:%.c
	$(CC) $^ $(CFLAGS) $(DEFINES) $(FLAGS) $(IFLAGS) -o $@

clean:
	$(RM) *.o $(TARGET) ../*.o $(OBJ) 
	
install:
	cp $(TARGET) ../../../OK6254-linux-fs/rootfs/usr/bin/
