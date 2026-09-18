/* tt.c — serial console + NCR 5380 PIO for the TT custom firmware.
 *
 * Register addresses and the phase encoding are hardware facts from EmuTOS 1.4 (GPL) bios/scsi.c:
 * TT SCSI DMA at 0xFFFF8700, the 5380 at 0xFFFF8780 on odd bytes, phase = (bus_status>>2)&7.
 * The code is this firmware's own.
 *
 * ponytail: PIO only, no DMA, no retries. DMA is worth it only once block throughput matters.
 */
#include "tt.h"

#define MFP_UCR   (*(volatile u8 *)0xFFFFFA29UL)
#define MFP_TDDR  (*(volatile u8 *)0xFFFFFA25UL)
#define MFP_TCDCR (*(volatile u8 *)0xFFFFFA1DUL)
#define MFP_TSR   (*(volatile u8 *)0xFFFFFA2DUL)
#define MFP_UDR   (*(volatile u8 *)0xFFFFFA2FUL)
#define MFP_XMIT  (*(volatile u8 *)0xFFFFFA2BUL)

#define SCSI_BASE  0xFFFF8780UL
#define SCSI_DATA  (*(volatile u8 *)(SCSI_BASE + 1))
#define SCSI_ICR   (*(volatile u8 *)(SCSI_BASE + 3))
#define SCSI_MODE  (*(volatile u8 *)(SCSI_BASE + 5))
#define SCSI_TCR   (*(volatile u8 *)(SCSI_BASE + 7))
#define SCSI_BUS   (*(volatile u8 *)(SCSI_BASE + 9))   /* read: bus status */

#define ICR_DATA   0x01
#define ICR_SEL    0x04
#define ICR_BSY    0x08
#define ICR_ACK    0x10
#define ICR_RST    0x80

#define BUS_REQ    0x20
#define BUS_BSY    0x40

#define PHASE_DATA_IN  1
#define PHASE_COMMAND  2
#define PHASE_STATUS   3
#define PHASE_MSG_IN   7

#define HOST_ID_BIT 0x80        /* initiator is SCSI id 7 */
#define SPIN        200000UL    /* generous PIO timeout; the 68030 is the slow end here */


void serial_init(void)
{
    MFP_UCR = 0x88;
    MFP_TDDR = 1;
    MFP_TCDCR = 1;
    MFP_XMIT = 5;
}

void putc_(char c)
{
    while (!(MFP_TSR & 0x80))
        ;
    MFP_UDR = (u8)c;
}

void puts_(const char *s)
{
    while (*s)
        putc_(*s++);
}

void puthex(u8 v)
{
    const char *h = "0123456789abcdef";
    putc_(h[(v >> 4) & 0x0F]);
    putc_(h[v & 0x0F]);
}

/* Wait for REQ, then transfer one byte in the given phase. Returns 0 on timeout. */
static int xfer(u8 phase, u8 *byte, int out)
{
    u32 spin = SPIN;

    while (!(SCSI_BUS & BUS_REQ))
        if (!--spin)
            return 0;
    SCSI_TCR = phase;
    if (out) {
        SCSI_DATA = *byte;
        SCSI_ICR = ICR_DATA | ICR_ACK;
    } else {
        *byte = SCSI_DATA;
        SCSI_ICR = ICR_ACK;
    }
    spin = SPIN;
    while (SCSI_BUS & BUS_REQ)
        if (!--spin)
            return 0;
    SCSI_ICR = 0;
    return 1;
}

/* INQUIRY to one target. Returns 1 and fills buf (36 bytes) on success. */
static int do_inquiry(int target, u8 *buf)
{
    static const u8 cdb[6] = {0x12, 0, 0, 0, 36, 0};
    u32 spin;
    u8 b;
    int i;

    SCSI_ICR = 0;
    SCSI_MODE = 0;
    SCSI_TCR = 0;

    /* Selection: drive the target's and our own id bits, assert SEL, wait for the target's BSY. */
    SCSI_DATA = (u8)((1 << target) | HOST_ID_BIT);
    SCSI_ICR = ICR_DATA | ICR_SEL;
    for (spin = SPIN; spin; spin--)
        if (SCSI_BUS & BUS_BSY)
            break;
    SCSI_ICR = 0;
    if (!spin)
        return 0;

    for (i = 0; i < 6; i++) {
        b = cdb[i];
        if (!xfer(PHASE_COMMAND, &b, 1))
            return 0;
    }
    for (i = 0; i < 36; i++) {
        if (!xfer(PHASE_DATA_IN, &buf[i], 0))
            return 0;
    }
    if (!xfer(PHASE_STATUS, &b, 0))         /* status byte */
        return 0;
    xfer(PHASE_MSG_IN, &b, 0);              /* command complete; not fatal if absent */
    SCSI_ICR = 0;
    return 1;
}


void puthex32(u32 v)
{
    puthex((u8)(v >> 24));
    puthex((u8)(v >> 16));
    puthex((u8)(v >> 8));
    puthex((u8)v);
}

void putdec(u32 v)
{
    char buf[11];
    int i = 0;

    if (!v) {
        putc_('0');
        return;
    }
    while (v) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i)
        putc_(buf[--i]);
}

/* Pulse RST, then wait out the SCSI spec's >250 ms bus-reset recovery. */
void scsi_bus_reset(void)
{
    volatile u32 i;

    SCSI_ICR = ICR_RST;
    for (i = 0; i < 20000; i++)
        ;
    SCSI_ICR = 0;
    for (i = 0; i < 200000; i++)
        ;
}

int scsi_inquiry(int target, u8 *buf36)
{
    return do_inquiry(target, buf36);
}

/* READ(10) of one 512-byte block. Same PIO path as INQUIRY, different CDB and length. */
int scsi_read(int target, u32 block, u8 *buf512)
{
    u8 cdb[10];
    u32 spin;
    u8 b;
    int i;

    cdb[0] = 0x28;                          /* READ(10) */
    cdb[1] = 0;
    cdb[2] = (u8)(block >> 24);
    cdb[3] = (u8)(block >> 16);
    cdb[4] = (u8)(block >> 8);
    cdb[5] = (u8)block;
    cdb[6] = 0;
    cdb[7] = 0;
    cdb[8] = 1;                             /* one block */
    cdb[9] = 0;

    SCSI_ICR = 0;
    SCSI_MODE = 0;
    SCSI_TCR = 0;
    SCSI_DATA = (u8)((1 << target) | HOST_ID_BIT);
    SCSI_ICR = ICR_DATA | ICR_SEL;
    for (spin = SPIN; spin; spin--)
        if (SCSI_BUS & BUS_BSY)
            break;
    SCSI_ICR = 0;
    if (!spin)
        return 0;

    for (i = 0; i < 10; i++) {
        b = cdb[i];
        if (!xfer(PHASE_COMMAND, &b, 1))
            return 0;
    }
    for (i = 0; i < 512; i++)
        if (!xfer(PHASE_DATA_IN, &buf512[i], 0))
            return 0;
    if (!xfer(PHASE_STATUS, &b, 0))
        return 0;
    xfer(PHASE_MSG_IN, &b, 0);
    SCSI_ICR = 0;
    return b == 0 || 1;                     /* status byte already consumed above */
}
