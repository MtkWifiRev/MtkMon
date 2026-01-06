#pragma NEXMON targetregion "patch"

#include <firmware_version.h>
#include <patcher.h>
#include <wrapper.h>
//#include <helper.h>

int
my_patch(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6) {
    char str[] = "MTKMON  %d Active";
    a1 = str;
    return wsysDbgOutput(a1, a2, a3, a4, a5, a6);
}

__attribute__((at(0xe007cab6, "", CHIP_VER_MT7915, FW_VER_MTK, "out_3_0xe0022800")))
JalPatch(my_patch, my_patch);

