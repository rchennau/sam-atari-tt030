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

#include "tt.h"
void cmain(void)
{
    u8 buf[36];
    int target, i, found = 0;

    serial_init();
    puts_("SAM-TT firmware rom3\r\n");

    /* A missing 5380 bus-errors on the first register touch; resume at the report instead. */
    bus_error_jump = (u32)&&no_chip;   /* GCC label address */

    scsi_bus_reset();

    for (target = 0; target < 7; target++) {
        for (i = 0; i < 36; i++)
            buf[i] = 0;
        if (!scsi_inquiry(target, buf))
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
