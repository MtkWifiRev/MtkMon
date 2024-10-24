#pragma NEXMON targetregion "patch"

#include <firmware_version.h>
#include <patcher.h>
#include <wrapper.h>
//#include <helper.h>

#define HEXDUMP_COLS 16

struct dump_pkt_struct {
    unsigned int i1; // at 0x0
    unsigned int i2;
    unsigned int i3;
    unsigned int i4;
    unsigned int i5;
    unsigned int i6;
    unsigned int i7;
    unsigned int i8;
    unsigned int i9; // at 32
    unsigned int i10;
    unsigned int i11;
    unsigned int i12;
    unsigned int i13;
    unsigned int i14;
    unsigned int i15;
    unsigned int i16; // at 64
    unsigned int cur_addr;
    unsigned int len;
};

struct param_struct {
    unsigned int i1;
    unsigned int i2;
    unsigned int i3;
    struct dump_pkt_struct *ptr;
};

// -------------------------------------------

struct pkt_struct {
    unsigned int i1;
    unsigned int i2;
    unsigned int i3;
    unsigned int data;
};

struct mem_dump_struct {
    unsigned int i1;
    unsigned int i2;
    unsigned int i3;
    unsigned int i4;
    unsigned int i5;
    unsigned int i6;
    unsigned int i7;
    unsigned int cur_addr;
    unsigned int lenght;
    unsigned int remain_lenght;
};

void inline
hexdump(void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char *) addr;

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0) {
                unsigned char *some_log_flag = (unsigned char *) 0xf021faef;
                *some_log_flag = 1;
                printf("  %s\n", buff);
            }

            // Output the offset.
            unsigned char *some_log_flag = (unsigned char *) 0xf021faef;
            *some_log_flag = 1;
            printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        unsigned char *some_log_flag = (unsigned char *) 0xf021faef;
        *some_log_flag = 1;
        printf(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        unsigned char *some_log_flag = (unsigned char *) 0xf021faef;
        *some_log_flag = 1;
        printf("   ");
        i++;
    }

    // And print the final ASCII bit.
    unsigned char *some_log_flag = (unsigned char *) 0xf021faef;
    *some_log_flag = 1;
    printf("  %s\n", buff);
}

/*
void my_patch3(struct pkt_struct *pkt) {
    struct mem_dump_struct *pkt_data = (struct mem_dump_struct *) pkt->data;

    unsigned char *some_log_flag = (unsigned char *) 0xf021faef;
    *some_log_flag = 1;
    printf("cmd_mem_dump_maybe() #2, cur_addr 0x%08x\n", pkt_data->cur_addr);
    return FUN_f01b2260((void *) pkt);
}

// Overwrite jump to print in mem_dump function2
__attribute__((at(0xf019b34a, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xf0177000")))
JalPatch(my_patch3, my_patch3);
*/

__attribute__((at(0xf019b268, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xf0177000")))
int overwrite_cmd_mem_dump(struct param_struct *param_1) {
    struct dump_pkt_struct *dump_s = param_1->ptr;
    
    unsigned char *some_log_flag = (unsigned char *) 0xf021faef;
    *some_log_flag = 1;
    //printf("cmd_mem_dump_maybe(), cur_addr: 0x%08x, len: 0x%08x\n", dump_s->cur_addr, dump_s->len);
    printf("cmd_mem_dump_maybe(), cur_addr: 0x%08x, len: 0x%08x\n", dump_s->cur_addr, dump_s->len);

    for(int i=0; i<dump_s->len; i++) {
        unsigned char *some_log_flag = (unsigned char *) 0xf021faef;
        *some_log_flag = 1;
        printf("    contents: %02X\n",
            ((unsigned char *)dump_s->cur_addr)[i]);
    }
    //printf("cmd_mem_dump_maybe()\n");
    //hexdump((void *) &(dump_s->cur_addr), dump_s->len);
    return 0;
}

/*
//__attribute__((at(0xf019b2ac, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xf0177000")))
__attribute__((at(0xf019b294, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xf0177000")))
__attribute__((naked))
void overwrite_jump(void) {
    asm(
        "j8 LAB_f019b2c8\n"
    );
}
*/

/*
__attribute__((at(0xf019b2c4, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xf0177000")))
__attribute__((naked))
void nop_assignment(void) {
    asm(
        "nop16\n"
        "nop16\n"
    );
}
*/

/*
__attribute__((naked))
void my_log_patch(int a1, int a2) {
    asm (
        "smw.adm s0, [sp], s3,0x2\n"
        "addi10.sp -0xc\n"
        "movi55 a0, 0x1\n"
        "sethi ta, 0xf021f\n"
        "sbi a0, [ta + 0xaef]\n"
        "addi10.sp 0xc\n"
        "lmw.bim s0,[sp],s3,0x2\n"
        "ret5 lp\n"
    );
}

__attribute__((at(0xf019af08, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xf0177000")))
JalPatch(my_log_patch, my_log_patch);
*/

void my_set_fw_op_mode(void *a1) {
    unsigned char *some_log_flag = (unsigned char *) 0xf021faef;
    *some_log_flag = 1;
    //some_logging_related_function(0x24, 4);
    
    unsigned int gp_value;
    //gp_value = __builtin_nds32_mfusr(0);
    asm("mov55 %0, $gp" : "=r" (gp_value));
    
    //printf("MTKMON :: set_fw_op_mode()\n");
    printf("MTKMON :: set_fw_op_mode(), gp_value: 0x%x\n", gp_value);
    set_fw_op_mode(a1);
}


__attribute__((at(0xf01cfcac, "", CHIP_VER_MTK, FW_VER_MTK, "out_3_0xf0177000")))
JalPatch(my_set_fw_op_mode, my_set_fw_op_mode);
