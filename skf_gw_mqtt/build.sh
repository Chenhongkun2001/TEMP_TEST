#!/bin/bash

# The root workdir
ROOT_DIR=$(pwd)
export ROOT_DIR
# Target and Path config
TARGET_DIRS=(sqlite mqtt test_exec)
# modbus feature: 0 - disable; 1 - enable
MODBUS_FEATURE=1
# Toolchain config
CROSS_COMPILE=/home/forlinx/work/OK62xx-linux-sdk/external-toolchain-dir/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
export CC=${CROSS_COMPILE}gcc
export AS=${CROSS_COMPILE}as
export LD=${CROSS_COMPILE}ld
export CXX=${CROSS_COMPILE}g++
export AR=${CROSS_COMPILE}ar
export NM=${CROSS_COMPILE}nm
export STRIP=${CROSS_COMPILE}strip
export OBJCOPY=${CROSS_COMPILE}objcopy
export OBJDUMP=${CROSS_COMPILE}objdump
export AS LD CXX AR NM STRIP OBJCOPY OBJDUMP

handle_deploy(){
    cd  ${ROOT_DIR}/deployment/skf_package
    # cp -arf ${ROOT_DIR}/libs/cJSON-master/usr/local/lib/*  libs/
    # cp -arf ${ROOT_DIR}/libs/paho.mqtt.c/lib/*  libs/
    # cp -arf ${ROOT_DIR}/libs/libell/lib/*  libs/
    # tar -czf libs.tar.gz libs
    # rm -rf libs
    cp -arf ${ROOT_DIR}/libs/libs.tar.gz  ./
    cp  ${ROOT_DIR}/tools/* ./
    cd  ${ROOT_DIR}  
}


build_usage() {
    printf "Usage:
    ./build.sh all              build all target  
    ./build.sh test             build test target  
    ./build.sh sql              build sql target    
    ./build.sh mqtt             build matt target   
    ./build.sh clean            clean all files of subdir from build   
    ./build.sh sql(mqtt) bear   gen compile_commands.json which used to index project
    "
}

clean_all() {
    for element in ${TARGET_DIRS[*]}; do
        cd ${ROOT_DIR}/$element
        make clean
    done
    cd  ${ROOT_DIR}/deployment/skf_package
    rm -rf *
}
build_sql() {
    cd ${ROOT_DIR}/sqlite
    if [ x$1 == x ]; then
        make ENABLE_MODBUS_FEATURE=${MODBUS_FEATURE}
        # exit
    # fi
    elif [ $1 == "bear" ]; then
        bear make
        mv compile_commands.json ../
    fi
    handle_deploy
    cp ${ROOT_DIR}/sqlite/sql_op  ${ROOT_DIR}/deployment/skf_package
}
build_test() {
    cd ${ROOT_DIR}/test_exec
        make
        exit
}
build_all() {
    for element in ${TARGET_DIRS[*]}; do
        cd ${ROOT_DIR}/$element
        if [ $element == mqtt ]; then
            make GW_PRO=0 ENABLE_MODBUS_FEATURE=${MODBUS_FEATURE}
        else
            make ENABLE_MODBUS_FEATURE=${MODBUS_FEATURE}
        fi
    done
    handle_deploy
    cp ${ROOT_DIR}/mqtt/mqtt_op  ${ROOT_DIR}/deployment/skf_package
    cp ${ROOT_DIR}/sqlite/sql_op  ${ROOT_DIR}/deployment/skf_package
}
build_mqtt() {
    cd ${ROOT_DIR}/mqtt
    if [ x$1 == x ]; then
        make GW_PRO=0 ENABLE_MODBUS_FEATURE=${MODBUS_FEATURE}
        # exit
    # fi
    elif [ $1 == "pro" ]; then
        make GW_PRO=1 ENABLE_MODBUS_FEATURE=${MODBUS_FEATURE}
    elif [ $1 == "bear" ]; then
        bear make
        mv compile_commands.json ../
    fi
    handle_deploy   
    cp ${ROOT_DIR}/mqtt/mqtt_op  ${ROOT_DIR}/deployment/skf_package
}

case $1 in
"sql")
    build_sql $2
    ;;
"test")
    build_test 
    ;;
"mqtt")
    build_mqtt $2
    ;;
"all")
    build_all
    ;;
"clean")
    clean_all
    ;;
"help")
    build_usage
    ;;
*)
    echo "No valid input"
    ;;
esac
