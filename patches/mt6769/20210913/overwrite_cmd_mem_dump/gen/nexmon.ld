.text.dummy.wsysMemEventPktAlloc 0xf0196278 : { obj/wrapper.o (.*.wsysMemEventPktAlloc) }
.text.dummy.set_fw_op_mode 0xf0197044 : { obj/wrapper.o (.*.set_fw_op_mode) }
.text.dummy.printf 0xf019ab9c : { obj/wrapper.o (.*.printf) }
.text.dummy.print_to_logshell 0xf019ab9c : { obj/wrapper.o (.*.print_to_logshell) }
.text.dummy.some_logging_related_function 0xf019af08 : { obj/wrapper.o (.*.some_logging_related_function) }
.text.overwrite_cmd_mem_dump 0xf019b268 : { KEEP(obj/patch.o (.*.overwrite_cmd_mem_dump)) }
.text.dummy.LAB_f019b2c8 0xf019b2c8 : { obj/wrapper.o (.*.LAB_f019b2c8) }
.text.dummy.FUN_f01b2260 0xf01b2260 : { obj/wrapper.o (.*.FUN_f01b2260) }
.text.dummy.set_rf_test_mode_cmd_handler 0xf01cfc98 : { obj/wrapper.o (.*.set_rf_test_mode_cmd_handler) }
.text.jal_my_set_fw_op_mode 0xf01cfcac : { KEEP(obj/patch.o (.*.jal_my_set_fw_op_mode)) }
.text.patch : { obj/patch.o (.text .text.* .data .data.* .bss .bss.* .rodata .rodata.*) } >patch
