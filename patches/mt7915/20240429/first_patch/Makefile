FW_PATH=$(NEXMON_ROOT)/firmwares/mt7915/20240429
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
        -ffunction-sections \
	-I$(NEXMON_ROOT)/patches/include \
	-I$(FW_PATH) \
	-Isrc

all: mt7915_wm.bin

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
	$(Q)printf "out_3_0xe0022800: gen/patch.elf FORCE\n" > $@
	$(Q)sort gen/nexmon.pre | \
		gawk -v src_file=gen/patch.elf -f $(NEXMON_ROOT)/buildtools/scripts/nexmon.mk.1.awk | \
		gawk -v ramstart=$(RAMSTART) -f $(NEXMON_ROOT)/buildtools/scripts/nexmon.mk.2.awk >> $@
	$(Q)printf "\nFORCE:\n" >> $@
	$(Q)gawk '!a[$$0]++' $@ > tmp && mv tmp $@

gen/memory.ld: $(FW_PATH)/definitions.mk
	@printf "\033[0;31m  GENERATING LINKER FILE\033[0m %s\n" $@
	$(Q)printf "out3 : ORIGIN = 0x%08x, LENGTH = 0x%08x\n" $(OUT3_START) $(OUT3_SIZE) > $@
	$(Q)printf "patch : ORIGIN = 0x%08x, LENGTH = 0x%08x\n" $(PATCHSTART) $(PATCHSIZE) >> $@

gen/patch.elf: patch.ld gen/nexmon.ld gen/memory.ld $(OBJS)
	@printf "\033[0;31m  LINKING OBJECTS\033[0m => %s (details: log/linker.log, log/linker.err)\n" $@
	$(Q)$(CC)ld -T $< -o $@ >>log/linker.log 2>>log/linker.err

$(RAM_FILE): init gen/patch.elf $(FW_PATH)/$(RAM_FILE) gen/nexmon.mk
	# files contains patch code
	$(Q)cp $(FW_PATH)/out_3_0xe0022800 .
	@printf "\033[0;31m  APPLYING PATCHES\033[0m gen/nexmon.mk\n" 
	$(Q)make -f gen/nexmon.mk

#$(Q)make -f gen/nexmon.mk >>log/patches.log

#######################################################


mt7915_wm.bin.crc: $(RAM_FILE)
	dd if=/dev/zero of=out_3_0xe0022800 bs=1 count=4 seek=$$(( $$(wc -c < out_3_0xe0022800) - 4 )) conv=notrunc
	@python3 $(NEXMON_ROOT)/buildtools/crc.py out_3_0xe0022800 $@
	dd if=$@ of=out_3_0xe0022800 bs=1 count=4 seek=$$(( $$(wc -c < out_3_0xe0022800) - 4 )) conv=notrunc

mt7915_wm.bin: mt7915_wm.bin.crc
	$(Q)cp $(FW_PATH)/out_0_0x21fc00 .
	$(Q)cp $(FW_PATH)/out_1_0x31dc00 .
	$(Q)cp $(FW_PATH)/out_2_0x417400 .
	$(Q)cp $(FW_PATH)/out_4_0xe0086e00 .
	$(Q)cp $(FW_PATH)/out_5_0x23ac00 .
	$(Q)cp $(FW_PATH)/out_6_0x400000 .
	$(Q)cp $(FW_PATH)/trailer.bin .
	$(Q)cat out_0_0x21fc00 out_1_0x31dc00 out_2_0x417400 out_3_0xe0022800 out_4_0xe0086e00 out_5_0x23ac00 out_6_0x400000 trailer.bin > mt7915_mod_wm.bin
#	$(Q)cp $(FW_PATH)/out_3_0xe0022800 .

clean: FORCE
	$(Q)rm -r obj gen log out_0_0x21fc00 out_1_0x31dc00 out_2_0x417400 out_3_0xe0022800 out_4_0xe0086e00 out_5_0x23ac00 out_6_0x400000 trailer.bin mt7915_mod_wm.bin mt7915_wm.bin.crc

install:
	scp -O -oHostKeyAlgorithms=+ssh-rsa final_ram_fw.bin root@192.168.1.1:/lib/firmware/mediatek/mt7981_wm.bin
	scp -O -oHostKeyAlgorithms=+ssh-rsa final_rom_fw.bin root@192.168.1.1:/lib/firmware/mediatek/mt7981_rom_patch.bin

reload:
	ssh root@192.168.1.1 "rmmod mt7915e && rmmod mt76_connac_lib && modprobe mt76_connac_lib && modprobe mt7915e"

FORCE:
