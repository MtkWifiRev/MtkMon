/***************************************************************************
 *                            S E E M O O  N E X M O N                     *
 ***************************************************************************/

#ifndef WRAPPER_C
#define WRAPPER_C

#include <firmware_version.h>
#include <structs.h>

#ifndef WRAPPER_H
    #define VOID_DUMMY { ; }
    #define RETURN_DUMMY { ; return 0; }
    #define AT(CHIPVER, FWVER, ADDR) __attribute__((at(ADDR, "dummy", CHIPVER, FWVER)))
#else
    #define VOID_DUMMY ;
    #define RETURN_DUMMY ;
    #define AT(CHIPVER, FWVER, ADDR)
#endif

/* ==========================================================================
 * MT7915 nb04 firmware function addresses.
 *
 * All addresses verified for the firmware in
 *   firmwares/mt7915/20240429/.
 * Ghidra (`WIFI_RAM_CODE.elf` / `MERGED.elf`) holds a DIFFERENT build of the
 * firmware - addresses there are non-uniformly shifted ~48-180 KB per
 * function relative to the deployed build. Always verify against the
 * deployed firmware via byte-signature lookup or call-chain tracing.
 * ========================================================================= */

/* Generic chip-side logger (printf-ish, ~6 args). Kept untouched from
 * upstream nexmon for MTK SoC printing. */

/* wsysDbgHeader(u4Module, u1Class): logs the module/class header for the
 * subsequent wsysDbgOutput call. The dispatcher uses (0x24, 0x8) for
 * normal EXT cmd dispatch and (0x24, 0x2) for error/NOT_HANDLED. */
AT(CHIP_VER_MT7915, FW_VER_MTK, 0xe0055146)
AT(CHIP_VER_MT7961, FW_VER_MTK_20260224, 0x00921898)
void
wsysDbgHeader(unsigned int u4Module, unsigned char u1Class)
VOID_DUMMY

/* wsysDbgOutput(fmt, ...): printf-style logger. */
AT(CHIP_VER_MT7981, FW_VER_MTK, 0xe0041dd4)
AT(CHIP_VER_MT7915, FW_VER_MTK, 0xe0055014)
AT(CHIP_VER_MT7961, FW_VER_MTK_20260224, 0x009217ba)
int
wsysDbgOutput(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
RETURN_DUMMY

/* wsysCmdExtProcessCmd(prMsduInfo): the EXT command dispatcher entered for
 * every host-side EXT command (cid=0xed). Walks the dispatch table at
 * chip 0x002460ac matching cmdbuf[0x29] against table[i].cmd_byte. */
AT(CHIP_VER_MT7915, FW_VER_MTK, 0xe0055842)
int
wsysCmdExtProcessCmd(void *prMsduInfo)
RETURN_DUMMY

/* wsysCmdExtRespondToHif(prMsduInfo, status): builds an 8-byte status
 * response and ships it back to the host using the original cmds buffer.
 * The dispatcher itself calls this from its NOT_HANDLED path with
 * status=0xfe. Snapshot hook calls it with status=0 just to keep the
 * driver from timing out (full snapshot data path replaces this). */
AT(CHIP_VER_MT7915, FW_VER_MTK, 0xe00557f2)
int
wsysCmdExtRespondToHif(void *prMsduInfo, unsigned char status)
RETURN_DUMMY

/* wsysCmdExtStuffEventHeader(prEvent, u2Len, ucSeq, ucExtEID, ucS2DIndex):
 * fills the WIFI event header at buf+0x18. Writes len/pkt_type/eid/seq/
 * ext_eid/s2d_index; leaves option/rsv/rsv1 untouched (memset the area
 * first if you care). prEvent = pucBuffer + 0x18. */
AT(CHIP_VER_MT7915, FW_VER_MTK, 0xe00557cc)
void
wsysCmdExtStuffEventHeader(void *prEvent, unsigned short u2Len,
                           unsigned char ucSeq, unsigned char ucExtEID,
                           unsigned char ucS2DIndex)
VOID_DUMMY

/* whFbmWfdmaEventAlloc(u4Len): single-arg variant used in nb04. Returns a
 * FRAME_BUF_INFO_T* pointer in $r0, or 0 on failure. The actual frame
 * buffer (pucBuffer) is at FRAME_BUF_INFO_T offset 0xc. The event WIFI
 * header lives at pucBuffer+0x18, body at pucBuffer+0x24. */
AT(CHIP_VER_MT7915, FW_VER_MTK, 0xe00242a2)
void *
whFbmWfdmaEventAlloc(unsigned int u4Len)
RETURN_DUMMY

/* whFbmWfdmaPrepareXmit(rBufInfo): post-fill + pre-xmit setup. Calls the
 * function pointer at chip 0x0001e5d4 (the dcache-flush callback)
 * internally, plus a timestamp helper at *0x0001eac0. Must be invoked
 * after writing the body and before whFbmWfdmaCmdEventXmit. */
AT(CHIP_VER_MT7915, FW_VER_MTK, 0xe002b3d8)
void
whFbmWfdmaPrepareXmit(void *rBufInfo)
VOID_DUMMY

/* whFbmWfdmaCmdEventXmit(rBufInfo): final WFDMA send. After this the
 * event is on its way to the host. */
AT(CHIP_VER_MT7915, FW_VER_MTK, 0xe002b506)
int
whFbmWfdmaCmdEventXmit(void *rBufInfo)
RETURN_DUMMY

/* libc-style helpers in the FW. Signature-matched against Ghidras
 * WIFI_RAM_CODE.elf (identical bytes, different addresses). */
AT(CHIP_VER_MT7915, FW_VER_MTK, 0x00220614)
int
memcpy(void *dst, const void *src, unsigned int n)
RETURN_DUMMY

AT(CHIP_VER_MT7915, FW_VER_MTK, 0x0022072c)
void *
memset(void *s, int c, unsigned int n)
RETURN_DUMMY


/* ==========================================================================
 * MT7961 / MT7921AU WM firmware, linux-firmware build 20260224110949
 * (WIFI_RAM_CODE_MT7961_1.bin, sha256 b94217a9...bd6a2ab9).
 *
 * Both logger addresses were located in the deployed image itself, not read
 * off a Ghidra listing of some other build:
 *
 *   - wsysDbgOutput @ 0x009217ba.  Its first 10 bytes
 *     (3a1f943c effc 3a6fa0bc) are byte-identical to the MT7915 20240429
 *     wsysDbgOutput at 0xe0055014 below.  It is also the target of EX9
 *     instruction-table entry 2 (ITB = 0x0096c7bc, entry word 0x49490cdd =
 *     jal 0x009217ba), which is how essentially every DBGLOG call site in
 *     this firmware reaches it -- there is exactly one plain 4-byte
 *     "jal 0x009217ba" in the whole image, at 0x00966314.
 *   - wsysDbgHeader @ 0x00921898, EX9 table entry 1 (word 0x49490c4c).
 *     Prologue "push25 s4,0x0 / movd44 s0,a0" -> takes the (module, class)
 *     register pair, matching the MT7915 signature.
 *
 * Call them the way the firmware does: wsysDbgHeader(module, class) first,
 * then wsysDbgOutput(fmt, ...).  wsysDbgHeader sets the "line in progress"
 * byte at 0x020346fc that wsysDbgOutput checks before emitting anything, and
 * output is additionally gated on the log-enable global at 0x02026378 -- so
 * nothing appears unless firmware logging is on (fw_debug / DbgToHostMode).
 * ========================================================================= */

#endif /*WRAPPER_C*/

/* v61s3/v61s4: jump-target aliases for the iq_live alloc-fail retry hook.
 * These must live in wrapper.c (its .text is discarded by the link script;
 * only the patcher-generated address rules survive), because at()-dummies
 * inside a patch TU are emitted AT their target address and would
 * overwrite live firmware code. Referenced by the naked asm in
 * iq_live_dump_v61s3/src/iq_live.c. */
AT(CHIP_VER_MT7961, FW_VER_MTK_20260224, 0x0095c7c4)
void builder_alloc_site(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
    VOID_DUMMY
