#!/bin/bash

:<<!
If you are developing something on the linux environment provided by forlinx, please run this in advance. This script would set the environment (e.g., CC, LD) to the ARM cross compilition tool. NOTICE: Please using source to run this script (i.e., source environmentSetup.sh).
!

echo "Using source to run this script. This script is used to setup the environment for compiling binaries for OK6254"
. /opt/arago-2023.04/environment-setup-aarch64-oe-linux
