/* v62: JalPatch placement stubs for iq_live.c.
 *
 * Kept in their own translation unit, built with the project's -O3: at -Os
 * (iq_live.c, to fit the 2820-byte cave) gcc 8 appends ret5 to naked
 * functions, turning each 4-byte `jal` stub into 6 bytes. The extra ret5 then
 * overwrote the instruction following each hook - at 0x0095c7c8 the builder's
 * allocator call - and the dump builder returned before calling iq_read.
 * Rebuilds must confirm every .text.jal_* section is exactly 4 bytes.
 */
#pragma NEXMON targetregion "patch"
#include <firmware_version.h>
#include <patcher.h>

/* Replace the builder's call `jal 0x0096b9d4` at 0x0095c864. */
__attribute__((at(0x0095c864, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(iq_hook, iq_read);

/* v61s4: replace the builder's fail-block head `movpi45 a0,0x17;
 * movi55 a1,0x1` at 0x0095c7ce (2+2 bytes) with a jal to
 * iq_alloc_fail_hook (see the hook comment for the displaced-pair
 * re-execution contract). */
__attribute__((at(0x0095c7ce, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(iq_alloc_fail, iq_alloc_fail_hook);

/* v61t: replace the builder's pacing `movi a0,#0x1f4` at 0x0095c888 with a
 * jal to iq_pace_hook. The stock `jral5 a1` at 0x0095c88c stays and performs
 * the delay through the live slot value. IQ_PACE=0 (crt zeroed DLM) makes
 * the hook behave exactly like the stock 500-unit pacing. */
__attribute__((at(0x0095c888, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(iq_pace, iq_pace_hook);

/* v61r: replace the builder's alloc-size `movi a0,#0x43c` at 0x0095c7c4;
 * the stock `ex9.it 0x21` allocator call at 0x0095c7c8 stays. IQ_MAX=0
 * (boot default) keeps the stock 0x43c request. */
__attribute__((at(0x0095c7c4, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(iq_alloc, iq_alloc_hook);



/* v61s2: replace `mov55 a0,s3; jral5 a1` at 0x00946ea6 (the backpressure
 * delay call in hifSubmitBackpressureDelay) with a jal to the clamp hook.
 * This is the builder-freeze fix: a garbage/huge a2 can no longer stall the
 * MCU for more than ~1 ms. */
__attribute__((at(0x00946ea6, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(iq_delay_clamp, iq_delay_clamp_hook);
