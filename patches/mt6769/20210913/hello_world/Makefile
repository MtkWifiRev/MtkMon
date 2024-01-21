FW_PATH=$(NEXMON_ROOT)/firmwares/mt6769/20210913
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

all: WIFI_RAM_CODE_soc1_0_1a_1.bin

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

out_3_0xf0177000: init gen/patch.elf $(FW_PATH)/$(RAM_FILE) gen/nexmon.mk
	$(Q)cp $(FW_PATH)/$(RAM_FILE) $@
	$(Q)chmod u+w $@
	@printf "\033[0;31m  APPLYING PATCHES\033[0m gen/nexmon.mk\n" 
	$(Q)make -f gen/nexmon.mk >>log/patches.log


#######################################################

WIFI_RAM_CODE_soc1_0_1a_1.bin: out_3_0xf0177000
	$(Q)cp $(FW_PATH)/out_0_0x57000 .
	$(Q)chmod u+w out_0_0x57000
	$(Q)cp $(FW_PATH)/out_1_0x2010000 .
	$(Q)chmod u+w out_1_0x2010000
	$(Q)cp $(FW_PATH)/out_2_0x40100200 .
	$(Q)chmod u+w out_2_0x40100200
	$(Q)cp $(FW_PATH)/out_4_0xf01fe000 .
	$(Q)chmod u+w out_4_0xf01fe000
	$(Q)cp $(FW_PATH)/trailer.bin .
	$(Q)stat --printf="%s" out_3_0xf0177000 |xargs printf '000%x' | fold -w2 |tac |tr -d "\n" | xxd -r -p - | dd of=trailer.bin bs=1 seek=196 conv=notrunc
	$(Q)cat out_0_0x57000 out_1_0x2010000 out_2_0x40100200 out_3_0xf0177000 out_4_0xf01fe000 trailer.bin > $@

clean: FORCE
	$(Q)rm -r obj gen log out_0_0x57000 out_1_0x2010000 out_2_0x40100200 out_3_0xf0177000 out_4_0xf01fe000 trailer.bin

#prepare:
#	sudo ifconfig enp5s0 192.168.1.177/24
#	ssh habanero "rmmod ath10k_pci && rmmod ath10k_core"
#	ssh habanero "insmod /root/ath10k_core.ko debug_mask=0x432 && insmod /root/ath10k_pci.ko"

install:
	adb push WIFI_RAM_CODE_soc1_0_1a_1.bin /sdcard/
	adb shell 'su -c "cp /sdcard/WIFI_RAM_CODE_soc1_0_1a_1.bin /data/adb/WIFI_RAM_CODE_soc1_0_1a_1.bin"'
	adb shell 'su -c "chcon -v u:object_r:vendor_file:s0 /data/adb/WIFI_RAM_CODE_soc1_0_1a_1.bin"'
	adb shell 'su -c "reboot"'
FORCE:
