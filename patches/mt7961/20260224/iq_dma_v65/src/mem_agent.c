/* mem_agent: MCU-privileged arbitrary memory read/write via a DLM mailbox.
 * Host writes params + trigger via regidx/regval; the dispatcher hook polls and
 * executes; host reads the result from MB_VAL. Reads/writes ANY address the MCU
 * can reach (SYSRAM, MCU regs, code RAM reads) - bypasses regidx write limits.
 *
 * Mailbox: 0x02017300-0x0201730b, in region-1 DLM. Although the IMAGE has the
 * string "wsysIsrProcessLmacIST" there, the WM crt zeroes that range at boot
 * (runtime reads 0; same for 0x020173c8). Validated live on 20260224: marker
 * words written via regidx/regval at 0x02017300/04/08 survive association +
 * scan + traffic, so the firmware does not use them. Each firmware reload
 * re-zeroes the range (crt .bss clear), so the mailbox always starts idle.
 *
 * The trampoline calls this function; gcc 8.0.1-nds32 -O2/-O3 does NOT emit a
 * return instruction for a leaf function with a fully merged epilogue (the
 * code just falls off the end - straight into whatever the linker places
 * next, which is mem_agent_tramp => infinite self-recursion, $sp runs away,
 * WM dead at the first MCU command - that is exactly the "Message 0004008a
 * (seq 11) timeout" failure of the first mem_agent build). The trailing
 * asm ret5 forces a real return on every path. Verified in gen/patch.elf:
 * every path ends in ret5 $lp. */
#pragma NEXMON targetregion "patch"
#include <firmware_version.h>
#include <patcher.h>

#define MB_TRIG (*(volatile unsigned int *)0x02017300)  /* 0 idle;1 read;2 write */
#define MB_ADDR (*(volatile unsigned int *)0x02017304)
#define MB_VAL  (*(volatile unsigned int *)0x02017308)  /* write value / read result */

void mem_agent(void) {
    unsigned int op = MB_TRIG;
    if (op == 1) MB_VAL = *(volatile unsigned int *)MB_ADDR;       /* read  */
    else if (op == 2) *(volatile unsigned int *)MB_ADDR = MB_VAL;  /* write */
    MB_TRIG = 0;
    asm volatile("ret5 $lp");
    /* NOTE: no "else return;" above - adding it makes this gcc miscompile the
     * idle path into the read path (beqc/bnec fusion bug, seen at -O3). */
}

__attribute__((naked)) void mem_agent_tramp(void) {
    asm(
        "addi $sp, $sp, -8   \n"
        "swi  $r0, [$sp + 0] \n"
        "swi  $lp, [$sp + 4] \n"
        "jal  mem_agent      \n"
        "lwi  $r0, [$sp + 0] \n"
        "lwi  $lp, [$sp + 4] \n"
        "addi $sp, $sp, 8    \n"
        "addi $r6, $r0, 0x20 \n"
        "ret  $lp            \n");
}
__attribute__((at(0x00920dc2, "", CHIP_VER_MT7961, FW_VER_MTK_20260224, "out_0_0x915000")))
JalPatch(mem_hook, mem_agent_tramp);
