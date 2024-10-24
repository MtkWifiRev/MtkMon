out_3_0xf0177000: gen/patch.elf FORCE
	$(Q)$(CC)objcopy -O binary -j .text.overwrite_cmd_mem_dump $< gen/section.bin && dd if=gen/section.bin of=out_3_0xf0177000 bs=1 conv=notrunc seek=$$((0xf019b268 - 0xf0177000))
	$(Q)printf "  PATCH overwrite_cmd_mem_dump @ 0xf019b268\n"
	$(Q)$(CC)objcopy -O binary -j .text.jal_my_set_fw_op_mode $< gen/section.bin && dd if=gen/section.bin of=out_3_0xf0177000 bs=1 conv=notrunc seek=$$((0xf01cfcac - 0xf0177000))
	$(Q)printf "  PATCH jal_my_set_fw_op_mode @ 0xf01cfcac\n"
	$(Q)$(CC)objcopy -O binary -j .text.patch $< gen/section.bin && dd if=gen/section.bin of=out_4_0xf01fe000 bs=1 conv=notrunc seek=$$((0xf021b400 - 0xf01fe000))
	$(Q)printf "  TARGETREGION patch @ f021b400\n"

FORCE:
