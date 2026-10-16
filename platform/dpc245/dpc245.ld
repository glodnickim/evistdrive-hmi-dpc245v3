/* SPDX-License-Identifier: GPL-3.0-or-later */
ENTRY(Reset_Handler)

/* DPC245 V3 flash layout:
 *   0x08000000..0x08005FFF  resident bootloader
 *   0x08006000..0x0803FFFD  application code/data region
 *   0x0803FFFE..0x0803FFFF  bootable application marker, value 0xAA55
 *   0x08040000..0x0807FFFF  area after the application; the installation file
 *                           writes it blank (0xFF) and the application never reads it
 *
 * The application firmware keeps executable/loadable data below the marker.
 * The ASSETS region is reserved for future project resources; it is empty and
 * the installation tool rejects any non-blank byte there.
 */
MEMORY
{
  APP     (rx)  : ORIGIN = 0x08006000, LENGTH = 0x00039FFE
  MARKER  (rx)  : ORIGIN = 0x0803FFFE, LENGTH = 2
  ASSETS  (rx)  : ORIGIN = 0x08040000, LENGTH = 0x00040000
  /* 64 KiB: the SRAM window used by this application.
   * The 96 KiB of the GD32F303VG candidate is inferred, not measured
   * for this application. */
  RAM     (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > APP

  .text :
  {
    . = ALIGN(4);
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
  } > APP

  .ARM.exidx :
  {
    *(.ARM.exidx*)
  } > APP

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > APP
  _sidata = LOADADDR(.data);

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /* The resident bootloader requires little-endian 0xAA55 here. */
  .app_boot_marker :
  {
    KEEP(*(.app_boot_marker))
  } > MARKER

  /* Optional resources can be placed here explicitly in future builds. */
  .assets :
  {
    KEEP(*(.assets*))
  } > ASSETS

  _end = .;
}

ASSERT(SIZEOF(.app_boot_marker) == 2, "DPC245 app marker must be exactly 2 bytes")
