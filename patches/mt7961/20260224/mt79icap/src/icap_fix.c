/* icap_fix: the two firmware-side ICAP fixes (README.mt7921-IQ.md 4.24/4.25),
 * applied to the 20260224 WM build (the addresses Ghidra gave for this build
 * were byte-verified in out_0_0x915000 before patching).
 *
 * Fix A (4.25): NOP the unmapped 0x0000d440 read-modify-write in the type-4
 *   branch of halRbistSetBbpCapNode (0x0096c770-0x0096c77d, 14 bytes). Without
 *   it, capture_node=0x40000000 (the only config known to produce real I/Q on
 *   mt7915) kills the chip. a0/a2 are both reassigned immediately after the
 *   block (sethi a0 @0x96c77e, lwi a2 @0x96c782), so the NOPs are provably
 *   safe; the swi37.sp a1 at 0x96c76e stays (a1 restored at 0x96c7a2).
 *
 * Fix B (4.24 Difference 2): fire TrigRbistCap AFTER the BBP-tap config
 *   (icapSpectrumSetParam_hal), matching mt7915 order, instead of before.
 *   RUNTIME-TOGGLEABLE via DLM flag so both orders can be A/B tested from one
 *   image:
 *     0x02017310 = 0  -> stock behaviour (trigger fires at the stock position)
 *     0x02017310 = 1  -> reorder mode (trigger fires after the hal config)
 *     0x02017314      -> internal "fire later" marker (arm vs stop)
 *   Both words validated runtime-free on the live stick (marker survives
 *   association + scan + traffic; crt re-zeroes them on every fw reload).
 *
 *   v49 correction (2026-09-13): the stock stop path ifcalls INTO the
 *   trigger jal at 0x0095c6bc. IFC-aware jal sets lp=0x0095c628, NOT
 *   0x0095c6c0, so the guard must return through lp on every path: stop
 *   continues at 0x0095c628 and releases the memory interface via its second
 *   shared call; it must not execute the arm HAL setup.
 *
 *   Both hooks replace real calls, so since v66 they are plain C functions
 *   with the callee's ABI: they return through lp like any function. Up to
 *   v65 they were hand-written assembly.
 *
 * The mem_agent hook (src/mem_agent.c) is included in this image so the
 * host-side read/write primitive stays available for live debugging.
 */
#pragma NEXMON targetregion "patch"
#include <firmware_version.h>
#include <patcher.h>

/* same macro set wrapper.c uses (this TU does not include wrapper.h) */
#ifndef WRAPPER_H
    #define VOID_DUMMY { }
    #define RETURN_DUMMY { }
    #define AT(CHIPVER, FWVER, ADDR) __attribute__((naked, at(ADDR, "dummy", CHIPVER, FWVER)))
#endif

#define ICAP_MODE   (*(volatile unsigned int *)0x02017310)
#define ICAP_MARKER (*(volatile unsigned int *)0x02017314)
#define MON_CUSTOM_RANGE (*(volatile unsigned int *)0x020173c8)

/* Called only after BBP setup, with trigger-last enabled. Use MCU-side MMIO:
 * host regidx/regval writes did not latch these RBIST registers in normal
 * mode. Leave trigger clear; icap_arm_then_trig calls the firmware's
 * trigger wrapper after this helper returns. */
__attribute__((noinline)) void monitor_custom_range_config(void) {
    volatile unsigned int *ctrl = (volatile unsigned int *)0x80021090;
    *ctrl = 0;
    *(volatile unsigned int *)0x80021098 = 0;
    *(volatile unsigned int *)0x8002109c = 0xffc;
    *(volatile unsigned int *)0x800210c0 = 0;
    *ctrl = 0x24f1;
}

/* firmware functions (dummies; placed by at() below) */
AT(CHIP_VER_MT7961, FW_VER_MTK_20260224, 0x0096b77c)
int whICapSpectrumTrigRbistCap(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
    RETURN_DUMMY

AT(CHIP_VER_MT7961, FW_VER_MTK_20260224, 0x0095c630)
int icapSpectrumSetParam_hal(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
    RETURN_DUMMY

/* Replaces the trigger call `jal 0x0096b77c` at 0x0095c6bc: entered with
 * lp = 0x0095c6c0 on the arm path (on != 0), or 0x0095c628 from the stop
 * helper's ifcall9 (on == 0). */
int icap_trig_guard(void *on, void *a2, void *a3, void *a4, void *a5, void *a6) {
    int r;

    /* stock mode: fire now, exactly like the displaced call */
    if (!ICAP_MODE)
        return whICapSpectrumTrigRbistCap(on, a2, a3, a4, a5, a6);
    /* arm: record the marker, skip the premature trigger */
    if (on) {
        ICAP_MARKER = 1;
        return (int)on;
    }
    /* stop in reorder mode: fire(0) now and clear the marker */
    r = whICapSpectrumTrigRbistCap(on, a2, a3, a4, a5, a6);
    ICAP_MARKER = 0;
    return r;
}

/* Replaces `jal 0x0095c630` at 0x0095c6c4 (arm path only). Runs the real hal
 * config, then, in reorder mode with the arm marker set, fires
 * TrigRbistCap(1), so the caller continues at 0x0095c6c8 as before. */
int icap_arm_then_trig(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6) {
    int r = icapSpectrumSetParam_hal(a1, a2, a3, a4, a5, a6);

    if (ICAP_MODE && MON_CUSTOM_RANGE)
        monitor_custom_range_config();
    if (ICAP_MODE && ICAP_MARKER)
        return whICapSpectrumTrigRbistCap((void *)1, (void *)1, 0, 0, 0, 0);
    return r;
}

/* Fix A: seven nop16 placed OVER the 14-byte 0x0000d440 RMW. Execution falls
 * through into 0x0096c77e (sethi a0,0x830ad) as before. NDS32 instructions
 * are stored big-endian, so nop16 (0x9200) is the little-endian halfword
 * 0x0092. aligned(2) matters: the assembler pads a section to its alignment,
 * and the 4-byte alignment gcc gives a byte array would make this 16 bytes,
 * overwriting the first half of that sethi. */
__attribute__((at(0x0096c770, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
const unsigned short icap_d440_nop[7] __attribute__((aligned(2))) = {
    0x0092, 0x0092, 0x0092, 0x0092, 0x0092, 0x0092, 0x0092,
};

__attribute__((at(0x0095c6bc, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(icap_trig_hook, icap_trig_guard);

__attribute__((at(0x0095c6c4, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(icap_hal_hook, icap_arm_then_trig);
