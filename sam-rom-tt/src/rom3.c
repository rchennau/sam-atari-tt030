/* rom3 — fourth custom-firmware milestone: talk to the BlueSCSI from ROM.
 *
 * Sends a SCSI INQUIRY to each target with the TT's NCR 5380 in PIO mode and prints what answers,
 * over the MFP serial port. No TOS, no driver, no DMA: this is the step that proves the firmware
 * can reach the disk it will later boot from.
 *
 * Register addresses and the phase encoding come from EmuTOS 1.4 (GPL) bios/scsi.c — hardware facts,
 * not code: TT SCSI DMA at 0xFFFF8700, the 5380 at 0xFFFF8780 on odd bytes, phase = (bus_status>>2)&7.
 * Everything below is written for this firmware.
 *
 * Build: make rom3    Run: make hatari3   (Hatari needs --scsi 0=<image>, which the target passes)
 *
 * ponytail: PIO only, one command, no retries and no DMA — enough to answer "is the disk there and
 * who is it". Block reads and error recovery come with the FAT16 step.
 */

typedef unsigned char u8;
typedef unsigned long u32;

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

/* start3.S reads this low-RAM cell in its bus-error handler; 0 = halt. Not a variable in .data,
 * which would sit in ROM and be unwritable. */
#define bus_error_jump (*(volatile u32 *)0x00000404UL)

static void serial_init(void)
{
    MFP_UCR = 0x88;
    MFP_TDDR = 1;
    MFP_TCDCR = 1;
    MFP_XMIT = 5;
}

static void putc_(char c)
{
    while (!(MFP_TSR & 0x80))
        ;
    MFP_UDR = (u8)c;
}

static void puts_(const char *s)
{
    while (*s)
        putc_(*s++);
}

static void puthex(u8 v)
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
static int inquiry(int target, u8 *buf)
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

void cmain(void)
{
    u8 buf[36];
    int target, i, found = 0;

    serial_init();
    puts_("SAM-TT firmware rom3\r\n");

    /* A missing 5380 bus-errors on the first register touch; resume at the report instead. */
    bus_error_jump = (u32)&&no_chip;   /* GCC label address */

    SCSI_ICR = ICR_RST;
    for (i = 0; i < 20000; i++)
        ;
    SCSI_ICR = 0;
    for (i = 0; i < 200000; i++)            /* SCSI spec: >250 ms bus-reset recovery */
        ;

    for (target = 0; target < 7; target++) {
        for (i = 0; i < 36; i++)
            buf[i] = 0;
        if (!inquiry(target, buf))
            continue;
        found++;
        puts_("target ");
        putc_((char)('0' + target));
        puts_("  type ");
        puthex(buf[0]);
        puts_("  ");
        for (i = 8; i < 32; i++)            /* vendor + product, space padded */
            putc_(buf[i] >= 0x20 && buf[i] < 0x7F ? (char)buf[i] : ' ');
        puts_("\r\n");
    }
    bus_error_jump = 0;

    if (!found)
        puts_("no SCSI targets answered\r\n");
    puts_("scsi scan done\r\n");
    return;

no_chip:
    bus_error_jump = 0;
    puts_("no 5380 at 0xffff8780 (bus error)\r\n");
}
