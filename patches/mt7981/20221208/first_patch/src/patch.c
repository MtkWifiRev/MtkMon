#pragma NEXMON targetregion "patch"

#include <firmware_version.h>
#include <patcher.h>
#include <wrapper.h>
//#include <helper.h>

int
my_patch(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6) {
    char str[] = "MTKMON MTKMON";
    a1 = str;
    return wsysDbgOutput(a1, a2, a3, a4, a5, a6);
}

__attribute__((at(0xe0049c72, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xe003b000")))
JalPatch(my_patch, my_patch);

/*
__attribute__((at(0xf00a460e, "", CHIP_VER_MTK, FW_VER_MTK, "out_10_0xf0096000")))
__attribute__((naked))
void overrite_jral2(void) {
// target: 0xe0096000
    asm(
        "sethi $r15,#0xe0096\n"
        "ori $r15, $r15, 0x0\n"
    );
}
*/

/*
__attribute__((at(0xe00aa96c, "", CHIP_VER_MTK, FW_VER_MTK, "out_4_0xe009c400")))
StringPatch(str1, "----- g_rRegisterXXXXX0 = %llu g_rRegisterTimer1 = %llu g_rTWTSPLDL = %llu  g_rTWTSPLUL = %llu  -----\n");
__attribute__((at(0xe00aabbc, "", CHIP_VER_MTK, FW_VER_MTK, "out_4_0xe009c400")))
StringPatch(str2, "----- g_rRegisterYYYYY0 = %llu  g_rRegisterTimer1 = %llu   g_rTWTSPLDL = %llu  g_rTWTSPLUL = %llu  -----\n");
*/
