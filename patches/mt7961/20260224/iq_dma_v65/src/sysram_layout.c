/* v64 (plan item D3): give ICAP its own SYSRAM partition at boot.
 *
 * The init state machine (0x00916758, case 2) calls the SYSRAM mode switch
 * 0x0094bb3e with the init-mode word gp+0x2fd28 (0x02032d28), which is 0 in
 * normal operation. Mode 0 picks bank profile 4: PLE 10 banks at pool
 * position 0 (0xe0200000-0xe0204fff), ICAP 0 banks, PSE 214 banks from
 * 0xe0205000 (read live 2026-10-14). The ICAP ring at 0xe0200000 therefore
 * overwrites PLE/PSE memory, and received frames show up in the I/Q.
 *
 * Mode 2 is the vendor ICAP layout: halUmacLibSetSysramLayout(PLE 1, PSE 2,
 * ICAP 0), halUmacLibSetWsIcapBankSysramNum(0x80) and bank profile 5, so
 * ICAP owns 0xe0200000-0xe023ffff (128 banks, 256 KiB) and PLE/PSE start
 * above it.
 *
 * Only the argument of that one call changes: the `lwi.gp $r0,[+0x2fd28]`
 * at 0x009167a0 (bytes 3c 0c bf 4a, verified in out_0_0x915000) becomes
 * `movi $r0, #2`. The mode word itself stays 0, so every other place that
 * reads it (0x00916898, 0x0091695c, 0x00916988, the 0x00933c84 handler)
 * keeps normal-mode behaviour. 0x0094bb3e has no other direct caller.
 */
#pragma NEXMON targetregion "patch"
#include <firmware_version.h>
#include <patcher.h>

/* Not called: the body is the replacement bytes for the 4-byte lwi.gp.
 * NDS32 instruction words are stored big-endian. */
__attribute__((at(0x009167a0, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
__attribute__((naked)) void sysram_icap_mode(void) {
    asm(".byte 0x44, 0x00, 0x00, 0x02 \n");	/* movi $r0, #2 */
}
