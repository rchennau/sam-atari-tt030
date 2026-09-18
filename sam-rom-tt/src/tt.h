/* tt.h — shared firmware helpers: serial console and SCSI PIO. */
#ifndef TT_H
#define TT_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

/* start3.S reads this low-RAM cell in its bus-error handler; 0 = halt. Not a .data variable,
 * which would sit in unwritable ROM. */
#define bus_error_jump (*(volatile u32 *)0x00000404UL)

/* Writable storage must be at a fixed low-RAM address: the ROM is linked as one .text blob, so any
 * static/global object would land in ROM and bus-error on the first write (measured 2026-09-18 —
 * "Bus Error writing at address $e00fd8"). The stack lives at 0x800 and grows down. */
#define RAM_BUF  ((volatile u8 *)0x00001000UL)   /* 1 KB scratch: sector buffers etc. */

void serial_init(void);
void putc_(char c);
void puts_(const char *s);
void puthex(u8 v);
void puthex32(u32 v);
void putdec(u32 v);

void scsi_bus_reset(void);
int scsi_inquiry(int target, u8 *buf36);
int scsi_read(int target, u32 block, u8 *buf512);   /* READ(10), one 512-byte block */

#endif
