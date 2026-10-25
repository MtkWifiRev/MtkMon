# MT7961 (== MT7921 / MT7921AU) WM firmware, linux-firmware build 20260224.
#   WIFI_RAM_CODE_MT7961_1.bin
#   sha256 b94217a951518a9c14095765f367bc5dd7698f2dc033941d6f18fc2ebd6a2ab9
#   trailer: fw_ver "____010000", build_date "20260224110949"
# All five regions are plaintext (feature_set has no FW_FEATURE_SET_ENCRYPT bit;
# region 4 is FW_FEATURE_NON_DL and is not downloaded).

NEXMON_CHIP=CHIP_VER_MT7961
NEXMON_CHIP_NUM=`$(NEXMON_ROOT)/buildtools/scripts/getdefine.sh $(NEXMON_CHIP)`
NEXMON_FW_VERSION=FW_VER_MTK_20260224
NEXMON_FW_VERSION_NUM=`$(NEXMON_ROOT)/buildtools/scripts/getdefine.sh $(NEXMON_FW_VERSION)`

NEXMON_ARCH=nds32

HEADER_SIZE=0x0

TRAILER_START=0x000c1520

1ST_SEGMENT_START=0x0
1ST_SEGMENT_SIZE=0x0

2ND_SEGMENT_START=0x0
2ND_SEGMENT_SIZE=0x0

SWAP_IMG_START=0x0
SWAP_IMG_SIZE=0x0

# Region 0 - the main WM code segment, and the one we patch.
RAM_FILE=out_0_0x915000
RAMSTART=0x915000
RAMSIZE=0x58c10

NEX_LITPOOL_START=0x0
NEX_LITPOOL_SIZE=0x0

# Patch area: the code cave between the end of the last real function
# (0x0096cfbb, which is also the end of the 512-entry EX9 instruction table at
# 0x0096c7bc) and the 335-byte data blob at 0x0096dac1.  2821 bytes, all zero
# in the image, no xrefs, inside r/w/x region 0.  See README.mt7921-IQ.md 4.27.
PATCH_FILE=out_0_0x915000
PATCHSTART=0x0096cfbc
PATCHSIZE=0x00000b04
