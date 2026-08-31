/****************************************************************************
 * boards/microchip/czca90curiosity/nuttx-config/scripts/script.ld
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/* Linker script for PIC32CZ CA80/CA90 (Cortex-M7):
 *   - 128 KB Boot Flash Memory (BFM) at 0x08000000 — vector table lives here
 *   - 8 MB Program Flash Memory (PFM) at 0x0C000000 — code and rodata
 *   - 128 KB DTCM at 0x20000000 (tightly coupled, kept disabled)
 *   - 1024 KB FLEXRAM at 0x20020000 (DFP ATDF: size=0x100000, XC32 LD: RAM_LENGTH=0x100000)
 *
 * Boot sequence (verified from Harmony PIC32CZ8110CA80208.ld):
 *   BootROM (0x04000000) reads SP from [0x08000000] (BFM[0])
 *                        reads PC from [0x08000004] (BFM[1] → __start in PFM)
 *   VTOR is set to 0x08000000 (BFM start) by BootROM, then re-affirmed in __start.
 *   VECTOR_REGION = boot_rom (matches Harmony default when boot_rom region present).
 *
 * Memory layout (1024 KB FLEXRAM = 960 KB general + 64 KB nocache):
 *   sram    : 960 KB at 0x20020000 - 0x2010FFFF (heap + stack)
 *   nocache : 64 KB  at 0x20110000 - 0x2011FFFF (DMA buffers, uncached)
 *
 * CA80 vs CA90: identical memory map; CA90 adds HSM (Hardware Security Module).
 */

MEMORY
{
  boot_rom (rx) : ORIGIN = 0x08000000, LENGTH = 128K  /* BFM — vector table */
  flash (rx)    : ORIGIN = 0x0C000000, LENGTH = 8M    /* PFM — code          */
  sram (rwx)    : ORIGIN = 0x20020000, LENGTH = 960K
  nocache (rwx) : ORIGIN = 0x20110000, LENGTH = 64K
}

OUTPUT_ARCH(arm)
EXTERN(_vectors)
ENTRY(__start)

SECTIONS
{
    /* Vector table: placed at BFM start (0x08000000).
     * BootROM reads [BFM+0]=SP and [BFM+4]=PC (= __start in PFM) before jumping.
     */
    .vectors : {
        KEEP(*(.vectors))
    } > boot_rom

    .text : {
        _stext = ABSOLUTE(.);
        *(.text .text.*)
        *(.fixup)
        *(.gnu.warning)
        *(.rodata .rodata.*)
        *(.gnu.linkonce.t.*)
        *(.glue_7)
        *(.glue_7t)
        *(.got)
        *(.gcc_except_table)
        *(.gnu.linkonce.r.*)
        _etext = ABSOLUTE(.);
    } > flash

    .init_section : {
        _sinit = ABSOLUTE(.);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        _einit = ABSOLUTE(.);
    } > flash

    .ARM.extab : {
        *(.ARM.extab*)
    } > flash

    __exidx_start = ABSOLUTE(.);
    .ARM.exidx : {
        *(.ARM.exidx*)
    } > flash
    __exidx_end = ABSOLUTE(.);

    _eronly = ABSOLUTE(.);

    .data : {
        _sdata = ABSOLUTE(.);
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        CONSTRUCTORS
        . = ALIGN(4);
        _edata = ABSOLUTE(.);
    } > sram AT > flash

    .ramfunc ALIGN(4): {
        _sramfuncs = ABSOLUTE(.);
        *(.ramfunc  .ramfunc.*)
        _eramfuncs = ABSOLUTE(.);
    } > sram AT > flash

    _framfuncs = LOADADDR(.ramfunc);

    .bss : {
        _sbss = ABSOLUTE(.);
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = ABSOLUTE(.);
    } > sram

    .nocache (NOLOAD) : {
        . = ALIGN(32);
        _s_nocache = ABSOLUTE(.);
        *(.nocache .nocache.*)
        . = ALIGN(32);
        _e_nocache = ABSOLUTE(.);
    } > nocache

    /* Stabs debugging sections */
    .stab 0 : { *(.stab) }
    .stabstr 0 : { *(.stabstr) }
    .stab.excl 0 : { *(.stab.excl) }
    .stab.exclstr 0 : { *(.stab.exclstr) }
    .stab.index 0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment 0 : { *(.comment) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_info 0 : { *(.debug_info) }
    .debug_line 0 : { *(.debug_line) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_aranges 0 : { *(.debug_aranges) }
}
