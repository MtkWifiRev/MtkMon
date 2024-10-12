#pragma NEXMON targetregion "patch"

#include <firmware_version.h>
#include <patcher.h>
#include <wrapper.h>
//#include <helper.h>

void my_patch(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6) {
    unsigned char log_enable_flag = *(char *) 0xf021faf0;
    unsigned char some_log_flag = *(char *) 0xf021faef;

    //print_to_logshell("MTKMON :: Set Wifi Pwr:Max=%d,Min=%d.\n", a2, a3, a4, a5, a6);
    printf("MTKMON :: log_enable_flag: 0x%02hhx, some_log_flag: 0x%02hhx.\n", log_enable_flag, some_log_flag, a4, a5, a6);
}


// Overwrite jump to print of "Set Wifi Pwr:Max="
__attribute__((at(0xf01add80, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xf0177000")))
JalPatch(my_patch, my_patch);

void my_set_fw_op_mode(void *a1) {
    unsigned char *some_log_flag = (unsigned char *) 0xf021faef;
    *some_log_flag = 1;
    //some_logging_related_function(0x24, 4);
    
    unsigned int gp_value;
    //gp_value = __builtin_nds32_mfusr(0);
    asm("mov55 %0, $gp" : "=r" (gp_value));
    
    //printf("MTKMON :: set_fw_op_mode()\n");
    printf("MTKMON :: set_fw_op_mode(), gp_value: 0x%x\n", gp_value);
    set_fw_op_mode(a1);
}


__attribute__((at(0xf01cfcac, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xf0177000")))
JalPatch(my_set_fw_op_mode, my_set_fw_op_mode);
