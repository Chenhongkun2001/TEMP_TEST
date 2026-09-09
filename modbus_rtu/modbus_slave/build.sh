#!/bin/bash

# The root workdir
ROOT_DIR=$(pwd)
export ROOT_DIR
# Target and Path config
TARGET_DIRS=
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

# Build all: 
# with Insight-T and with scanning-during-a-connection
build_all() {
    make clean
    #make DATA_RESERVED_TIME_S_CONF=$1
    make #DATA_RESERVED_TIME_S_CONF=$1
    cd ${ROOT_DIR}
    cp ../magicFile.json ../../skf_gw_mqtt/deployment/skf_package/
}

# Build without Insight-T: 
# without Insight-T and without scanning-during-a-connection
build_wo_T() {
    make clean
    make DISABLE_COLLECT_INSIGHT_T=1 NO_SCANNING_DURING_CONNECTION=1 DATA_RESERVED_TIME_S_CONF=$1
}

# Build with Insight-T but without scanning-during-a-connection:
# with Insight-T but without scanning-during-a-connection
build_T_wo_Scan() {
    make clean
    make NO_SCANNING_DURING_CONNECTION=1 DATA_RESERVED_TIME_S_CONF=$1
}

# Build all and require ACC waveform: 
# with Insight-T, scanning-during-a-connection, and ACC waveform uploading
build_all_acc() {
    make clean
    make DATA_RESERVED_TIME_S_CONF=$1 ACC_WAVEFORM_REQUIRED=1
}

# Build without Insight-T and require ACC waveform: 
# without Insight-T and without scanning-during-a-connection, but with ACC waveform uploading
build_wo_T_acc() {
    make clean
    make DISABLE_COLLECT_INSIGHT_T=1 NO_SCANNING_DURING_CONNECTION=1 DATA_RESERVED_TIME_S_CONF=$1 ACC_WAVEFORM_REQUIRED=1
}

# Build with Insight-T but without scanning-during-a-connection, require ACC waveform:
# with Insight-T and ACC waveform uploading, but without scanning-during-a-connection
build_T_wo_Scan_acc() {
    make clean
    make NO_SCANNING_DURING_CONNECTION=1 DATA_RESERVED_TIME_S_CONF=$1 ACC_WAVEFORM_REQUIRED=1
}

case $1 in
"all")
    build_all $2
    ;;
*)
    echo "No valid input"
    ;;
esac
