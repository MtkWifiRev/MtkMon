/* iq_live: turn the firmware's RBIST dump-event builder into the I/Q
 * stream engine.
 *
 * icapSpectrumDumpDataEvent (0x0095c7a0) loops: alloc an event ->
 * halRbistQueryIQData fills data + data_len -> send -> delay -> repeat while
 * data_len != 0. JalPatch replaces its query call (`jal 0x0096b9d4` at
 * 0x0095c864) with iq_read below, same ABI (12 args, 6 in registers and 6 on
 * the stack). The DLM mode word selects what iq_read does; the WM crt zeroes
 * DLM at boot, so mode 0 (stock) is the default:
 *
 *   0x02017340 IQ_MODE  0 = stock: call the firmware's query
 *                       3 = stop: end marker (data_len 0) without the
 *                           stock reader
 *                       6 = stream IQB2 blocks through MCU-WFDMA TX0 until
 *                           IQ_MODE changes (iq_dma_stream, below)
 *   0x020173a8 IQ_GENERATION      host-written stream generation
 *   0x020173ac IQ_EOF_GENERATION  = IQ_GENERATION once the builder has
 *                                 produced its end marker
 *
 * Every other value of IQ_MODE behaves like mode 0. Up to v65 this file also
 * held the polling event exporter of v57-v61 (modes 1 and 2, its DLM tuning
 * words 0x02017344-0x020173c0 and four more hooks at 0x0095c7c4, 0x0095c7ce,
 * 0x0095c888 and 0x00946ea6). The driver never used them, so v66 drops them;
 * see the v65 sources for that history.
 *
 * Built with the project's -O3 like everything else. The old patched
 * toolchain needed this file at -Os and its JalPatch stub in a separate
 * translation unit (it appended ret5 to naked functions at -Os and dropped
 * the return of frameless leaf functions at -O1 and above); the unmodified
 * toolchain does neither.
 */
#pragma NEXMON targetregion "patch"
#include <firmware_version.h>
#include <patcher.h>
#include <nds32_intrinsic.h>

/* same macro set wrapper.c uses (this TU does not include wrapper.h) */
#ifndef WRAPPER_H
    #define VOID_DUMMY { }
    #define RETURN_DUMMY { }
    #define AT(CHIPVER, FWVER, ADDR) __attribute__((naked, at(ADDR, "dummy", CHIPVER, FWVER)))
#endif

#define IQ_MODE    (*(volatile unsigned int *)0x02017340)
#define IQ_GENERATION (*(volatile unsigned int *)0x020173a8)
#define IQ_EOF_GENERATION (*(volatile unsigned int *)0x020173ac)

#define MCUTICK    (*(volatile unsigned int *)0x89020068) /* free-run counter, FUN_0080ce6c(4) */
#define RBIST_WPTR (*(volatile unsigned int *)0x800210b4)

/* The query the builder calls (hooked at 0x0095c864), for mode 0.
 * noinline, noclone and the empty asm keep gcc from inlining or
 * const-folding this dummy body: an early build inlined it, so mode 0
 * returned 0 without ever calling the firmware's function. */
__attribute__((at(0x0096b9d4, "dummy", CHIP_VER_MT7961, FW_VER_MTK_20260224), noinline, noclone, used))
int
halRbistQueryIQData_orig(void *a0, void *a1, void *a2, void *a3, void *a4,
                         unsigned a5, unsigned a6, unsigned a7, unsigned a8,
                         unsigned a9, unsigned a10, unsigned a11)
{
    asm volatile("");
    return 0;
}

/* End of the builder loop: no data, data_len 0. */
static __attribute__((noinline)) int
iq_end(unsigned *wf_out, unsigned *smpl_out, unsigned *bank_out,
       unsigned *len_out)
{
    *wf_out = 0;
    *smpl_out = 0;
    *bank_out = 0;
    *len_out = 0;
    IQ_EOF_GENERATION = IQ_GENERATION;
    return 0;
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

/* Keeps the helpers out of line, which keeps the patch small. */
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
        __nds32__dsb();
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
        __nds32__dsb();
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
    return iq_end(wf_out, smpl_out, bank_out, len_out);
}

int
iq_read(unsigned *wf_out, unsigned *smpl_out, unsigned *bank_out,
        unsigned *len_out, unsigned *data_out,
        unsigned band, unsigned phy, unsigned node, unsigned caplen,
        unsigned z1, unsigned z2, unsigned transport)
{
    unsigned mode = IQ_MODE;

    if (mode == 6)
        return iq_dma_stream(wf_out, smpl_out, bank_out, len_out);
    if (mode == 3)
        return iq_end(wf_out, smpl_out, bank_out, len_out);
    return halRbistQueryIQData_orig(wf_out, smpl_out, bank_out, len_out,
                                    data_out, band, phy, node, caplen,
                                    z1, z2, transport);
}

/* Replace the builder's call `jal 0x0096b9d4` at 0x0095c864. */
__attribute__((at(0x0095c864, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(iq_hook, iq_read);
