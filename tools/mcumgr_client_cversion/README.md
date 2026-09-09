# README

## Compilation

This is a C-based mcumgr-client (including the required static library libtinycbor.a). To generate an executable binary file mcumgr by just running build.sh.
*cmcumgr-client* and *tinycbor* are two submodules. The build.sh would apply patches to the submodules first, then compile them. The toolchain path can be modified in *tinycbor/toolchain-arm.cmake* and *cmcumgr-client/CMakeLists.txt* if necessary. 

### Note 
1. The expected cmake version should be HIGHER than v 3.10 (cmake 3.10.2 cannot generate the makefile for tinycbor). We do not adopt cmake instead of the original build framework meson to compile cmcumgr-client.
1. Submodules *cmcumgr-client* and *tinycbor* are probably empty, you can update the modules by the following instruction:
```
# Remove *cmcumgr-client* and *tinycbor* if they are empty, otherwise, go to *Update submodules* step directly
cd tools/mcumgr_client_cversion
rmdir cmcumgr-client tinycbor
cd ../..
# Init submodules
git submodule init
# Update submodules
git submodule update
# Checkout the specific version
cd tools/mcumgr_client_cversion/tinycbor
git checkout 8c72b3e
cd ../cmcumgr-client
git checkout 6bdedb2
```

## Usage

Here is a script as an instance:

```
# Stop all the SKF BLE-based applications
killall restart_ble.sh
killall skf_gw_1
killall skf_gw_2
#  Stop BLE service
/etc/init.d/bluetooth stop

# Reset the NRF52840-NIC
echo 82 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio82/direction
echo 1 > /sys/class/gpio/gpio82/value 
echo 81 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio81/direction
echo 1 > /sys/class/gpio/gpio81/value
sleep 1
echo out > /sys/class/gpio/gpio82/direction
echo 0 > /sys/class/gpio/gpio82/value 
echo out > /sys/class/gpio/gpio81/direction
echo 0 > /sys/class/gpio/gpio81/value
sleep 1
echo out > /sys/class/gpio/gpio82/direction
echo 1 > /sys/class/gpio/gpio82/value 
echo out > /sys/class/gpio/gpio81/direction
echo 1 > /sys/class/gpio/gpio81/value

# Check the existed image
sleep 1
./mcumgr -s=dev=/dev/ttyS8,baud=115200 image list

# Upload the new image (for instance, the new image is hci_uart_offset_17000h_size_e6800h_v1_0_2_with_mcuboot_0218938.signed.bin)
./mcumgr -s=dev=/dev/ttyS8,baud=115200 image analyze hci_uart_offset_17000h_size_e6800h_v1_0_2_with_mcuboot_0218938.signed.bin
./mcumgr -s=dev=/dev/ttyS8,baud=115200 image upload hci_uart_offset_17000h_size_e6800h_v1_0_2_with_mcuboot_0218938.signed.bin

# Check the image again (the new image should have been uploaded)
sleep 1
./mcumgr -s=dev=/dev/ttyS8,baud=115200 image list

# Reset the NIC
./mcumgr -s=dev=/dev/ttyS8,baud=115200 reset
```