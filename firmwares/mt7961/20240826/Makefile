all: extract

extract: FORCE
	@printf "\033[0;31m EXTRACTING FW Parts \033[0m\n"
	$(Q)$(NEXMON_ROOT)/buildtools/all_gen/all_gen WIFI_RAM_CODE_MT7961_1.bin gen4m extract
	$(Q)tail -c 388 WIFI_RAM_CODE_MT7961_1.bin > trailer.bin

FORCE:

