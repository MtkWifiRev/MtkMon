#pragma NEXMON targetregion "patch"

#include <firmware_version.h>
#include <patcher.h>
#include <wrapper.h>
//#include <helper.h>

int
my_patch(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6) {
    char str[] = "MTKMON MTKMON";
    a1 = str;
    return print_related(a1, a2, a3, a4, a5, a6);
}

/*
__attribute__((at(0x91893c, "", CHIP_VER_MTK, FW_VER_MTK, "out_0_0x915000")))
__attribute__((naked))
void overrite_jral1(void) {
    asm(
        "push25 r6,#0\n"
    );
}
*/

__attribute__((at(0x919844, "", CHIP_VER_MTK, FW_VER_MTK, "out_0_0x915000")))
__attribute__((naked))
void overrite_jral2(void) {
    asm(
        "sethi $r6,#0x96d\n"
    );
}




