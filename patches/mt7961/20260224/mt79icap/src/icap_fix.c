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
 *   0x0095c6c0. Return through the saved lp on EVERY guard path. The
 *   former fixed jump entered the arm continuation with the stop frame.
 *   Stop must continue at 0x0095c628 and release the memory interface via
 *   its second shared call; it must not execute the arm HAL setup.
 *
 * The mem_agent hook (src/mem_agent.c) is included in this image so the
 * host-side read/write primitive stays available for live debugging.
 */
#pragma NEXMON targetregion "patch"
#include <firmware_version.h>
#include <patcher.h>

/* same macro set wrapper.c uses (this TU does not include wrapper.h) */
#ifndef WRAPPER_H
    #define VOID_DUMMY { ; }
    #define RETURN_DUMMY { ; return 0; }
    #define AT(CHIPVER, FWVER, ADDR) __attribute__((at(ADDR, "dummy", CHIPVER, FWVER)))
#endif

#define ICAP_MODE   (*(volatile unsigned int *)0x02017310)
#define ICAP_MARKER (*(volatile unsigned int *)0x02017314)
#define MON_CUSTOM_RANGE (*(volatile unsigned int *)0x020173c8)

/* Called only after BBP setup, with trigger-last enabled. Use MCU-side MMIO:
 * host regidx/regval writes did not latch these RBIST registers in normal
 * mode. Leave trigger clear; the existing hook below calls the firmware's
 * trigger wrapper after this helper returns. */
__attribute__((used, noinline)) void monitor_custom_range_config(void) {
    volatile unsigned int *ctrl = (volatile unsigned int *)0x80021090;
    *ctrl = 0;
    *(volatile unsigned int *)0x80021098 = 0;
    *(volatile unsigned int *)0x8002109c = 0xffc;
    *(volatile unsigned int *)0x800210c0 = 0;
    *ctrl = 0x24f1;
    asm volatile("ret5 $lp");
}

/* firmware functions / shared tails (dummies; placed by at() below) */
AT(CHIP_VER_MT7961, FW_VER_MTK_20260224, 0x0096b77c)
void whICapSpectrumTrigRbistCap(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
    VOID_DUMMY

AT(CHIP_VER_MT7961, FW_VER_MTK_20260224, 0x0095c630)
void icapSpectrumSetParam_hal(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
    VOID_DUMMY

AT(CHIP_VER_MT7961, FW_VER_MTK_20260224, 0x0095c6c0)
void icap_resume(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
    VOID_DUMMY

/* Replaces the trigger jal. Incoming lp is 0x0095c6c0 on the arm path,
 * or 0x0095c628 after the stop helper's ifcall9. Save/restore that actual
 * return address around the wrapped call. Each ret5 is padded with nop16
 * so this correction changes only the three old 4-byte jumps, preserving
 * all patch addresses and all other executable bytes for a controlled test.
 * a1/a2/$ta are caller-saved at this call boundary. */
__attribute__((naked)) void icap_trig_guard(void) {
    asm(
        "addi  $sp, $sp, -8        \n"
        "swi   $lp, [$sp + 0]      \n"
        "sethi $ta, #0x2017        \n"
        "lwi   $ta, [$ta + 0x310]  \n"
        "bnez $ta, 1f            \n"
        /* stock mode: fire now, exactly like the displaced jal */
        "jal   whICapSpectrumTrigRbistCap \n"
        "lwi   $lp, [$sp + 0]      \n"
        "addi  $sp, $sp, 8         \n"
        "ret5  $lp                 \n"
        "nop16                     \n"
        "1:                        \n"
        "beqz38 $r0, 2f            \n"
        /* arm: record marker, skip the premature trigger */
        "sethi $ta, #0x2017        \n"
        "movi55 $r1, 1             \n"
        "swi   $r1, [$ta + 0x314]  \n"
        "lwi   $lp, [$sp + 0]      \n"
        "addi  $sp, $sp, 8         \n"
        "ret5  $lp                 \n"
        "nop16                     \n"
        "2:                        \n"
        /* stop in reorder mode: fire(0) now and clear the marker */
        "jal   whICapSpectrumTrigRbistCap \n"
        "sethi $ta, #0x2017        \n"
        "swi   $r0, [$ta + 0x314]  \n"
        "lwi   $lp, [$sp + 0]      \n"
        "addi  $sp, $sp, 8         \n"
        "ret5  $lp                 \n"
        "nop16                     \n"
    );
}

/* Replaces `jal 0x0095c630` at 0x0095c6c4 (arm path only).
 * Calls the real hal config, then - in reorder mode with the
 * arm marker set - fires TrigRbistCap(1) as a tail call, so it returns to
 * 0x0095c6c8 exactly where the displaced flow would have continued.
 * a1 is dead after the hal call (stock never reads it back either). */
__attribute__((naked)) void icap_arm_then_trig(void) {
    asm(
        "addi  $sp, $sp, -8        \n"
        "swi   $lp, [$sp + 0]      \n"
        "jal   icapSpectrumSetParam_hal \n"
        "sethi $ta, #0x2017        \n"
        "lwi   $r1, [$ta + 0x310]  \n"
        "beqz38 $r1, 4f            \n"
        "lwi   $r1, [$ta + 0x3c8]  \n"
        "beqz38 $r1, 4f            \n"
        "jal   monitor_custom_range_config \n"
        "4:                        \n"
        "sethi $ta, #0x2017        \n"
        "lwi   $r1, [$ta + 0x310]  \n"
        "beqz38 $r1, 3f            \n"
        "lwi   $r1, [$ta + 0x314]  \n"
        "beqz38 $r1, 3f            \n"
        "movi55 $r0, 1             \n"
        "lwi   $lp, [$sp + 0]      \n"
        "addi  $sp, $sp, 8         \n"
        "j     whICapSpectrumTrigRbistCap \n"
        "3:                        \n"
        "lwi   $lp, [$sp + 0]      \n"
        "addi  $sp, $sp, 8         \n"
        "ret5 $lp                  \n"
    );
}

/* Fix A: 14 bytes of nop16 placed OVER the 0x0000d440 RMW. The function is
 * deliberately ret-less: it is not called - its body IS the replacement
 * bytes, and execution falls through into 0x0096c77e (sethi a0,0x830ad) as
 * before. */
__attribute__((at(0x0096c770, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
__attribute__((naked)) void icap_d440_nop(void) {
    asm(".hword 0x0092 \n .hword 0x0092 \n .hword 0x0092 \n"
        ".hword 0x0092 \n .hword 0x0092 \n .hword 0x0092 \n .hword 0x0092 \n");
}

__attribute__((at(0x0095c6bc, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(icap_trig_hook, icap_trig_guard);

__attribute__((at(0x0095c6c4, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(icap_hal_hook, icap_arm_then_trig);
