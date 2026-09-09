#!/bin/bash

# Build all: 
# with Insight-T and with scanning-during-a-connection
build_all() {
    rm -r build
    rm skf_gw
    mkdir build
    cd build
#    cmake -DCMAKE_BUILD_TYPE=Debug -DDATA_RESERVED_TIME_S_CONF=$1 ..
    cmake -S .. -DCMAKE_BUILD_TYPE=Debug -DNRF52840NIC=$1
    make clean
    make
    cp skf_gw ../
    cd ..
}

# Build without Insight-T: 
# without Insight-T and without scanning-during-a-connection
build_wo_T() {
    rm -r build
    rm skf_gw
    mkdir build
    cd build
#    cmake -DCMAKE_BUILD_TYPE=Debug -DDISABLE_COLLECT_INSIGHT_T=1 -DNO_SCANNING_DURING_CONNECTION=1 -DDATA_RESERVED_TIME_S_CONF=$1 ..
    cmake -S .. -DCMAKE_BUILD_TYPE=Debug -DDISABLE_COLLECT_INSIGHT_T=1 -DNO_SCANNING_DURING_CONNECTION=1 -DNRF52840NIC=$1
    make clean
    make
    cp skf_gw ../
    cd ..
}

# Build with Insight-T but without scanning-during-a-connection:
# with Insight-T but without scanning-during-a-connection
build_T_wo_Scan() {
    rm -r build
    rm skf_gw
    mkdir build
    cd build
#    cmake -DCMAKE_BUILD_TYPE=Debug -DNO_SCANNING_DURING_CONNECTION=1 -DDATA_RESERVED_TIME_S_CONF=$1 ..
    cmake -S .. -DCMAKE_BUILD_TYPE=Debug -DNO_SCANNING_DURING_CONNECTION=1 -DNRF52840NIC=$1
    make clean
    make
    cp skf_gw ../
    cd ..
}

# Build all and require ACC waveform: 
# with Insight-T, scanning-during-a-connection, and ACC waveform uploading
build_all_acc() {
    rm -r build
    rm skf_gw
    mkdir build
    cd build
#    cmake -DCMAKE_BUILD_TYPE=Debug -DACC_WAVEFORM_REQUIRED=1 -DDATA_RESERVED_TIME_S_CONF=$1 ..
    cmake -S .. -DCMAKE_BUILD_TYPE=Debug -DACC_WAVEFORM_REQUIRED=1 -DNRF52840NIC=$1
    make clean
    make
    cp skf_gw ../
    cd ..
}

# Build without Insight-T and require ACC waveform: 
# without Insight-T and without scanning-during-a-connection, but with ACC waveform uploading
build_wo_T_acc() {
    rm -r build
    rm skf_gw
    mkdir build
    cd build
#    cmake -DCMAKE_BUILD_TYPE=Debug -DDISABLE_COLLECT_INSIGHT_T=1 -DNO_SCANNING_DURING_CONNECTION=1 -DACC_WAVEFORM_REQUIRED=1 -DDATA_RESERVED_TIME_S_CONF=$1 ..
    cmake -S .. -DCMAKE_BUILD_TYPE=Debug -DDISABLE_COLLECT_INSIGHT_T=1 -DNO_SCANNING_DURING_CONNECTION=1 -DACC_WAVEFORM_REQUIRED=1 -DNRF52840NIC=$1
    make clean
    make
    cp skf_gw ../
    cd ..
}

# Build with Insight-T but without scanning-during-a-connection, require ACC waveform:
# with Insight-T and ACC waveform uploading, but without scanning-during-a-connection
build_T_wo_Scan_acc() {
    rm -r build
    rm skf_gw
    mkdir build
    cd build
#    cmake -DCMAKE_BUILD_TYPE=Debug -DNO_SCANNING_DURING_CONNECTION=1 -DACC_WAVEFORM_REQUIRED=1 -DDATA_RESERVED_TIME_S_CONF=$1 ..
    cmake -S .. -DCMAKE_BUILD_TYPE=Debug -DNO_SCANNING_DURING_CONNECTION=1 -DACC_WAVEFORM_REQUIRED=1 -DNRF52840NIC=$1
    make clean
    make
    cp skf_gw ../
    cd ..
}

# Build the refactoring code
build_new() {
    rm -r build
    rm skf_gw
    mkdir build
    cd build
    cmake -S .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DACC_WAVEFORM_REQUIRED=1 \
    -DSKF_GW_NEW=1 \
    -DUNIT_TEST=1 \
    -DUNIT_TEST_CASE=15
    make clean
    make
    cp skf_gw ../
    cd ..
}

# Build the refactoring code-based unit test
build_new_unit_test() {
    rm -r build
    rm skf_gw
    mkdir build
    cd build
    cmake -S .. -DCMAKE_BUILD_TYPE=Debug -DACC_WAVEFORM_REQUIRED=1 -DSKF_GW_NEW=1 -DUNIT_TEST=1 -DUNIT_TEST_CASE=$1 -DNRF52840NIC=$2 -DACC_WAVEFORM_REQUIRED=$3
    make clean
    make
    cp skf_gw ../
    cd ..
}

#"./build.sh allacc 1", where 1 indicates nrf52840_NIC used; otherwise, "./build.sh allacc", means the default BLE NIC used.
case $1 in
"all")
    build_all $2
    ;;
"wot")
    build_wo_T $2
    ;;
"twoscan")
    build_T_wo_Scan $2
    ;;
"allacc")
    build_all_acc $2
    ;;
"wotacc")
    build_wo_T_acc $2
    ;;
"twoscanacc")
    build_T_wo_Scan_acc $2
    ;;
"new")
    build_new $2
    ;;
"newutest")
    build_new_unit_test $2 $3 $4
    ;;
*)
    echo "No valid input"
    ;;
esac
