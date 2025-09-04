FW_PATH=$(NEXMON_ROOT)/firmwares/mt7981/20221208
include $(FW_PATH)/definitions.mk

LOCAL_SRCS=$(wildcard src/*.c)
COMMON_SRCS=$(wildcard $(NEXMON_ROOT)/patches/common/*.c)

OBJS=$(addprefix obj/,$(notdir $(LOCAL_SRCS:.c=.o)) $(notdir $(COMMON_SRCS:.c=.o)) $(notdir $(FW_SRCS:.c=.o)))

CFLAGS= \
	-fplugin=$(CCPLUGIN) \
	-fplugin-arg-nexmon-objfile=$@ \
	-fplugin-arg-nexmon-prefile=gen/nexmon.pre \
	-fplugin-arg-nexmon-chipver=$(NEXMON_CHIP_NUM) \
	-fplugin-arg-nexmon-fwver=$(NEXMON_FW_VERSION_NUM) \
	-fplugin-arg-nexmon-fwfile=$(RAM_FILE) \
	-DNEXMON_CHIP=$(NEXMON_CHIP) \
	-DNEXMON_FW_VERSION=$(NEXMON_FW_VERSION) \
    -Wall -Werror -O3 -nostdlib -nostartfiles -ffreestanding \
	-ffunction-sections -fdata-sections \
	-I$(NEXMON_ROOT)/patches/include \
	-I$(FW_PATH) \
	-Isrc

all: mt7981_rom_patch.bin

init: FORCE
	$(Q)make -s -f $(NEXMON_ROOT)/patches/common/header.mk
	$(Q)mkdir -p obj gen log

obj/%.o: src/%.c
	@printf "\033[0;31m  COMPILING\033[0m %s => %s (details: log/compiler.log)\n" $< $@
	$(Q)cat gen/nexmon.pre 2>>log/error.log | gawk '{ if ($$3 != "$@") print; }' > tmp && mv tmp gen/nexmon.pre
	$(Q)$(CC)gcc $(CFLAGS) -c $< -o $@ >>log/compiler.log


obj/%.o: $(NEXMON_ROOT)/patches/common/%.c
	@printf "\033[0;31m  COMPILING\033[0m %s => %s (details: log/compiler.log)\n" $< $@
	$(Q)cat gen/nexmon.pre 2>>log/error.log | gawk '{ if ($$3 != "$@") print; }' > tmp && mv tmp gen/nexmon.pre
	$(Q)$(CC)gcc $(CFLAGS) -c $< -o $@ >>log/compiler.log


gen/nexmon.ld: $(OBJS)
	@printf "\033[0;31m  GENERATING LINKER FILE\033[0m gen/nexmon.pre => %s\n" $@
	$(Q)sort gen/nexmon.pre | gawk -f $(NEXMON_ROOT)/buildtools/scripts/nexmon.ld.awk > $@

gen/nexmon.mk: $(OBJS) $(FW_PATH)/definitions.mk
	@printf "\033[0;31m  GENERATING MAKE FILE\033[0m gen/nexmon.pre => %s\n" $@
	$(Q)printf "out_3_0xf0177000: gen/patch.elf FORCE\n" > $@
	$(Q)sort gen/nexmon.pre | \
		gawk -v src_file=gen/patch.elf -f $(NEXMON_ROOT)/buildtools/scripts/nexmon.mk.1.awk | \
		gawk -v ramstart=$(RAMSTART) -f $(NEXMON_ROOT)/buildtools/scripts/nexmon.mk.2.awk >> $@
	$(Q)printf "\nFORCE:\n" >> $@
	$(Q)gawk '!a[$$0]++' $@ > tmp && mv tmp $@

gen/memory.ld: $(FW_PATH)/definitions.mk
	@printf "\033[0;31m  GENERATING LINKER FILE\033[0m %s\n" $@
	$(Q)printf "ram : ORIGIN = 0x%08x, LENGTH = 0x%08x\n" $(RAMSTART) $(RAMSIZE) >> $@
	$(Q)printf "patch : ORIGIN = 0x%08x, LENGTH = 0x%08x\n" $(PATCHSTART) $(PATCHSIZE) >> $@

gen/patch.elf: patch.ld gen/nexmon.ld gen/memory.ld $(OBJS)
	@printf "\033[0;31m  LINKING OBJECTS\033[0m => %s (details: log/linker.log, log/linker.err)\n" $@
	$(Q)$(CC)ld -T $< -o $@ >>log/linker.log 2>>log/linker.err

$(RAM_FILE): init gen/patch.elf $(FW_PATH)/$(RAM_FILE) gen/nexmon.mk
	# file contains patch code
	$(Q)cp $(FW_PATH)/$(RAM_FILE) $@
	$(Q)chmod u+w $@
	# file needed to modify jump
	$(Q)cp $(FW_PATH)/out_3_0xf0177000 .
	$(Q)chmod u+w out_3_0xf0177000
	@printf "\033[0;31m  APPLYING PATCHES\033[0m gen/nexmon.mk\n" 
	$(Q)make -f gen/nexmon.mk >>log/patches.log


#######################################################

mt7981_rom_patch.bin: mt7981_wm.bin

#mt7981_wm.bin: $(RAM_FILE)
mt7981_wm.bin: 
	$(Q)cp $(FW_PATH)/out_0_0x220a800 .
	$(Q)cp $(FW_PATH)/out_1_0x231dc00 .
	$(Q)cp $(FW_PATH)/out_2_0x414800 .
	$(Q)cp $(FW_PATH)/out_3_0xe003b000 .
	$(Q)cp ${FW_PATH}/out_4_0xe009c400 .
	$(Q)cp $(FW_PATH)/out_5_0x2229800 .
	$(Q)cp $(FW_PATH)/out_6_0x411600 .
	$(Q)cp $(FW_PATH)/out_7_0xf0025800 .
	$(Q)cp $(FW_PATH)/out_8_0xf0000000 .
	$(Q)cp $(FW_PATH)/out_9_0xf0064000 .
	$(Q)cp $(FW_PATH)/out_10_0xf0096000 .
	$(Q)cp $(FW_PATH)/trailer.bin .
	$(Q)printf '\x00\x00\x00\x00' | dd of="out_4_0xe009c400" bs=1 seek=$$(($$(stat --printf="%s" "out_4_0xe009c400") - 4)) conv=notrunc
	$(Q)cat out_0_0x220a800 out_1_0x231dc00 out_2_0x414800 out_3_0xe003b000 out_4_0xe009c400 out_5_0x2229800 out_6_0x411600 out_7_0xf0025800 out_8_0xf0000000 out_9_0xf0064000 out_10_0xf0096000 trailer.bin > mt7981_wm_mod.bin

#$(Q)cp $(FW_PATH)/out_4_0xe009c400 .
#$(Q)stat --printf="%s" out_3_0xf0177000 |xargs printf '000%x' | fold -w2 |tac |tr -d "\n" | xxd -r -p - | dd of=trailer.bin bs=1 seek=196 conv=notrunc

crc:
	$(eval MY_VAR := $(shell ssh root@192.168.1.1 "dmesg" | grep 0x2229800 | grep BEFORE| tail -n 1 | awk '{print $$(NF-2)}' | tr -d ","))
	@file_size=$$(stat -c%s out_4_0xe009c400 2>/dev/null || stat -f%z out_4_0xe009c400); \
	hex_rev=$$(printf '%08x' $(MY_VAR) | sed 's/\(..\)/\1\n/g' | tac | tr -d '\n'); \
	printf "\x$${hex_rev:0:2}\x$${hex_rev:2:2}\x$${hex_rev:4:2}\x$${hex_rev:6:2}" | \
	dd of=out_4_0xe009c400 bs=1 seek=$$(($$file_size - 4)) count=4 conv=notrunc 2>/dev/null

#$(info MY_VAR is [$(MY_VAR)])

#ssh root@192.168.1.1 "dmesg" | grep 0x2229800 | grep BEFORE| tail -n 1 | awk '{print $$(NF-2)}' | tr -d ","

clean: FORCE
	$(Q)rm -r obj gen log out_0_0x220a800 out_1_0x231dc00 out_2_0x414800 out_3_0xe003b000 out_4_0xe009c400 out_5_0x2229800 out_6_0x411600 out_7_0xf0025800 out_8_0xf0000000 out_9_0xf0064000 out_10_0xf0096000 trailer.bin mt7981_wm_mod.bin

#prepare:
#	sudo ifconfig enp5s0 192.168.1.177/24
#	ssh habanero "rmmod ath10k_pci && rmmod ath10k_core"
#	ssh habanero "insmod /root/ath10k_core.ko debug_mask=0x432 && insmod /root/ath10k_pci.ko"

install:
	scp -O -oHostKeyAlgorithms=+ssh-rsa mt7981_wm_mod.bin root@192.168.1.1:/lib/firmware/mediatek/mt7981_wm.bin

reload:
	ssh root@192.168.1.1 "rmmod mt7915e && rmmod mt76_connac_lib && modprobe mt76_connac_lib && modprobe mt7915e"

FORCE:
