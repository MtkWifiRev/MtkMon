/***************************************************************************
 *                                                                         *
 *          ###########   ###########   ##########    ##########           *
 *         ############  ############  ############  ############          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ###########   ####  ######  ##   ##   ##  ##    ######          *
 *          ###########  ####  #       ##   ##   ##  ##    #    #          *
 *                   ##  ##    ######  ##   ##   ##  ##    #    #          *
 *                   ##  ##    #       ##   ##   ##  ##    #    #          *
 *         ############  ##### ######  ##   ##   ##  ##### ######          *
 *         ###########    ###########  ##   ##   ##   ##########           *
 *                                                                         *
 *            S E C U R E   M O B I L E   N E T W O R K I N G              *
 *                                                                         *
 * This file is part of NexMon.                                            *
 *                                                                         *
 * Copyright (c) 2016 NexMon Team                                          *
 *                                                                         *
 * NexMon is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation, either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * NexMon is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with NexMon. If not, see <http://www.gnu.org/licenses/>.          *
 *                                                                         *
 **************************************************************************/

#ifndef WRAPPER_C
#define WRAPPER_C

#include <firmware_version.h>
#include <structs.h>

#ifndef WRAPPER_H
    // if this file is not included in the wrapper.h file, create dummy functions
    #define VOID_DUMMY { ; }
    #define RETURN_DUMMY { ; return 0; }

    #define AT(CHIPVER, FWVER, ADDR) __attribute__((at(ADDR, "dummy", CHIPVER, FWVER)))
#else
    // if this file is included in the wrapper.h file, create prototypes
    #define VOID_DUMMY ;
    #define RETURN_DUMMY ;
    #define AT(CHIPVER, FWVER, ADDR)
#endif

AT(CHIP_VER_MTK, FW_VER_MTK, 0xf019ab9c)
void
print_to_logshell(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
VOID_DUMMY

AT(CHIP_VER_MTK, FW_VER_MTK, 0x822000)
int
print_related(void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
RETURN_DUMMY

AT(CHIP_VER_MTK, FW_VER_MTK, 0xf019ab9c)
void
printf(const char *format, ...)
VOID_DUMMY

AT(CHIP_VER_MTK, FW_VER_MTK, 0xf019af08)
void
some_logging_related_function(int a1, int a2)
VOID_DUMMY

AT(CHIP_VER_MTK, FW_VER_MTK, 0xf01cfc98)
int
set_rf_test_mode_cmd_handler(void *a1)
RETURN_DUMMY

/*
AT(CHIP_VER_MTK, FW_VER_MTK, 0xf019b268)
int
cmd_mem_dump_maybe(int a1)
RETURN_DUMMY
*/

AT(CHIP_VER_MTK, FW_VER_MTK, 0xf0197044)
void
set_fw_op_mode(void *a)
VOID_DUMMY

AT(CHIP_VER_MTK, FW_VER_MTK, 0xf0196278)
void *
wsysMemEventPktAlloc(void *a1)
RETURN_DUMMY

AT(CHIP_VER_MTK, FW_VER_MTK, 0xf01b2260)
void
FUN_f01b2260(void *a1)
VOID_DUMMY

AT(CHIP_VER_MTK, FW_VER_MTK, 0xf019b2c8)
void
LAB_f019b2c8(void)
VOID_DUMMY

#endif /*WRAPPER_C*/
