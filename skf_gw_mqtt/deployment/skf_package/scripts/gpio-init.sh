#!/bin/sh
set -eu
 
prepare_gpio()
{
    GPIO="$1"
    VALUE="$2"
 
    if [ ! -d "/sys/class/gpio/gpio${GPIO}" ]; then
        echo "${GPIO}" > /sys/class/gpio/export
 
        retry=0
        while [ ! -d "/sys/class/gpio/gpio${GPIO}" ]; do
            retry=$((retry + 1))
 
            if [ "${retry}" -ge 30 ]; then
                echo "GPIO${GPIO}: export timeout"
                exit 1
            fi
 
            sleep 0.1
        done
    fi
 
    echo out > "/sys/class/gpio/gpio${GPIO}/direction"
    echo "${VALUE}" > "/sys/class/gpio/gpio${GPIO}/value"
 
    chown root:skfgw \
        "/sys/class/gpio/gpio${GPIO}/value"
 
    chmod 0620 \
        "/sys/class/gpio/gpio${GPIO}/value"
}
 
# LED GPIOs confirmed from app_io.h/app_io.c
prepare_gpio 61 0
prepare_gpio 65 0
prepare_gpio 83 0
prepare_gpio 84 0
prepare_gpio 128 0
 
# GPIO86 intentionally NOT configured.
# PIN86 is currently part of main-rgmii1-pins-default.
# Do not force it to GPIO mode until BSP/hardware confirms it is safe.
 
exit 0
