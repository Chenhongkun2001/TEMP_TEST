TARGET= skf_gw_modbus_rtu

OBJS= main.o ../skf_gw/util_dbg.o

ifdef DISABLE_COLLECT_INSIGHT_T
  DEFINES+= -DCOLLECT_INSIGHT_T=0
else
  DEFINES+= -DCOLLECT_INSIGHT_T=1
endif

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

LDFLAGS+= -L./libmodbus-3.1.11/src/.libs/   -lmodbus

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
IFLAGS+= -I../skf_gw/\
		 -I./libmodbus-3.1.11/src/
				

$(TARGET):$(OBJS)
	$(CC) $^ -W -Xlinker -Map=./output.map  $(LDFLAGS) -pthread -o $@  

%.o:%.c
	$(CC) $^ $(CFLAGS) $(DEFINES) $(FLAGS) $(IFLAGS) -o $@

clean:
	$(RM) *.o $(TARGET) 
	
install:
	cp $(TARGET) ../../../OK6254-linux-fs/rootfs/usr/bin/
