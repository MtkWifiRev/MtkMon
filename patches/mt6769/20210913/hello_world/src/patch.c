#pragma NEXMON targetregion "patch"

#include <firmware_version.h>
#include <patcher.h>
#include <wrapper.h>
//#include <helper.h>

void my_patch(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6) {
    print_to_logshell(a1, a2, a3, a4, a5, a6);
}


// Overwrite jump to print of "Set Wifi Pwr:Max="
__attribute__((at(0xf01add80, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xf0177000")))
JalPatch(my_patch, my_patch);
