/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA memory map.
 * Code + constants execute in place from XIP (app.bin offset 0 = 0x02000120).
 * 0x01C00000..0x01C06000 holds .ram_text (code that must run while the flash
 * is busy: the flash driver); 0x01C06000..0x01C08000 stays for RAM-run debug.
 * RAM  : .data/.bss (96 KiB)
 * POOL : big zero-initialised buffers (FX lines, canvases), section .pool
 * STACKS with 256-byte guard bands; NOINIT survives resets and UBOOT entry.
 * 0x01C7FD50.. is reserved: boot_info, boot-state mailbox, vector table. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAMTEXT (rwx): ORIGIN = 0x01C00000, LENGTH = 0x6000
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 96K
    POOL   (rw)  : ORIGIN = 0x01C20000, LENGTH = 0x54000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
}
SECTIONS
{
    .text : { KEEP(*(.text.crt0)) *(.text*) *(.rodata*) . = ALIGN(4); _etext = .; } > XIP
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4); _rt_end = .; } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    .ble_trace (NOLOAD) : { . = ALIGN(4); _ble_trace_start = .; *(.ble_trace) _ble_trace_end = .; } > RAMTEXT
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(EXCLUDE_FILE (*nimble_*.o) .bss* .sbss*) *(COMMON) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) *nimble_*.o(.bss* .sbss*) . = ALIGN(4); } > POOL
    .ble_dma (NOLOAD) : { _ble_dma_start = .; *(.ble_dma) . = ALIGN(4); _ble_dma_end = .; _pool_end = .; } > POOL
    ASSERT(_ble_trace_end <= ORIGIN(RAMTEXT) + LENGTH(RAMTEXT), "BLE trace exceeds RAMTEXT spare space")
    ASSERT(_ble_dma_start >= 0x01C20000 && _ble_dma_end <= 0x01C74000, "BLE DMA outside verified SRAM class")
    .noinit (NOLOAD) : { *(.noinit*) } > NOINIT
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
