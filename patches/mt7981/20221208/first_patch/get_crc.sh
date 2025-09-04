#!/usr/bin/env bash

FW_PATH=$NEXMON_ROOT/firmwares/mt7981/20221208

files=$(ls -l $FW_PATH | awk '{print $NF}' | grep out_)

result=$(echo "$files" | grep "$search_string")

echo $result

#BIN_NAME=$1
##Overwrite last 4 bytes of region to 0x0
#printf '\x00\x00\x00\x00' | dd of="$BIN_NAME" bs=1 seek=$(($(stat --printf="%s" "$BIN_NAME") - 4)) conv=notrunc
##Concatinate all regions to one bin
#cat out_0_0x220a800 out_1_0x231dc00 out_2_0x414800 out_3_0xe003b000 out_4_0xe009c400 out_5_0x2229800 out_6_0x411600 out_7_0xf0025800 out_8_0xf0000000 out_9_0xf0064000 out_10_0xf0096000 trailer.bin > mt7981_wm_mod.bin
##copy mod file to router
#scp -O -oHostKeyAlgorithms=+ssh-rsa mt7981_wm_mod.bin root@192.168.1.1:/lib/firmware/mediatek/mt7981_wm.bin
##reload driver
#ssh root@192.168.1.1 "rmmod mt7915e && rmmod mt76_connac_lib && modprobe mt76_connac_lib && modprobe mt7915e"
