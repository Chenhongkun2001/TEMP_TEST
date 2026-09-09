cmake_minimum_required(VERSION 3.10)

# ============================================================
# SKF Gateway build configuration
# ============================================================
# Target: Forlinx OK62xx / ARM64 (aarch64)
# Language: C
#
# Important:
#   1. Toolchain must be configured before project().
#   2. Do not overwrite CMake's built-in PROJECT_SOURCE_DIR variable.
#   3. All paths shared with skf_gw_mqtt are derived from this file's
#      location so they do not depend on the shell working directory.
# ============================================================

# ------------------------------------------------------------
# Cross toolchain - MUST be before project()
# ------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TOOLCHAIN_DIR
    "/home/forlinx/work/OK62xx-linux-sdk/external-toolchain-dir/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-g++")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-as")
set(CMAKE_LINKER       "${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-ld")
set(CMAKE_AR           "${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-ar")
set(CMAKE_OBJCOPY      "${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-objcopy")
set(CMAKE_STRIP        "${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-strip")
set(CMAKE_OBJDUMP      "${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-objdump")
set(CMAKE_NM           "${TOOLCHAIN_DIR}/bin/aarch64-none-linux-gnu-nm")

if(NOT EXISTS "${CMAKE_C_COMPILER}")
    message(FATAL_ERROR
        "AArch64 compiler not found: ${CMAKE_C_COMPILER}")
endif()

project(
    skf_gw
    VERSION 0.0.1
    LANGUAGES C
    DESCRIPTION "BLE process of SKF CNEA Gateway"
)

# ------------------------------------------------------------
# Project paths
# ------------------------------------------------------------
set(SKF_GW_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(SKF_GW_SOURCE_DIR "${SKF_GW_ROOT_DIR}/source")

get_filename_component(
    SKF_GW_MQTT_DIR
    "${SKF_GW_ROOT_DIR}/../skf_gw_mqtt"
    ABSOLUTE
)

set(SKF_GW_MIDDLEWARE_DIR "${SKF_GW_MQTT_DIR}/middleware")
set(SKF_GW_COMMON_DIR "${SKF_GW_MIDDLEWARE_DIR}/common")
set(SKF_GW_NANOPB_PROTOCOL_DIR "${SKF_GW_MIDDLEWARE_DIR}/nanopb_protocol")
set(SKF_GW_PROTOBUF_DIR "${SKF_GW_MIDDLEWARE_DIR}/protobuf")

set(SKF_GW_TARGET_ROOTFS
    "/home/forlinx/work/OK62xx-linux-sdk/OK62xx-linux-fs/rootfs")

set(SKF_GW_BLUEZ_LIB_DIR "${SKF_GW_SOURCE_DIR}/monitor/lib_bluez")

# Use the ELL package that belongs to skf_gw so the headers and library
# come from the same package/version.
set(SKF_GW_ELL_ROOT
    "${SKF_GW_SOURCE_DIR}/libell_for_OK62xx/libell")
set(SKF_GW_ELL_HEADER_SOURCE_DIR "${SKF_GW_ELL_ROOT}/inc")
set(SKF_GW_ELL_LIB_DIR "${SKF_GW_ELL_ROOT}/lib")

message(STATUS "SKF_GW_ROOT_DIR = ${SKF_GW_ROOT_DIR}")
message(STATUS "SKF_GW_SOURCE_DIR = ${SKF_GW_SOURCE_DIR}")
message(STATUS "SKF_GW_MQTT_DIR = ${SKF_GW_MQTT_DIR}")
message(STATUS "SKF_GW_NANOPB_PROTOCOL_DIR = ${SKF_GW_NANOPB_PROTOCOL_DIR}")
message(STATUS "SKF_GW_PROTOBUF_DIR = ${SKF_GW_PROTOBUF_DIR}")
message(STATUS "SKF_GW_TARGET_ROOTFS = ${SKF_GW_TARGET_ROOTFS}")

# ------------------------------------------------------------
# Basic directory checks
# ------------------------------------------------------------
foreach(REQUIRED_DIR
        "${SKF_GW_SOURCE_DIR}"
        "${SKF_GW_MQTT_DIR}"
        "${SKF_GW_COMMON_DIR}"
        "${SKF_GW_NANOPB_PROTOCOL_DIR}"
        "${SKF_GW_PROTOBUF_DIR}"
        "${SKF_GW_BLUEZ_LIB_DIR}"
        "${SKF_GW_ELL_HEADER_SOURCE_DIR}"
        "${SKF_GW_ELL_LIB_DIR}"
        "${SKF_GW_TARGET_ROOTFS}")
    if(NOT IS_DIRECTORY "${REQUIRED_DIR}")
        message(FATAL_ERROR "Required directory not found: ${REQUIRED_DIR}")
    endif()
endforeach()

# ------------------------------------------------------------
# Generate skf_gw_version.h
# ------------------------------------------------------------
if(NOT EXISTS "${SKF_GW_SOURCE_DIR}/include/skf_gw_version.h.in")
    message(FATAL_ERROR
        "Missing version template: ${SKF_GW_SOURCE_DIR}/include/skf_gw_version.h.in")
endif()

configure_file(
    "${SKF_GW_SOURCE_DIR}/include/skf_gw_version.h.in"
    "${SKF_GW_SOURCE_DIR}/include/skf_gw_version.h"
)

# ------------------------------------------------------------
# ELL headers
# ------------------------------------------------------------
# The skf_gw ELL package stores headers flat in:
#   .../libell/inc/*.h
# while the source code includes them as:
#   <ell/ell.h>, <ell/queue.h>, ...
# Create a build-only include layout build/generated/include/ell/*.h.
set(SKF_GW_ELL_GENERATED_INCLUDE_DIR
    "${CMAKE_BINARY_DIR}/generated/include")
set(SKF_GW_ELL_GENERATED_ELL_DIR
    "${SKF_GW_ELL_GENERATED_INCLUDE_DIR}/ell")

file(GLOB SKF_GW_ELL_HEADERS
    "${SKF_GW_ELL_HEADER_SOURCE_DIR}/*.h")

if(NOT SKF_GW_ELL_HEADERS)
    message(FATAL_ERROR
        "No ELL headers found in: ${SKF_GW_ELL_HEADER_SOURCE_DIR}")
endif()

file(MAKE_DIRECTORY "${SKF_GW_ELL_GENERATED_ELL_DIR}")
file(COPY ${SKF_GW_ELL_HEADERS}
     DESTINATION "${SKF_GW_ELL_GENERATED_ELL_DIR}")

# Prefer libell.so if prepareLibs.sh created it; otherwise use the
# versioned symlink already present in the project.
if(EXISTS "${SKF_GW_ELL_LIB_DIR}/libell.so")
    set(SKF_GW_LIB_ELL "${SKF_GW_ELL_LIB_DIR}/libell.so")
elseif(EXISTS "${SKF_GW_ELL_LIB_DIR}/libell.so.0")
    set(SKF_GW_LIB_ELL "${SKF_GW_ELL_LIB_DIR}/libell.so.0")
else()
    message(FATAL_ERROR
        "ELL library not found in: ${SKF_GW_ELL_LIB_DIR}")
endif()

message(STATUS "SKF_GW_LIB_ELL = ${SKF_GW_LIB_ELL}")

# ------------------------------------------------------------
# libusb header
# ------------------------------------------------------------
# prepareLibs.sh normally copies libx into source/libx.
# Also accept libx extracted directly under the skf_gw project root.
find_path(
    SKF_GW_LIBUSB_INCLUDE_DIR
    NAMES libusb-1.0/libusb.h
    PATHS
        "${SKF_GW_SOURCE_DIR}/libx/include"
        "${SKF_GW_ROOT_DIR}/libx/include"
        "${SKF_GW_TARGET_ROOTFS}/usr/include"
    NO_DEFAULT_PATH
)

if(NOT SKF_GW_LIBUSB_INCLUDE_DIR)
    message(FATAL_ERROR
        "libusb header not found. Expected libusb-1.0/libusb.h under "
        "source/libx/include, libx/include, or target rootfs/usr/include. "
        "If needed, run ./prepareLibs.sh from the skf_gw directory.")
endif()

message(STATUS "SKF_GW_LIBUSB_INCLUDE_DIR = ${SKF_GW_LIBUSB_INCLUDE_DIR}")

# ------------------------------------------------------------
# Target libraries
# ------------------------------------------------------------
# Use libraries from the actual OK62xx target rootfs. NO_DEFAULT_PATH
# prevents accidentally linking x86_64 libraries from the development PC.
find_library(
    SKF_GW_LIB_DL
    NAMES dl
    PATHS
        "${SKF_GW_TARGET_ROOTFS}/lib"
        "${SKF_GW_TARGET_ROOTFS}/usr/lib"
    NO_DEFAULT_PATH
)

find_library(
    SKF_GW_LIB_RT
    NAMES rt
    PATHS
        "${SKF_GW_TARGET_ROOTFS}/lib"
        "${SKF_GW_TARGET_ROOTFS}/usr/lib"
    NO_DEFAULT_PATH
)

find_library(
    SKF_GW_LIB_USB
    NAMES usb-1.0
    PATHS
        "${SKF_GW_TARGET_ROOTFS}/lib"
        "${SKF_GW_TARGET_ROOTFS}/usr/lib"
    NO_DEFAULT_PATH
)

find_library(
    SKF_GW_LIB_UDEV
    NAMES udev
    PATHS
        "${SKF_GW_TARGET_ROOTFS}/lib"
        "${SKF_GW_TARGET_ROOTFS}/usr/lib"
    NO_DEFAULT_PATH
)

set(SKF_GW_LIB_SHARED_MAINLOOP
    "${SKF_GW_BLUEZ_LIB_DIR}/libshared-mainloop.a")
set(SKF_GW_LIB_GLIB_2_0
    "${SKF_GW_BLUEZ_LIB_DIR}/libglib-2.0.so")
set(SKF_GW_LIB_GLIB_BLUETOOTH_INTERNAL
    "${SKF_GW_BLUEZ_LIB_DIR}/libbluetooth-internal.a")

foreach(REQUIRED_LIB_VAR
        SKF_GW_LIB_DL
        SKF_GW_LIB_RT
        SKF_GW_LIB_USB
        SKF_GW_LIB_UDEV)
    if(NOT ${REQUIRED_LIB_VAR})
        message(FATAL_ERROR
            "Required target library not found: ${REQUIRED_LIB_VAR}")
    endif()
endforeach()

foreach(REQUIRED_LIB_FILE
        "${SKF_GW_LIB_ELL}"
        "${SKF_GW_LIB_SHARED_MAINLOOP}"
        "${SKF_GW_LIB_GLIB_2_0}"
        "${SKF_GW_LIB_GLIB_BLUETOOTH_INTERNAL}")
    if(NOT EXISTS "${REQUIRED_LIB_FILE}")
        message(FATAL_ERROR
            "Required bundled library not found: ${REQUIRED_LIB_FILE}")
    endif()
endforeach()

message(STATUS "SKF_GW_LIB_DL = ${SKF_GW_LIB_DL}")
message(STATUS "SKF_GW_LIB_RT = ${SKF_GW_LIB_RT}")
message(STATUS "SKF_GW_LIB_SHARED_MAINLOOP = ${SKF_GW_LIB_SHARED_MAINLOOP}")
message(STATUS "SKF_GW_LIB_GLIB_2_0 = ${SKF_GW_LIB_GLIB_2_0}")
message(STATUS "SKF_GW_LIB_GLIB_BLUETOOTH_INTERNAL = ${SKF_GW_LIB_GLIB_BLUETOOTH_INTERNAL}")
message(STATUS "SKF_GW_LIB_USB = ${SKF_GW_LIB_USB}")
message(STATUS "SKF_GW_LIB_UDEV = ${SKF_GW_LIB_UDEV}")

# pthread is used directly by gateway source files.
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# ------------------------------------------------------------
# C language / build flags
# ------------------------------------------------------------
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Keep the project's existing O0 behavior for both Debug and Release.
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O0")

message(STATUS "CMAKE_C_FLAGS_DEBUG = ${CMAKE_C_FLAGS_DEBUG}")
message(STATUS "CMAKE_C_FLAGS_RELEASE = ${CMAKE_C_FLAGS_RELEASE}")

# ------------------------------------------------------------
# Source files
# ------------------------------------------------------------
set(SKF_GW_SOURCES
    ${SKF_GW_SOURCE_DIR}/src/common/util_dbg.c
    ${SKF_GW_SOURCE_DIR}/src/general/froto/app_froto_new.c
    ${SKF_GW_SOURCE_DIR}/src/general/froto/app_froto_common.c
    ${SKF_GW_SOURCE_DIR}/src/general/froto/decoder/app_froto_data_upload_decoder.c
    ${SKF_GW_SOURCE_DIR}/src/general/froto/decoder/app_froto_config_decoder.c
    ${SKF_GW_SOURCE_DIR}/src/general/froto/decoder/app_froto_fuota_decoder.c
    ${SKF_GW_SOURCE_DIR}/src/general/froto/encoder/app_froto_command_encoder.c
    ${SKF_GW_SOURCE_DIR}/src/general/froto/encoder/app_froto_config_encoder.c
    ${SKF_GW_SOURCE_DIR}/src/general/froto/encoder/app_froto_data_selection_encoder.c
    ${SKF_GW_SOURCE_DIR}/src/general/froto/encoder/app_froto_fuota_encoder.c
    ${SKF_GW_SOURCE_DIR}/src/general/sensorInterface/sensor_interface_insight_predict.c
    ${SKF_GW_SOURCE_DIR}/src/general/sensorInterface/sensor_interface_insight_predict_pro.c
    ${SKF_GW_SOURCE_DIR}/src/general/serviceInterface/mobile_server_interface.c
    ${SKF_GW_SOURCE_DIR}/src/sys_def.c
    ${SKF_GW_SOURCE_DIR}/src/bt_common.c
    ${SKF_GW_SOURCE_DIR}/src/sys_timer.c
    ${SKF_GW_SOURCE_DIR}/src/bt_advertising.c
    ${SKF_GW_SOURCE_DIR}/src/ble/bt_advertising_new.c
    ${SKF_GW_SOURCE_DIR}/src/ble/bt_connection_new.c
    ${SKF_GW_SOURCE_DIR}/src/ble/bt_gatt_new.c
    ${SKF_GW_SOURCE_DIR}/src/bt_connection.c
    ${SKF_GW_SOURCE_DIR}/src/bt_agent.c
    ${SKF_GW_SOURCE_DIR}/src/bt_gatt.c
    ${SKF_GW_SOURCE_DIR}/src/app_local_service.c
    ${SKF_GW_SOURCE_DIR}/src/app_pro_general.c
    ${SKF_GW_SOURCE_DIR}/src/general/app_pro_general_new.c
    ${SKF_GW_SOURCE_DIR}/src/app_pro_ble.c
    ${SKF_GW_SOURCE_DIR}/src/ble/app_pro_ble_new.c
    ${SKF_GW_SOURCE_DIR}/src/main.c
    ${SKF_GW_SOURCE_DIR}/src/test_le_connect.c
    ${SKF_GW_SOURCE_DIR}/src/app_common.c
    ${SKF_GW_SOURCE_DIR}/src/util_froto.c
    ${SKF_GW_SOURCE_DIR}/src/util_froto_datawrap.c
    ${SKF_GW_SOURCE_DIR}/src/app_getConfigure.c
    ${SKF_GW_SOURCE_DIR}/src/app_waiting_queue.c
    ${SKF_GW_SOURCE_DIR}/src/app_froto.c
    ${SKF_GW_SOURCE_DIR}/src/app_cjson.c
    ${SKF_GW_SOURCE_DIR}/src/general/app_db.c
    ${SKF_GW_SOURCE_DIR}/src/app_gw_scheduler.c
    ${SKF_GW_SOURCE_DIR}/src/general/app_tim_service.c
    ${SKF_GW_SOURCE_DIR}/src/general/app_io.c
    ${SKF_GW_SOURCE_DIR}/src/ble/app_hci_evt_mgmt_new.c
    ${SKF_GW_SOURCE_DIR}/src/common/bt_hcitool.c
    ${SKF_GW_SOURCE_DIR}/src/cjson/cJSON.c
    ${SKF_GW_SOURCE_DIR}/src/cjson/cJSON_Utils.c
    ${SKF_GW_SOURCE_DIR}/src/cjson/jsonbuild.c
    ${SKF_GW_SOURCE_DIR}/src/cjson/jsonparse.c
    ${SKF_GW_SOURCE_DIR}/src/cjson/cjson_test.c

    ${SKF_GW_NANOPB_PROTOCOL_DIR}/Common.pb.c
    ${SKF_GW_NANOPB_PROTOCOL_DIR}/DeviceAppBulletGateway.pb.c
    ${SKF_GW_NANOPB_PROTOCOL_DIR}/Froto.pb.c
    ${SKF_GW_NANOPB_PROTOCOL_DIR}/SensingDataUpload.pb.c
    ${SKF_GW_NANOPB_PROTOCOL_DIR}/FirmwareUpdateOverTheAir.pb.c
    ${SKF_GW_NANOPB_PROTOCOL_DIR}/ConfigurationAndCommand.pb.c
    ${SKF_GW_NANOPB_PROTOCOL_DIR}/Debug.pb.c

    ${SKF_GW_PROTOBUF_DIR}/pb_common.c
    ${SKF_GW_PROTOBUF_DIR}/pb_decode.c
    ${SKF_GW_PROTOBUF_DIR}/pb_encode.c

    ${SKF_GW_COMMON_DIR}/auxiliar.c

    ${SKF_GW_SOURCE_DIR}/monitor/a2dp.c
    ${SKF_GW_SOURCE_DIR}/monitor/analyze.c
    ${SKF_GW_SOURCE_DIR}/monitor/att.c
    ${SKF_GW_SOURCE_DIR}/monitor/avctp.c
    ${SKF_GW_SOURCE_DIR}/monitor/avdtp.c
    ${SKF_GW_SOURCE_DIR}/monitor/bnep.c
    ${SKF_GW_SOURCE_DIR}/monitor/broadcom.c
    ${SKF_GW_SOURCE_DIR}/monitor/btmon_main.c
    ${SKF_GW_SOURCE_DIR}/monitor/control.c
    ${SKF_GW_SOURCE_DIR}/monitor/crc.c
    ${SKF_GW_SOURCE_DIR}/monitor/display.c
    ${SKF_GW_SOURCE_DIR}/monitor/ellisys.c
    ${SKF_GW_SOURCE_DIR}/monitor/hcidump.c
    ${SKF_GW_SOURCE_DIR}/monitor/hwdb.c
    ${SKF_GW_SOURCE_DIR}/monitor/intel.c
    ${SKF_GW_SOURCE_DIR}/monitor/jlink.c
    ${SKF_GW_SOURCE_DIR}/monitor/keys.c
    ${SKF_GW_SOURCE_DIR}/monitor/l2cap.c
    ${SKF_GW_SOURCE_DIR}/monitor/ll.c
    ${SKF_GW_SOURCE_DIR}/monitor/lmp.c
    ${SKF_GW_SOURCE_DIR}/monitor/msft.c
    ${SKF_GW_SOURCE_DIR}/monitor/packet.c
    ${SKF_GW_SOURCE_DIR}/monitor/rfcomm.c
    ${SKF_GW_SOURCE_DIR}/monitor/sdp.c
    ${SKF_GW_SOURCE_DIR}/monitor/vendor.c
    ${SKF_GW_SOURCE_DIR}/monitor/textfile.c
)

# Fail early with the exact missing path instead of allowing add_executable()
# to report a less useful "Cannot find source file" message later.
foreach(SOURCE_FILE IN LISTS SKF_GW_SOURCES)
    if(NOT EXISTS "${SOURCE_FILE}")
        message(FATAL_ERROR "Required source file not found: ${SOURCE_FILE}")
    endif()
endforeach()

add_executable(skf_gw ${SKF_GW_SOURCES})

target_compile_options(skf_gw PRIVATE -Wall -Wextra)

target_include_directories(
    skf_gw
    PRIVATE
        "${SKF_GW_SOURCE_DIR}/include"
        "${SKF_GW_SOURCE_DIR}/include/general"
        "${SKF_GW_SOURCE_DIR}/include/ble"
        "${SKF_GW_SOURCE_DIR}/include/common"
        "${SKF_GW_SOURCE_DIR}/include/cjson"

        "${SKF_GW_SOURCE_DIR}/monitor"
        "${SKF_GW_BLUEZ_LIB_DIR}/glib-2.0"
        "${SKF_GW_BLUEZ_LIB_DIR}/x86_64-linux-gnu/glib-2.0/include"
        "${SKF_GW_BLUEZ_LIB_DIR}/bluez"
        "${SKF_GW_BLUEZ_LIB_DIR}/bluez/lib"

        "${SKF_GW_ELL_GENERATED_INCLUDE_DIR}"

        "${SKF_GW_COMMON_DIR}"
        "${SKF_GW_PROTOBUF_DIR}"
        "${SKF_GW_NANOPB_PROTOCOL_DIR}"
        "${SKF_GW_LIBUSB_INCLUDE_DIR}"
)

# ------------------------------------------------------------
# Compile-time feature switches
# ------------------------------------------------------------
target_compile_definitions(skf_gw PRIVATE SKF_GW_INTEGRATION=1)

if(DATA_RESERVED_TIME_S_CONF)
    target_compile_definitions(
        skf_gw PRIVATE SYS_DATA_RESERVED_TIME_S=${DATA_RESERVED_TIME_S_CONF})
endif()

if(DISABLE_COLLECT_INSIGHT_T)
    target_compile_definitions(skf_gw PRIVATE COLLECT_INSIGHT_T=0)
else()
    target_compile_definitions(skf_gw PRIVATE COLLECT_INSIGHT_T=1)
endif()

if(NO_SCANNING_DURING_CONNECTION)
    target_compile_definitions(skf_gw PRIVATE SCANNING_IN_A_CONNECTION=0)
else()
    target_compile_definitions(skf_gw PRIVATE SCANNING_IN_A_CONNECTION=1)
endif()

if(ACC_WAVEFORM_REQUIRED)
    target_compile_definitions(
        skf_gw PRIVATE SENSOR_DATA_COL_RULE_REQUIRING_ACC_WAVE=1)
else()
    target_compile_definitions(
        skf_gw PRIVATE SENSOR_DATA_COL_RULE_REQUIRING_ACC_WAVE=0)
endif()

if(NRF52840NIC)
    target_compile_definitions(skf_gw PRIVATE NRF52840NIC=1)
else()
    target_compile_definitions(skf_gw PRIVATE NRF52840NIC=0)
endif()

if(SKF_GW_NEW)
    target_compile_definitions(
        skf_gw
        PRIVATE
            SKF_GW_NEW=1
            SKF_GW_CONFIG_MULTI_CONNECTION=1
            STATUS_SENSING_FEATURE_ENABLE=1
    )

    # ENABLE_MODBUS_FEATURE remains disabled, matching the original build.
    # target_compile_definitions(skf_gw PRIVATE ENABLE_MODBUS_FEATURE=1)

    if(UNIT_TEST)
        target_compile_definitions(skf_gw PRIVATE UNITTEST=${UNIT_TEST_CASE})
    endif()
else()
    target_compile_definitions(skf_gw PRIVATE SKF_GW_NEW=0)
endif()

# ------------------------------------------------------------
# Link libraries
# ------------------------------------------------------------
# Static libraries are kept before the shared libraries that satisfy their
# unresolved symbols (for example libshared-mainloop.a before glib-2.0).
target_link_libraries(
    skf_gw
    PRIVATE
        "${SKF_GW_LIB_ELL}"
        "${SKF_GW_LIB_DL}"
        "${SKF_GW_LIB_RT}"
        "${SKF_GW_LIB_SHARED_MAINLOOP}"
        "${SKF_GW_LIB_GLIB_2_0}"
        "${SKF_GW_LIB_GLIB_BLUETOOTH_INTERNAL}"
        "${SKF_GW_LIB_USB}"
        "${SKF_GW_LIB_UDEV}"
        Threads::Threads
        m
)

# libglib-2.0.so depends on libraries from the target rootfs (for example
# libpcre.so.1). rpath-link helps the cross linker resolve those target
# dependencies without using host x86_64 libraries.
set_target_properties(
    skf_gw
    PROPERTIES
        LINK_FLAGS
        "-Wl,-Map=${CMAKE_BINARY_DIR}/output.map -Wl,--rpath-link=${SKF_GW_BLUEZ_LIB_DIR} -Wl,--rpath-link=${SKF_GW_TARGET_ROOTFS}/lib -Wl,--rpath-link=${SKF_GW_TARGET_ROOTFS}/usr/lib"
)
