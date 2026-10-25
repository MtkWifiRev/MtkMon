/* V61S6 Phase 2 synthetic-transport source. Still a polling exporter,
 * not a non-blocking MCU task. New DLM words below require the v61s3 runner.
 * iq_live: replace the RBIST dump query with a live-ring reader, so the
 * firmware's own dump-event loop becomes a continuous I/Q export engine.
 *
 * WHY (2026-09-16, from v56 + static analysis):
 *
 * icapSpectrumDumpDataEvent (0x0095c7a0) loops: alloc 0x43c event ->
 * halRbistQueryIQData fills data + data_len -> send -> 500-unit delay ->
 * repeat while data_len != 0. The event send path works end to end on the
 * SPECTRUM_CTRL path (v56 measured the data_len==0 marker arriving).
 * What never delivered data is the query itself:
 *
 *   icapSpectrumDumpDataEvent calls the wrapper 0x0096b9d4 (jal at
 *   0x0095c864) with a0..a5 = {evt+0x34 wf_cnt out, evt+0x38 smpl_cnt out,
 *   evt+0x2c bank out, evt+0x30 data_len out, evt+0x54 data buf, band_idx}
 *   and stack args {phy_idx, capture_node, capture_len, 0, 0, transport}.
 *   halRbistQueryIQData (0x0096c276) reads the 6th stack arg (transport:
 *   0=RF_TEST, 1=SPECTRUM_CTRL) where it expects "bank", and both of its
 *   init paths require that word == 0. So on the SPECTRUM_CTRL path the
 *   function returns data_len=0 immediately - every v56 dump was one
 *   end-marker event (~7 ms). The historical RF_TEST flood streamed because
 *   transport==0 there. (The uVar10/uVar7 "type/group" decompile fields are
 *   capture_node's byte fields - 1st stack arg is phy_idx, 2nd is
 *   capture_node at in_stack_00000004.)
 *
 * THE PATCH: JalPatch over the builder's `jal 0x0096b9d4` at 0x0095c864 ->
 * iq_read below. Same ABI (12 args, 6 in regs + 6 on stack). DLM mode word
 * selects behaviour; crt zeroes DLM at boot so mode 0 (stock passthrough)
 * is the safe default:
 *
 *   0x02017340 IQ_MODE   0 = stock passthrough (calls 0x0096b9d4)
 *                        1 = live follow: copy from IQ_CURSOR to live
 *                            0x800210b4, polling up to IQ_POLLS rounds
 *                        2 = frozen drain: copy until IQ_CURSOR == IQ_LIMIT
 *                            (engine stopped; 0x800210b4 reads 0 then)
 *                        5 = synthetic transport: emit monotonically
 *                            increasing u32 words without reading ICAP;
 *                            IQ_CURSOR is the next logical output word.
 *                            IQ_LIMIT selects payload format:
 *                              0 synthetic incrementing u32
 *                              1 DLPF lane-0 packed14/CS16 (1 sample/u32)
 *                              2 DLPF lane-0 CS8 (2 samples/u32)
 *                            IQ_POLLS is the raw DLPF sample index for
 *                            formats 1/2 (0..2047).
 *   0x02017344 IQ_CURSOR word cursor 0..0xffc (record = 4 words; host
 *                        writable; reader publishes after every record)
 *   0x02017348 IQ_LIMIT  frozen-drain target (cursor units)
 *   0x0201734C IQ_POLLS  catch-up poll rounds in live mode (0 -> builtin 16)
 *   0x02017350 IQ_WORDS  total words exported (host readback)
 *   0x02017354 IQ_EVENTS event counter (host readback)
 *
 * Ring geometry = the v52-v55 verified custom-range layout: range regs
 * 0x80021098=0 / 0x8002109c=0xffc, data window 0xE0200000 + cursor*4,
 * cursor wraps 0xffc -> 0 (matches (after-before)&0xfff producer math).
 * Events carry up to 256 words (MAX_WORDS below; v58d exact fit in the
 * SwRfb wire buffer 0x458). The v59a 512-word/0x860-alloc variant is
 * archived and NOT part of v60/v61t: it measured slower (173K vs 221K
 * words/s) and this experiment keeps the deployed v60 geometry.
 * The driver cap MT7921_SPECTRUM_DATA_MAX must stay >= 256.
 *
 * Continuity contract: wf_cnt field = record cursor at event start, so
 * consecutive events satisfy wf_next == (wf + data_len) & 0xfff (each event
 * advances the cursor by exactly data_len cursor units; 4 cursor units =
 * one 16-byte record = 4 words). Whole producer laps remain invisible;
 * this contract alone does not establish sample freshness or gap-free ADC.
 *
 * gcc 8.0.1 nds32 -O2/-O3 leaf-return trap (README.fw-patch.md 12.3):
 * iq_read calls halRbistQueryIQData_orig, so it is never a leaf and gcc
 * emits real returns. Re-verify in gen/patch.elf after building.
 *
 * V61T (2026-09-20): event-cycle timing breakdown, measured not assumed.
 *
 * Static facts established in Ghidra (target WIFI_RAM_CODE_MT7961_1.elf +
 * MT7961_ROM.elf; see experiment README):
 *   - The builder's inter-event pacing at 0x0095c888 is `movi a0,#0x1f4`
 *     then `jral5 a1`, i.e. (*slot)(500) where slot = *(0x00822414).
 *     ROM default target = FUN_00802178 = busy-wait of (a0+1) ticks of the
 *     free-running MCU counter at 0x89020068 (selector 4 of FUN_0080ce6c;
 *     selector 2 = 0x89020034 is the ms-class OS tick).
 *   - The PSE alloc poller FUN_0082cf04 (register 0x820c8350/0x820c0350)
 *     busy-waits through the same primitive (up to 1000 iterations), so PSE
 *     allocation latency is inside the cycle residue too.
 *   - The deployed v60 image still carries movi a0,#0x1f4 at 0x0095c888
 *     (file offset 0x47888, sha256 2769a99b...): v60 = stock 500-unit
 *     pacing + v59b burst copy = 405K words/s. The "350" note in the old
 *     header was never applied to v60.
 *   - DLM 0x020173ec..0x02017546 is a live init-time descriptor table
 *     (FUN_0094912c/FUN_00949826); free scratch ends at 0x020173ea. The
 *     counters below therefore stop at 0x02017397.
 *
 * V61T adds two hooks, both inert until the host writes the DLM words:
 *
 *   1. iq_pace_hook: JalPatch over the builder's `movi a0,#0x1f4` at
 *      0x0095c888. It sets a0 = IQ_PACE (0 -> stock 500) and re-loads
 *      a1 = *(0x00822414) (the live slot target, in case RAM retargeted
 *      it), then returns; the stock `jral5 a1` performs the delay. With
 *      IQ_PACE = 0 the behaviour is bit-identical to stock. Clobbers only
 *      a0/a1 (r0/r1), which the replaced movi/jral5 pair uses anyway.
 *
 *   2. Timing aggregates in iq_read (modes 1/2 only), all tick-based:
 *      entry-to-entry cycle, in-copy ticks, idle-spin (catch-up wait)
 *      ticks, poll rounds, SwRfb free-list minimum. Window is armed by
 *      IQ_RST=1 and delimited by IQ_ENTRY0/IQ_TICK, so the host can
 *      convert ticks to seconds and separate copy from residue without
 *      any per-event buffering (only ~140 bytes of DLM free).
 *
 * V61R (2026-09-20): "use the RX path" — bulk payloads through the proven
 * transport, driven lean. Static facts added (target + ROM):
 *
 *   - Per-packet submission does TWO synchronous PSE handshakes: the SwRfb
 *     backing alloc (FUN_00941416 -> FUN_0082cf04 polls 0x820c8350 up to
 *     1000 delay-ticks) and the queue command (009463a4 -> 0093a7c2 ->
 *     ROM 0082cd1e writes 0x820c0320 + 0x80000000 and polls it the same
 *     way). CR writes 0x1405a0/a1 (dest<<16-relative) carry packet page
 *     info, 0x1405c0 carries the FID.
 *   - Reclaim is interrupt-driven: the WSYS ISR (FUN_00916a9a) dispatches
 *     PSE completion bits and runs the completion loop FUN_0091715a, which
 *     pops completed entries (FUN_009462fc), dispatches or FbmPoolPushFree's
 *     them. v60 already sustains >= 1584 recycles/s with pool depth 16, so
 *     reclaim is not the first-order suspect.
 *   - The wire buffer is elastic (v59a, proven): the alloc request flows to
 *     FUN_00941416 (PSE honors the size); iq_read stamps rxd.len =
 *     total*4+60 per send (the LogEventSendVarLen-proven formula; gives
 *     driver avail = total exactly).
 *
 * v61r therefore = v61t + runtime event size + per-send rxd.len stamp:
 *
 *   3. iq_alloc_hook: JalPatch over the builder's `movi a0,#0x43c` at
 *      0x0095c7c4. Sets a0 = 0x43c (stock, IQ_MAX <= 256) or
 *      IQ_MAX*4 + 0x60 (512 -> 0x860, the v59a-proven request). The stock
 *      `ex9.it 0x21` allocator call then proceeds with our size. Clobbers
 *      only a0.
 *   4. iq_read honors IQ_MAX as the per-event word bound (firmware-clamped
 *      to 512 = the deployed driver's MT7921_SPECTRUM_DATA_MAX) and stamps
 *      rxd.len per event so big payloads actually reach the host.
 *
 * Wire format is byte-identical to v60 events -> the deployed driver parses
 * everything unchanged. Sweep grid: IQ_PACE x IQ_MAX. IQ_PACE = 0xffffffff
 * means ZERO inter-event delay (the v58b cell, now retried with the v59b
 * burst copy and big packets); 0 means stock 500.
 *
 * V61S (2026-09-20): lossless-collector support, two changes over v61r:
 *
 *   1. Live-mode wait strategy (replaces the 30,000-iteration busy spin):
 *      on catching up with the cached producer position, reread the
 *      producer pointer IMMEDIATELY (IQ_RCHK_N total, IQ_RCHK hits), and
 *      only when the reread shows nothing, wait IQ_WAIT timer ticks
 *      (0x89020068, ~1 us/tick; 0 -> default 100) before the next recheck.
 *      IQ_EMPTY counts producer-quiet waits. Live mode never returns an
 *      empty event: len 0 IS the builder's end marker, so temporary
 *      emptiness keeps polling; only IQ_MODE=3 (explicit stop) emits the
 *      marker. Mode 2 (frozen drain) keeps stock bounded-poll semantics.
 *
 *   2. Events complete at IQ_MAX words (or on explicit stop), so live
 *      events are producer-paced and full-size when the producer keeps up.
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

#define IQ_MODE    (*(volatile unsigned int *)0x02017340)
#define IQ_CURSOR  (*(volatile unsigned int *)0x02017344)
#define IQ_LIMIT   (*(volatile unsigned int *)0x02017348)
#define IQ_POLLS   (*(volatile unsigned int *)0x0201734c)
#define IQ_WORDS   (*(volatile unsigned int *)0x02017350)
#define IQ_EVENTS  (*(volatile unsigned int *)0x02017354)

/* v61t timing words: 0x02017358..0x02017397, below the live init table at
 * 0x020173ec (verified free in Ghidra). All reset by IQ_RST=1. */
#define IQ_PACE    (*(volatile unsigned int *)0x02017358)
#define IQ_RST     (*(volatile unsigned int *)0x0201735c)
#define IQ_N       (*(volatile unsigned int *)0x02017360)
#define IQ_CYC_SUM (*(volatile unsigned int *)0x02017364)
#define IQ_CYC_MAX (*(volatile unsigned int *)0x02017368)
#define IQ_CYC_MIN (*(volatile unsigned int *)0x0201736c)
#define IQ_COPY_SUM (*(volatile unsigned int *)0x02017370)
#define IQ_COPY_MAX (*(volatile unsigned int *)0x02017374)
#define IQ_BUSY_SUM (*(volatile unsigned int *)0x02017378)
#define IQ_POLL_N  (*(volatile unsigned int *)0x0201737c)
#define IQ_SLOT    (*(volatile unsigned int *)0x02017380)
#define IQ_POOL_MIN (*(volatile unsigned int *)0x02017384)
#define IQ_ENTRY0  (*(volatile unsigned int *)0x02017388)
#define IQ_TICK    (*(volatile unsigned int *)0x0201738c)
#define IQ_PREV    (*(volatile unsigned int *)0x02017390)
#define IQ_MAX     (*(volatile unsigned int *)0x02017394)
/* v61s: wait-strategy words (all < 0x020173ea, below the init table) */
#define IQ_WAIT    (*(volatile unsigned int *)0x02017398) /* timer wait ticks; 0->100 */
#define IQ_RCHK    (*(volatile unsigned int *)0x0201739c) /* immediate rereads that revealed data */
#define IQ_RCHK_N  (*(volatile unsigned int *)0x020173a0) /* total immediate rereads */
#define IQ_EMPTY   (*(volatile unsigned int *)0x020173a4) /* producer-quiet waits */
#define IQ_GENERATION (*(volatile unsigned int *)0x020173a8)
#define IQ_EOF_GENERATION (*(volatile unsigned int *)0x020173ac)
#define IQ_CLAMP_COUNT (*(volatile unsigned int *)0x020173b0)
#define IQ_CLAMP_LAST (*(volatile unsigned int *)0x020173b4)
#define IQ_FAULT (*(volatile unsigned int *)0x020173b8)
#define IQ_ALLOC_RETRY (*(volatile unsigned int *)0x020173bc)
/* v61s4 full-ring frozen drain: the host writes the exact number of ring
 * words to export (e.g. 4096 for a complete ring) alongside CURSOR. Mode 2
 * decrements this per copied word and returns data_len 0 (the clean EOF) once
 * it reaches 0. This terminates a full-ring drain deterministically, which the
 * cursor==IQ_LIMIT test cannot: the cursor wraps mod 4096 to 0 while a limit
 * of 0x1000 is never equal to it, so the legacy loop spun forever (and the
 * legacy LIMIT=0xffc always omitted the final 4-word record). When
 * IQ_DRAIN_LEFT is 0 the legacy cursor==IQ_LIMIT behaviour is unchanged. */
#define IQ_DRAIN_LEFT (*(volatile unsigned int *)0x020173c0)
/* Fault codes: 1 invalid live pointer, 2 cursor/limit alignment/range.
 * EOF_GENERATION means EOF prepared, not transmitted: the host must ALSO
 * receive a new end marker before any further MCU commands or mode changes.
 */

#define DELAY_SLOT_ROM (*(volatile unsigned int *)0x00822414) /* IFCALL slot value */

#define MCUTICK    (*(volatile unsigned int *)0x89020068) /* free-run counter, FUN_0080ce6c(4) */
#define DELAY_SLOT (*(volatile unsigned int *)0x00822414) /* builder delay IFCALL slot */
#define SWRFB_FREE (*(volatile unsigned int *)0x0202fe7c) /* pool 0 free count (FbmPoolPop) */

#define RBIST_WPTR (*(volatile unsigned int *)0x800210b4)
#define RING_BASE  0xe0200000u
#define RING_END   0xffcu          /* last valid cursor unit; wrap at 0x1000 */
#define MAX_WORDS  256u            /* 64 records; SwRfb wire buffer 0x458 - exact fit (v58d) */

/* v59a remnants removed: the 0x860-alloc/512-word variant is archived in
 * mt7921-v57-iq-live-dump-20260916/v59/; v61t keeps the deployed v60
 * geometry (256-word events, stock 0x43c alloc request). */

/* the wrapper the builder calls (hooked at 0x0095c864); used for passthrough.
 * noinline + opaque body: the dummy body lives in THIS TU, and gcc -O3
 * inlined it into iq_read's mode-0 path on the first build (the passthrough
 * jal disappeared entirely, mode 0 returned 0 without touching the real
 * function). noinline forces a real jal to 0x0096b9d4, where the firmware's
 * own bytes execute. */
__attribute__((at(0x0096b9d4, "dummy", CHIP_VER_MT7961, FW_VER_MTK_20260224), noinline, noclone, used))
int
halRbistQueryIQData_orig(void *a0, void *a1, void *a2, void *a3, void *a4,
                         unsigned a5, unsigned a6, unsigned a7, unsigned a8,
                         unsigned a9, unsigned a10, unsigned a11)
{
    asm volatile("");
    return 0;
}

/* v60: preserve count outside the load-multiple clobber set (r10), and
 * increment the destination (smw.bim). The previous r2 count was replaced
 * by capture data; smw.adm wrote backward over event metadata.
 *
 * v59b: burst copy helper - the copy, not the transport, is the bottleneck
 * (measured ~4.5-6.4 us/word with per-word loads from the E02 window).
 * lmw.bim loads 8 registers = 8 words in ONE instruction; the bus burst
 * amortizes the window latency (the v55 burst validated lmw from 0xE02xxxx
 * on this hardware). ABI: a0 = src byte address (word aligned, inside the
 * E02 ring), a1 = dst word pointer, a2 = word count (multiple of 8).
 * Reduced-register compliant: r0-r7 data, r8-r10 pointers, r6-r10+lp
 * saved/restored. */
__attribute__((naked)) void iq_burst_copy(unsigned src, unsigned *dst,
                                          unsigned words) {
    asm(
        "smw.adm $r6,[$sp],$r10,#0x2  ! {$r6~$r10, $lp} \n"
        "mov55  $r8, $r0 \n"
        "mov55  $r9, $r1 \n"
        "mov55  $r10, $r2 \n"
        "1: \n"
        "lmw.bim $r0,[$r8],$r7,#0x0  ! {$r0~$r7} \n"
        "smw.bim $r0,[$r9],$r7,#0x0  ! {$r0~$r7} \n"
        "subi45 $r10, #8 \n"
        "bnez $r10, 1b \n"
        "lmw.bim $r6,[$sp],$r10,#0x2  ! {$r6~$r10, $lp} \n"
        "ret5 $lp \n"
    );
}

/* v61r: replaces the builder's `movi a0,#0x1f4` at 0x0095c888. Sets
 * a0 = IQ_PACE (0 -> stock 500; 0xffffffff -> zero delay) and
 * a1 = the live delay-slot target *(0x00822414); the following stock
 * `jral5 a1` performs the wait. Clobbers only a0/a1 (r0/r1) - the same
 * registers the replaced movi/jral5 pair consumes. lp untouched. */
__attribute__((naked)) void iq_pace_hook(void) {
    asm(
        "sethi $r0, #0x2017 \n"          /* a0 = 0x02017000            */
        "lwi   $r0, [$r0+#0x358] \n"     /* a0 = IQ_PACE               */
        "bnez  $r0, 1f \n"
        "movi  $r0, #0x1f4 \n"           /* 0 -> stock 500 units       */
        "j     2f \n"
        "1: \n"
        "bgez  $r0, 2f \n"               /* positive: use as given     */
        "movi  $r0, #0x0 \n"             /* 0xffffffff -> zero delay   */
        "2: \n"
        "sethi $r1, #0x822 \n"           /* a1 = 0x00822000            */
        "lwi   $r1, [$r1+#0x414] \n"     /* a1 = *(0x00822414)         */
        "ret5  $lp \n"
    );
}

/* v61r: replaces the builder's alloc-size `movi a0,#0x43c` at 0x0095c7c4.
 * a0 = 0x43c for IQ_MAX 0 (stock 256-word template) or IQ_MAX*4 + 0x60
 * (512 words -> 0x860, the v59a-proven request; PSE honors it, wire =
 * request + 0x18). The stock `ex9.it 0x21` allocator call then proceeds
 * with our size. Clobbers only a0 (a1 untouched). */
__attribute__((naked)) void iq_alloc_hook(void) {
    asm(
        "sethi $r0, #0x2017 \n"          /* a0 = 0x02017000            */
        "lwi   $r0, [$r0+#0x394] \n"     /* a0 = IQ_MAX                */
        "bnez  $r0, 1f \n"
        "movi  $r0, #0x43c \n"           /* 0 -> stock request         */
        "ret5  $lp \n"
        "1: \n"
        "addi  $sp, $sp, #-4 \n"
        "swi   $r1, [$sp+#0] \n"
        "slti  $r1, $r0, #0x201 \n"        /* < 513 (signed)         */
        "beqz  $r1, 2f \n"                 /* >= 513 -> 512          */
        "slti  $r1, $r0, #0 \n"            /* negative (huge u32)?   */
        "bnez  $r1, 2f \n"                 /* negative -> 512        */
        "j     3f \n"                      /* in-range: keep         */
        "2: \n"
        "movi  $r0, #0x200 \n"
        "3: \n"
        "andi  $r1, $r0, #0x3 \n"
        "beqz  $r1, 4f \n"
        "movi  $r0, #0x100 \n"
        "4: \n"
        "slli  $r0, $r0, #0x2 \n"        /* a0 = IQ_MAX * 4            */
        "addi  $r0, $r0, #0x60 \n"       /* + 0x60                     */
        "lwi   $r1, [$sp+#0] \n"
        "addi  $sp, $sp, #4 \n"
        "ret5  $lp \n"
    );
}

/* v61s2: freeze clamp. hifSubmitBackpressureDelay (0x00946e5e) reaches the
 * builder freeze: on PSE status 2/5 with TESTMODE on it calls the delay
 * slot with a0 = the caller's a2, which arrives through the
 * swRfbSubmitQueueCmd -> hifPreSubmitDispatch call chain in registers and
 * is huge exactly when the host's USB event pipe stalls (driver 32-packet
 * buffer full / xHCI ep8 desync) -> the MCU task busy-waits ~(a2+1) ticks
 * (~1 us each) = forever, mid-send, silently (no marker, pool healthy).
 *
 * This hook replaces the two instructions `mov55 a0,s3; jral5 a1` at
 * 0x00946ea6: a0 = min(s3, 1000), a1 = live slot value, then the displaced
 * jral5 a1 is re-executed here. Clobbers only a0/a1/a2 (a2 is dead - the
 * caller copied it to s3 at entry; a0/a1 are the delay args). s0/s1/s3 and
 * lp preserved: lp is saved on the caller's stack and restored, because
 * jral5 a1 overwrites it (the displaced instruction did the same). */
__attribute__((naked)) void iq_delay_clamp_hook(void) {
    asm(
        "addi  $sp, $sp, #-4 \n"           /* sp -= 4                */
        "swi   $lp, [$sp+#0x0] \n"         /* save caller's lp       */
        "mov55 $a0, $s3 \n"                /* a0 = requested delay   */
        /* Preserve stock behavior outside exporter modes 1..5.
         * Signed compares only (this assembler has no sltiu): mode 0 is
         * excluded explicitly, mode >= 6 by the <6 test. */
        "sethi $a2, #0x2017 \n"
        "lwi   $a2, [$a2+#0x340] \n"
        "beqz  $a2, 2f \n"                 /* mode 0: unclamped      */
        "slti  $a2, $a2, #6 \n"            /* modes 1..5: clamp     */
        "beqz  $a2, 2f \n"
        "movi  $a2, #0x3e8 \n"             /* 1000 ticks (~1 ms)     */
        "slt   $a2, $a0, $a2 \n"           /* a2 = (a0 < 1000)       */
        "beqz  $a2, 1f \n"
        "j     2f \n"
        "1: \n"
        "sethi $a1, #0x2017 \n"
        "swi   $a0, [$a1+#0x3b4] \n"     /* original delay request */
        "lwi   $a2, [$a1+#0x3b0] \n"
        "addi  $a2, $a2, #1 \n"
        "swi   $a2, [$a1+#0x3b0] \n"
        "movi  $a0, #0x3e8 \n"             /* clamp                  */
        "2: \n"
        "sethi $a1, #0x822 \n"             /* a1 = 0x00822000        */
        "lwi   $a1, [$a1+#0x414] \n"       /* a1 = *(0x00822414)     */
        "jral5 $a1 \n"                     /* delay(clamped a0)      */
        "lwi   $lp, [$sp+#0x0] \n"         /* restore lp = 0x00946eaa */
        "addi  $sp, $sp, #0x4 \n"
        "ret5  $lp \n"
    );
}

/* v62: IQB2 block streaming through MCU-WFDMA TX0 (mode 6).
 *
 * TX0 is the boot ROM's firmware->host ring (fwDownloadSendToHif ->
 * halWfdmaHardXmit(0, 0, pkt) @ ROM 0x0082e31e). The RAM firmware sends its
 * events through PSE instead and leaves TX0 idle (base 0x02109900, 4
 * descriptors, cidx == didx == 2). Its DMA reads any MCU address we tried,
 * including the ICAP window 0xe0200000, and delivers each packet to the host
 * main bulk-IN queue, where mt7921/iqstream.c takes it. No CPU copy: per
 * block the CPU writes one 0x44-byte header and one or two descriptors.
 *
 * Descriptor: w0 SDP0 | w1 DDONE b31, LS0 b30, SDL0 b29:16, LS1 b14,
 * SDL1 b13:0 | w2 SDP1 | w3 0. The DMA stalls on a descriptor whose DDONE is
 * still set, so every reuse rewrites w1. A block that crosses the source
 * ring end is chained over two descriptors (LS only on the last segment).
 *
 * Control block (host-written; power-on noise until then, so it is only read
 * in mode 6 and checked by magic) at 0x02109a00, words:
 *   0 magic "IQD6"  1 source base  2 ring bytes (power of two, <= 256 KiB;
 *   v63: the producer pointer is no longer masked to 12 bits, so follow mode
 *   works with RBIST rings up to 64 KiB, end register 0x3ffc; v65: up to
 *   256 KiB, end register 0xfffc, which needs the v64 SYSRAM layout where
 *   ICAP owns 0xe0200000-0xe023ffff)
 *   3 block bytes (16-byte multiple, <= 16 KiB - 0x44)  4 flags (bit0: follow
 *   the RBIST producer; else free-run over the source ring)  5 block limit
 *   (0 = until IQ_MODE 3)  6 generation
 * A block that crosses the ring end is split there into two descriptors.
 * For the ICAP window choose block sizes that keep that split point aligned
 * (e.g. 8192 or 14336 = 7 x 2 KiB in a 16 KiB ring): splits at odd offsets
 * (first part = 480 mod 640 bytes with 16304-byte blocks) returned wrong ICAP
 * data in 1 of 8 blocks, while unsplit and DLM sources were exact. Packets
 * must stay below ~16392 bytes: longer MCU->USB transfers were truncated.
 * Host rule: the firmware SER check (g_ucEnableSER, 0x0202e08c) must be
 * masked while streaming from 0xe0200000; after ~0.2-0.4 GB of such reads it
 * otherwise resets the MCU WFDMA and host commands time out.
 * Status written by the firmware:
 *   8 blocks submitted  9 descriptors completed  10 producer-wait polls
 *   11 ring-full polls  12 state (1 running, 2 stopped, 0xfxx fault)
 *   13 EOF blocks  14 start tick  15 end tick  16 late blocks (producer
 *   within one block of lapping at submit)  17 max producer lead (bytes)
 * Descriptor ring 0x0210a000 (32 x 16 B), header slots 0x0210b000 (32 x
 * 0x48 B); 0x02109980..0x0210bfff held power-on noise, i.e. unused. */
#define WF_TX0_BASE (*(volatile unsigned *)0x54000300)
#define WF_TX0_CNT  (*(volatile unsigned *)0x54000304)
#define WF_TX0_CIDX (*(volatile unsigned *)0x54000308)
#define WF_TX0_DIDX (*(volatile unsigned *)0x5400030c)
#define TX0_ROM_BASE 0x02109900u
#define TX0_ROM_CNT  4u
#define TX0_ROM_IDX  2u            /* ROM's cached index since boot */
#define DMA_RING     0x0210a000u
#define DMA_HDRS     0x0210b000u
#define DMA_NDESC    32u
#define DMA_HDR_LEN  0x44u         /* MCU RXD 0x24 + IQB2 0x20 */
#define DMA_CTL      ((volatile unsigned *)0x02109a00u)
#define IQD6_MAGIC   0x36445149u
#define IQB2_MAGIC   0x32425149u
#define IQB2_EOF     0x8000u
/* This file is built with -Os (Makefile target-specific CFLAGS): -O3
 * inlining overflowed the 2820-byte cave. Do NOT use optimize("Os")
 * attributes instead: with them gcc 8 appended a ret5 to every naked
 * JalPatch stub (4 -> 6 bytes), overwriting the instruction after each hook
 * (at 0x0095c7c8 the builder's allocator call), and the builder returned
 * before ever calling iq_read. */
#define IQ_DMA_FN    __attribute__((noinline))

static IQ_DMA_FN void
iq_dma_desc(unsigned slot, unsigned sdp0, unsigned w1, unsigned sdp1)
{
    volatile unsigned *d = (volatile unsigned *)(DMA_RING + slot * 16u);

    d[0] = sdp0;
    d[2] = sdp1;
    d[3] = 0;
    d[1] = w1;                     /* DDONE clear: hand to the DMA */
}

static IQ_DMA_FN unsigned
iq_dma_header(unsigned slot, unsigned len, unsigned seq, unsigned cursor,
              unsigned flags)
{
    volatile unsigned *h = (volatile unsigned *)(DMA_HDRS + slot * 0x48u);
    unsigned total = DMA_HDR_LEN + len;

    h[0] = 0x38000000u | total;    /* RXD0: event packet, length */
    h[1] = 0; h[2] = 0; h[3] = 0; h[4] = 0; h[5] = 0;
    h[6] = ((total - 0x18u) & 0xffffu) | 0x70000u;
    h[7] = 0xed;                   /* eid MCU_EVENT_EXT, seq 0 */
    h[8] = 0x50;                   /* ext_eid */
    h[9] = IQB2_MAGIC;
    h[10] = (32u << 16) | 1u;      /* version 1, header 32 bytes */
    h[11] = len;
    h[12] = seq;
    h[13] = cursor;
    h[14] = flags;
    h[15] = DMA_CTL[6];            /* generation */
    h[16] = MCUTICK;
    return (unsigned)h;
}

/* Free descriptor slots between our producer index and the DMA index. */
static IQ_DMA_FN unsigned
iq_dma_room(unsigned head)
{
    return DMA_NDESC - 1u - ((head - WF_TX0_DIDX) & (DMA_NDESC - 1u));
}

static IQ_DMA_FN int
iq_dma_stream(unsigned *wf_out, unsigned *smpl_out, unsigned *bank_out,
              unsigned *len_out)
{
    volatile unsigned *c = DMA_CTL;
    unsigned src = c[1], ring = c[2], block = c[3], follow = c[4] & 1u;
    unsigned limit = c[5], head, cursor = 0, seq = 0, done_idx, t0;

    c[12] = 1;
    c[8] = c[9] = c[10] = c[11] = c[13] = c[16] = c[17] = 0;
    head = WF_TX0_CIDX;
    if (c[0] != IQD6_MAGIC || !ring || (ring & (ring - 1u)) ||
        ring > 0x40000u || !block || (block & 15u) || block > ring ||
        block > 0x3fbcu || head != WF_TX0_DIDX || head >= DMA_NDESC) {
        c[12] = 0xf01;
        goto out;
    }

    WF_TX0_BASE = DMA_RING;
    WF_TX0_CNT = DMA_NDESC;
    done_idx = head;
    c[14] = MCUTICK;
    if (follow)
        cursor = (RBIST_WPTR << 2) & (ring - 1u);

    while (IQ_MODE == 6 && (!limit || seq < limit)) {
        unsigned didx = WF_TX0_DIDX, n1, flags, lead = 0;

        c[9] += (didx - done_idx) & (DMA_NDESC - 1u);
        done_idx = didx;
        if (iq_dma_room(head) < 2u) {
            c[11] += 1;
            continue;
        }
        flags = c[4] & 0xffu;
        if (follow) {
            unsigned pos = (RBIST_WPTR << 2) & (ring - 1u);

            lead = (pos - cursor) & (ring - 1u);
            if (lead < block) {
                c[10] += 1;
                    continue;
            }
            if (lead > ring - block)
                c[16] += 1;
            if (lead > c[17])
                c[17] = lead;
            flags |= pos << 14;    /* producer word index in bits 31:16 */
        }

        n1 = ring - cursor;
        if (n1 > block)
            n1 = block;
        {
            unsigned hdr = iq_dma_header(head, block, seq, cursor, flags);
            unsigned first = head;

            if (n1 == block) {
                iq_dma_desc(first, hdr, (DMA_HDR_LEN << 16) | (1u << 14) | n1,
                            src + cursor);
                head = (head + 1u) & (DMA_NDESC - 1u);
            } else {
                unsigned second = (first + 1u) & (DMA_NDESC - 1u);

                iq_dma_desc(second, src + ((cursor + n1) & (ring - 1u)),
                            (1u << 30) | ((block - n1) << 16), 0);
                iq_dma_desc(first, hdr, (DMA_HDR_LEN << 16) | n1,
                            src + cursor);
                head = (second + 1u) & (DMA_NDESC - 1u);
            }
        }
        __asm__ volatile("dsb" ::: "memory");
        WF_TX0_CIDX = head;
        cursor = (cursor + block) & (ring - 1u);
        seq++;
        c[8] = seq;
    }

    /* Stop: EOF blocks until the index is back at the ROM's cached value,
     * then wait for the DMA and hand TX0 back in its boot geometry. */
    do {
        while (iq_dma_room(head) < 1u)
            ;
        iq_dma_desc(head,
                    iq_dma_header(head, 0, seq, cursor, IQB2_EOF),
                    (1u << 30) | (DMA_HDR_LEN << 16), 0);
        __asm__ volatile("dsb" ::: "memory");
        head = (head + 1u) & (DMA_NDESC - 1u);
        WF_TX0_CIDX = head;
        c[13] += 1;
    } while (head != TX0_ROM_IDX);

    t0 = MCUTICK;
    while (WF_TX0_DIDX != head) {
        if (MCUTICK - t0 > 200000u) {
            c[12] = 0xf02;         /* DMA did not drain: keep our ring */
            goto out;
        }
    }
    c[9] += (head - done_idx) & (DMA_NDESC - 1u);
    WF_TX0_BASE = TX0_ROM_BASE;
    WF_TX0_CNT = TX0_ROM_CNT;
    c[15] = MCUTICK;
    c[12] = 2;

out:
    *wf_out = 0;
    *smpl_out = 0;
    *bank_out = 0;
    *len_out = 0;
    IQ_EVENTS += 1;
    IQ_EOF_GENERATION = IQ_GENERATION;
    return 0;
}

int
iq_read(unsigned *wf_out, unsigned *smpl_out, unsigned *bank_out,
        unsigned *len_out, unsigned *data_out,
        unsigned band, unsigned phy, unsigned node, unsigned caplen,
        unsigned z1, unsigned z2, unsigned transport)
{
    unsigned mode = IQ_MODE;
    unsigned total = 0;
    unsigned cursor = IQ_CURSOR;
    unsigned start;
    unsigned polls = IQ_POLLS;
    unsigned poll = 0;
    unsigned maxw;

    if (polls == 0)
        polls = 16;

    /* Explicit stop: finish the builder without entering the stock reader.
     * v61s4: in mode 2, IQ_LIMIT may equal 0x1000 (RING_END+1) to request a
     * full 4096-word ring drain. The drain loop terminates when
     * cursor == IQ_LIMIT (mod-4096 arithmetic), so a limit one past the last
     * record yields exactly one full ring instead of the legacy 4092-word
     * short read that omitted the final record. cursor must stay <= RING_END
     * (0xffc): it is a ring position, never a one-past-end value. */
    if ((mode == 1 || mode == 2) &&
        (cursor > RING_END || (cursor & 3u) ||
         (mode == 2 &&
          (IQ_LIMIT > (RING_END + 4u) || (IQ_LIMIT & 3u) ||
           IQ_DRAIN_LEFT > (RING_END + 4u) || (IQ_DRAIN_LEFT & 3u))))) {
        IQ_FAULT = 2;
        IQ_MODE = 3;
        mode = 3;
    }
    if (mode == 3) {
        *wf_out = cursor;
        *smpl_out = 0;
        *bank_out = 0;
        *len_out = 0;
        IQ_EVENTS += 1;
        IQ_EOF_GENERATION = IQ_GENERATION;
        return 0;
    }

    if (mode == 6)
        return iq_dma_stream(wf_out, smpl_out, bank_out, len_out);

    if (mode != 1 && mode != 2)
        return halRbistQueryIQData_orig(wf_out, smpl_out, bank_out, len_out,
                                        data_out, band, phy, node, caplen,
                                        z1, z2, transport);

    if ((mode == 1 || mode == 2) && cursor > RING_END)
        cursor = 0;

    /* v61r: runtime event bound. 0 -> stock 256; clamped to 512 = the
     * deployed driver's MT7921_SPECTRUM_DATA_MAX (bigger needs a driver
     * rebuild; do not raise this constant without one). */
    maxw = IQ_MAX;
    if (maxw == 0)
        maxw = MAX_WORDS;
    else if (maxw > 512u)
        maxw = 512u;
    if (maxw & 3u)
        maxw = MAX_WORDS; /* identical policy to allocation hook */

    start = cursor;                /* event continuity marker */
    {
        unsigned busy = 0;

        if (mode == 1) {
            /* v61s live loop: fill up to maxw; when caught up, reread the
             * producer pointer IMMEDIATELY (IQ_RCHK counts how often that
             * alone reveals more data), and only then wait IQ_WAIT timer
             * ticks before the next recheck. An empty event must never be
             * returned in live mode - len 0 IS the builder's end marker;
             * only IQ_MODE=3 ends the stream (explicit stop). */
            while (total < maxw) {
                unsigned wptr, guard;

                if (IQ_MODE == 3)
                    break;             /* explicit stop -> len-0 marker */

                wptr = RBIST_WPTR;
                if (wptr > RING_END || (wptr & 3u)) {
                    IQ_FAULT = 1;
                    IQ_MODE = 3;
                    break;
                }

                guard = 0;
                while (cursor != wptr && total < maxw && guard < 0x1000) {
                    /* v59b: burst-copy 8-word chunks while they fit before
                     * the wrap; the per-record loop handles the wrap tail. */
                    unsigned room = maxw - total;
                    unsigned towrap = 0x1000u - cursor;
                    unsigned dist = (wptr - cursor) & 0xfff;
                    unsigned chunk = room;
                    if (dist < chunk)
                        chunk = dist;
                    if (towrap < chunk)
                        chunk = towrap;
                    chunk &= ~7u;
                    if (chunk >= 8) {
                        iq_burst_copy(0xe0200000u + (cursor << 2),
                                      data_out + total, chunk);
                        total += chunk;
                        cursor += chunk;
                        if (cursor > RING_END)
                            cursor = 0;
                        IQ_CURSOR = cursor;
                        guard += chunk / 4;
                        continue;
                    }
                    volatile unsigned int *src =
                        (volatile unsigned int *)(0xe0200000u + (cursor << 2));
                    data_out[total++] = src[0];
                    data_out[total++] = src[1];
                    data_out[total++] = src[2];
                    data_out[total++] = src[3];
                    cursor += 4;
                    if (cursor > RING_END)
                        cursor = 0;
                    IQ_CURSOR = cursor;
                    guard++;
                }

                if (total >= maxw)
                    break;

                /* caught up: immediate pointer reread (no wait) */
                {
                    unsigned wptr2 = RBIST_WPTR;
                    if (wptr2 > RING_END || (wptr2 & 3u)) {
                        IQ_FAULT = 1;
                        IQ_MODE = 3;
                        break;
                    }
                    IQ_RCHK_N += 1;
                    if (wptr2 != cursor) {
                        IQ_RCHK += 1;
                        continue;      /* more data arrived: copy now */
                    }
                }

                /* producer quiet: short timer-bounded wait, then recheck */
                {
                    unsigned wticks = IQ_WAIT;
                    unsigned t_wait0;
                    if (wticks == 0xffffffffu)
                        wticks = 0;            /* sentinel: recheck only */
                    else if (wticks == 0)
                        wticks = 100;          /* default 100 ticks (~us) */
                    else if (wticks > 10000)
                        wticks = 10000;
                    t_wait0 = MCUTICK;
                    while ((MCUTICK - t_wait0) < wticks && IQ_MODE != 3) { }
                    busy += MCUTICK - t_wait0;
                    IQ_EMPTY += 1;
                }
            }
        } else {
            /* mode 2: frozen drain - stock-style bounded polls; empty
             * return IS the desired clean end here. */
            for (poll = 0; poll < polls && total < maxw; poll++) {
                unsigned wptr = IQ_LIMIT;
                unsigned guard = 0;

                /* v61s4: drain-count mode terminates as soon as the count is
                 * exhausted, independent of the (possibly aliasing) limit. */
                if (IQ_DRAIN_LEFT == 0 && IQ_LIMIT == (RING_END + 4u))
                    break;

                while (cursor != wptr && total < maxw && guard < 0x1000) {
                    unsigned room = maxw - total;
                    unsigned towrap = 0x1000u - cursor;
                    /* v61s4: with a drain count active, IQ_LIMIT is not the
                     * terminator (it may alias the start cursor on a full
                     * ring); run to the wrap and let IQ_DRAIN_LEFT stop it. */
                    unsigned dist = IQ_DRAIN_LEFT ? (0x1000u - cursor)
                                                  : ((wptr - cursor) & 0xfff);
                    unsigned chunk = room;
                    if (dist < chunk)
                        chunk = dist;
                    if (towrap < chunk)
                        chunk = towrap;
                    chunk &= ~7u;
                    /* v61s4: never copy past the host-requested drain count. */
                    if (IQ_DRAIN_LEFT && chunk > IQ_DRAIN_LEFT)
                        chunk = IQ_DRAIN_LEFT & ~7u;
                    if (chunk >= 8) {
                        iq_burst_copy(0xe0200000u + (cursor << 2),
                                      data_out + total, chunk);
                        total += chunk;
                        if (IQ_DRAIN_LEFT)
                            IQ_DRAIN_LEFT -= chunk;
                        cursor += chunk;
                        if (cursor > RING_END)
                            cursor = 0;
                        IQ_CURSOR = cursor;
                        guard += chunk / 4;
                        continue;
                    }
                    /* v61s4: a sub-8 remainder of the drain count ends the
                     * ring copy here (4096 is 8-aligned, so this is only a
                     * safety net). */
                    if (IQ_DRAIN_LEFT && IQ_DRAIN_LEFT < 4)
                        break;
                    volatile unsigned int *src =
                        (volatile unsigned int *)(0xe0200000u + (cursor << 2));
                    data_out[total++] = src[0];
                    data_out[total++] = src[1];
                    data_out[total++] = src[2];
                    data_out[total++] = src[3];
                    if (IQ_DRAIN_LEFT)
                        IQ_DRAIN_LEFT -= 4;
                    cursor += 4;
                    if (cursor > RING_END)
                        cursor = 0;
                    IQ_CURSOR = cursor;
                    guard++;
                }

                /* v61s4: requested drain count reached -> return what we have;
                 * the next iq_read call sees IQ_DRAIN_LEFT == 0 and reports
                 * data_len 0, the clean EOF that ends the builder loop. */
                if (IQ_DRAIN_LEFT == 0)
                    break;

                if (total >= maxw)
                    break;

                {
                    unsigned t_busy0 = MCUTICK;
                    volatile unsigned d;
                    for (d = 0; d < 30000; d++) { }
                    busy += MCUTICK - t_busy0;
                }
            }
        }
        IQ_BUSY_SUM += busy;
        IQ_POLL_N += poll;
    }

    IQ_WORDS += total;
    IQ_EVENTS += 1;
    if (total == 0)
        IQ_EOF_GENERATION = IQ_GENERATION;
    /* v61r: per-send wire stamp (v59a, LogEventSendVarLen-proven):
     * rxd.len (u16 at len_out-0x18) = total*4 + 60 so the wire carries the
     * full copy; driver avail comes out exactly `total` words. */
    *(volatile unsigned short *)((char *)len_out - 0x18) =
        (unsigned short)(total * 4 + 60);
    *wf_out = start;
    *smpl_out = total;             /* words in this event */
    *bank_out = 0;
    *len_out = total;              /* data_len (words) - builder loops while nonzero */
    return 0;
}

/* v61s6-r2: the mid-stream silent-exit fix. The builder's real alloc-failure
 * branch is `bnez38 a0, 0x0095c804` at 0x0095c7cc; its fallthrough (a0==0)
 * enters the fail block at 0x0095c7ce, which logs twice and EXITS the
 * builder without a marker - one transient empty SwRfb free list kills the
 * stream. This hook replaces the fail block's first two instructions
 * (`movpi45 a0,0x17; movi55 a1,1` at 0x0095c7ce-0x95c7d1): on entry the
 * alloc result is 0 and a1 is dead (reloaded at 0x0095c7d4). It counts the
 * failure (0x020173bc). While an exporter mode 1..5 is active it waits about
 * 50 ticks (letting interrupt-driven reclaim return entries) and jumps back
 * to the loop head to re-run the allocation. The counter is diagnostic and
 * no longer a lifetime retry budget: the old cumulative 200 limit made a
 * healthy high-rate stream eventually exit without EOF after 200 transient
 * pool shortages. Outside exporter modes it re-executes the displaced pair
 * and resumes the stock fail path.
 * Clobbers only a1 (dead) and $ta (scratch; the stock fail path uses it).
 * No calls, so lp is untouched. The stock `bnez38` at 0x0095c7cc is
 * untouched: budget-exhausted behavior is identical to stock. */
__attribute__((naked)) void iq_alloc_fail_hook(void) {
    asm(
        "sethi $a1, #0x2017 \n"            /* a1 = 0x02017000             */
        "lwi   $ta, [$a1+#0x3bc] \n"       /* count failures              */
        "addi  $ta, $ta, #0x1 \n"
        "swi   $ta, [$a1+#0x3bc] \n"
        "lwi   $ta, [$a1+#0x340] \n"       /* IQ_MODE                     */
        "beqz  $ta, 1f \n"                 /* mode 0: stock fail path     */
        "slti  $ta, $ta, #0x6 \n"
        "beqz  $ta, 1f \n"                 /* mode >= 6: stock fail path  */
        "sethi $a1, #0x89020 \n"           /* a1 = 0x89020000             */
        "lwi   $ta, [$a1+#0x68] \n"        /* t0 = MCUTICK                */
        "2: \n"
        "sethi $a1, #0x89020 \n"
        "lwi   $a1, [$a1+#0x68] \n"        /* now                         */
        "sub45 $a1, $ta \n"                /* now - t0                    */
        "slti  $a1, $a1, #0x32 \n"         /* < 50 ticks?                 */
        "bnez  $a1, 2b \n"
        "movpi45 $a0, #0x17 \n"            /* displaced instruction 1     */
        "movi55 $a1, #0x1 \n"              /* displaced instruction 2     */
        "j     builder_alloc_site \n"      /* retry: loop head 0x0095c7c4 */
        "1: \n"
        "movpi45 $a0, #0x17 \n"            /* displaced instruction 1     */
        "movi55 $a1, #0x1 \n"              /* displaced instruction 2     */
        "ret5  $lp \n"                     /* return to 0x0095c7d2        */
    );
}

/* The JalPatch placement stubs live in iq_hooks.c (built at -O3): at -Os
 * gcc appends ret5 to naked functions, which would overwrite the firmware
 * instruction after each 4-byte hook. */

/* v58e pacing history (superseded by the runtime-tunable IQ_PACE):
 * the static 350-unit reduction was only ever built into the v58e image
 * (95cfbebd...); the v60 deployment kept the stock 500 (verified byte
 * 0x440001f4 at file offset 0x47888). v58e measured 190K words/s with a
 * replay signature - but with the pre-burst-copy reader, so its failure is
 * not evidence against 350 pacing with the v59b copy. The sweep decides. *//* v59a: the builder's event allocation request 0x43c -> 0x860. The alloc
 * flows to FUN_00941416(wire_size = request + 0x18) which allocates the
 * wire buffer from PSE packet memory. IF the PSE honors the size, the wire
 * buffer is 0x878 and iq_read stamps rxd.len = words*4+60 so the wire
 * carries the full 512-word payload. IF the PSE caps it, the delivered
 * tail words are garbage -> revert (v58 overrun lesson). */
