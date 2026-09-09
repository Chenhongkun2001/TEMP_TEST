#!/bin/bash
basename=skf_gw_mqtt
libs=/usr/lib
target=(v1 v2)
cur_path=`pwd`
findfile=restart_mqtt.sh
runsql=restart_sql.sh
runble=restart_ble.sh
runmod=restart_mod.sh
startfile=start.sh
findstr=v1
logdir=log

cd ~
if [ ! -d "$basename" ];then
mkdir $basename
fi
if [ ! -d "$basename/$logdir" ];then
mkdir $basename/$logdir
fi

if [ ! -d "$basename/$findfile" ];then
    mv $cur_path/$findfile $basename
fi
if [ ! -d "$basename/$runsql" ];then
    mv $cur_path/$runsql $basename
fi
if [ ! -d "$basename/$runmod" ];then
    mv $cur_path/$runmod $basename
fi
if [ ! -d "$basename/$runble" ];then
    mv $cur_path/$runble $basename
fi
if [ ! -d "$basename/$startfile" ];then
    mv $cur_path/$startfile $basename
fi

for element in ${target[*]}; do
    cd $basename
    if [ ! -d $element ];then
    mkdir $element
    fi
    cd ..
done

if [ `grep -c "$findstr" $basename/$findfile` -ne '0' ];then
    mv $cur_path/mqtt_op    ~/$basename/v1
    mv $cur_path/sql_op    ~/$basename/v1
    mv $cur_path/skf_gw_1    ~/$basename/v1
    mv $cur_path/skf_gw_mod    ~/$basename/v1
    # sed -i 's/v1/v2/g'  ~/$basename/$findfile
    # sed -i 's/v1/v2/g'  ~/$basename/$runsql
    # sed -i 's/v1/v2/g'  ~/$basename/$runble
else
    mv $cur_path/mqtt_op    ~/$basename/v2
    mv $cur_path/sql_op    ~/$basename/v2
    mv $cur_path/skf_gw_1    ~/$basename/v2
    mv $cur_path/skf_gw_mod    ~/$basename/v2
    # sed -i 's/v2/v1/g'  ~/$basename/$findfile
    # sed -i 's/v2/v1/g'  ~/$basename/$runsql
    # sed -i 's/v2/v1/g'  ~/$basename/$runble
fi

tar -zxf $cur_path/libs.tar.gz -C $cur_path
cp -arf  $cur_path/libs/*  $libs
# install the autorun service
sudo mv  $cur_path/skf_gw.service /etc/systemd/system
sudo systemctl daemon-reload 
sudo systemctl start skf_gw.service 
sudo systemctl enable skf_gw.service

sudo reboot

# cp this file into /etc/systemd/system
# systemctl start skf_gw.service 
# systemctl enable skf_gw.service
