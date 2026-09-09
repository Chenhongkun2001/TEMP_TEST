#/bin/sh

# file for LTE 4G control


echo "=====LTE power control=====" 
echo "==usage: lte_pctl.sh [init/on/off]"

echo $1

case "$1" in
	init)
	echo "going to init PIN86 for LTE control"	
	echo 86 > /sys/class/gpio/export
	echo out > /sys/class/gpio/gpio86/direction
	echo 0 > /sys/class/gpio/gpio86/value			
	;;	
	pon)	
	echo "power on LTE module"	
	echo 1 > /sys/class/gpio/gpio86/value			
	;;
	on)
	echo " try to dial and connect to internet"	
	ifconfig wwan0 up
	sleep 1	
	/usr/bin/quectel-CM &    	
	;;
	off)
	echo "power off LTE module, LTE connection down"	
	killall -s 9 quectel-CM		
	ifconfig wwan0 down	
	echo 0 > /sys/class/gpio/gpio86/value			
	;;
	*)
	echo "unknown cmd:$0 <init/on/off> "	
	exit 1	
	;;
esac




