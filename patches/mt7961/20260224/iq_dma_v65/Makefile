FW_PATH=$(NEXMON_ROOT)/firmwares/mt7961/20260224
# Version label in the image trailer (fw_ver, 10 characters at 29 bytes before
# the end): shown by mt76 as "WM Firmware Version" and by ethtool -i. Never
# loaded into the chip. The mt7921-drv driver streams only on firmware labelled
# iq_dma_vNN (NN >= 65). Empty keeps the stock "____010000" (the driver then
# refuses to stream unless loaded with iq_fw_check=0).
FW_LABEL ?= iq_dma_v65
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
	-ffunction-sections -fdata-sections -mreduced-regs -mno-fp-as-gp \
	-I$(NEXMON_ROOT)/patches/include \
	-I$(FW_PATH) \
	-Isrc

# v62: size-optimize iq_live.c (the patch cave is 2820 bytes). Target-specific
# so that no optimize() attributes are needed: those made gcc append ret5 to
# the naked JalPatch stubs.
obj/iq_live.o: CFLAGS += -Os

all: WIFI_RAM_CODE_MT7961_1.bin

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
	$(Q)printf "%s: gen/patch.elf FORCE\n" $(RAM_FILE) > $@
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
	@printf "\033[0;31m  APPLYING PATCHES\033[0m gen/nexmon.mk\n" 
	$(Q)make -f gen/nexmon.mk >>log/patches.log


#######################################################

# Every region of this image ends with a 4-byte MediaTek "PDA CRC" computed
# over the region itself with those last 4 bytes taken as zero.  The chip
# verifies it: change ONE byte anywhere in region 0 without fixing the CRC and
# the firmware refuses to start - the driver reports "Timeout for initializing
# firmware", then "MCU is not ready for firmware download" on every retry.
# Verified against the stock image: all five regions match buildtools/crc.py.
# The CRC word for region 0 lives at 0x0096dc0c-0x0096dc0f, well clear of the
# patch area (which ends at 0x0096dac0).
$(RAM_FILE).crc: $(RAM_FILE)
	@printf "\033[0;31m  FIXING REGION 0 CRC\033[0m %s\n" $(RAM_FILE)
	$(Q)dd if=/dev/zero of=$(RAM_FILE) bs=1 count=4 \
		seek=$$(( $$(wc -c < $(RAM_FILE)) - 4 )) conv=notrunc status=none
	$(Q)python3 $(NEXMON_ROOT)/buildtools/crc.py $(RAM_FILE) $@
	$(Q)dd if=$@ of=$(RAM_FILE) bs=1 count=4 \
		seek=$$(( $$(wc -c < $(RAM_FILE)) - 4 )) conv=notrunc status=none

WIFI_RAM_CODE_MT7961_1.bin: $(RAM_FILE) $(RAM_FILE).crc
	$(Q)cp $(FW_PATH)/out_1_0x2015c00 .
	$(Q)cp $(FW_PATH)/out_2_0x404400 .
	$(Q)cp $(FW_PATH)/out_3_0xe0270000 .
	$(Q)cp $(FW_PATH)/out_4_0x0 .
	$(Q)cp $(FW_PATH)/trailer.bin .
	$(Q)stat --printf="%s" out_0_0x915000 |xargs printf '000%x' | fold -w2 |tac |tr -d "\n" | xxd -r -p - | dd of=trailer.bin bs=1 seek=172 conv=notrunc
	$(Q)python3 -c "import sys; l = sys.argv[1].encode(); assert len(l) <= 10, 'FW_LABEL: at most 10 characters'; f = open('trailer.bin', 'r+b'); f.seek(-29, 2); l and f.write(l.ljust(10, bytes(1)))" '$(FW_LABEL)'
	$(Q)cat out_0_0x915000 out_1_0x2015c00 out_2_0x404400 out_3_0xe0270000 out_4_0x0 trailer.bin > $@


clean: FORCE
	$(Q)rm -rf obj gen log out_0_0x915000.crc out_0_0x915000 out_1_0x2015c00 out_2_0x404400 out_3_0xe0270000 out_4_0x0 trailer.bin  WIFI_RAM_CODE_MT7961_1.bin

# The MT7921AU stick lives on ubuntuvm03 (ssh root, no sudo).  Override with
#   make install DEV=root@somehost
DEV?=root@ubuntuvm03
SSH?=ssh -4 -o CheckHostIP=no

install: WIFI_RAM_CODE_MT7961_1.bin
	scp -O -4 -o CheckHostIP=no WIFI_RAM_CODE_MT7961_1.bin $(DEV):/lib/firmware/mediatek/WIFI_RAM_CODE_MT7961_1.bin

reload:
	$(SSH) $(DEV) "modprobe -r mt7921u; modprobe mt7921u"

FORCE:
