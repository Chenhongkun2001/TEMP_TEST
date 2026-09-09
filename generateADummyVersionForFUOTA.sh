#!/bin/bash

currentVer=1.0.1
dummyVer=1.0.2
# currentVer=2.0.1
# dummyVer=2.0.2
sed -i "s/CONFIG_APPLICATION_VERSION \"$currentVer\"/CONFIG_APPLICATION_VERSION \"$dummyVer\"/g" skf_gw_mqtt/middleware/common/global.h
