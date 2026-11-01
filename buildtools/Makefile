# NDS32 cross toolchain: binutils 2.30 and gcc 8.0.1 from Andes Technology's
# public sources, unmodified. The release also holds those sources and the
# script that built it:
# https://github.com/MtkWifiRev/MtkMon/releases/tag/nds32le-toolchain-v3
TOOLCHAIN = nds32le-linux-glibc-v3_pre_compiled
TOOLCHAIN_URL = https://github.com/MtkWifiRev/MtkMon/releases/download/nds32le-toolchain-v3/$(TOOLCHAIN).tar.xz
TOOLCHAIN_SHA256 = 22d3724fb4ba062319f63a62fd88ac76aca0547a535b9fefcf6fcc3b459902b7

all: $(TOOLCHAIN)/bin/nds32le-linux-gcc gcc-nexmon-plugin/nexmon.so

# Replaces an older toolchain unpacked here (the previous release was a
# patched gcc).
$(TOOLCHAIN)/bin/nds32le-linux-gcc: $(TOOLCHAIN).tar.xz
	rm -rf $(TOOLCHAIN)
	tar -xf $<
	touch $@

$(TOOLCHAIN).tar.xz:
	curl -fL -o $@.part $(TOOLCHAIN_URL)
	echo "$(TOOLCHAIN_SHA256)  $@.part" | sha256sum -c -
	mv $@.part $@

# The plugin is built against the plugin headers of this toolchain.
gcc-nexmon-plugin/nexmon.so: $(TOOLCHAIN)/bin/nds32le-linux-gcc
	$(MAKE) -C gcc-nexmon-plugin clean all

clean:
	rm -rf $(TOOLCHAIN) $(TOOLCHAIN).tar.xz $(TOOLCHAIN).tar.xz.part
	$(MAKE) -C gcc-nexmon-plugin clean

.PHONY: all clean
