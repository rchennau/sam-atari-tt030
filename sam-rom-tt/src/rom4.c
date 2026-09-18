/* rom4 — fifth custom-firmware milestone: read the BlueSCSI and list a partition's files.
 *
 * Reads the Atari root sector, prints the partition table, then for the first bootable/valid
 * partition parses the FAT16 BPB and lists the root directory over serial. That is everything the
 * firmware needs before it can load and run a file.
 *
 * Atari specifics this had to get right, all visible in the card image:
 *   - the partition table is four 12-byte entries at offset 0x1C6: flag, 3-char id ("BGM"/"GEM"),
 *     start and size as big-endian longs in 512-byte units;
 *   - the partition's own BPB uses a LOGICAL sector size that is not 512 (8192 on this card), so
 *     every FAT/directory offset must be scaled to physical blocks before a READ(10).
 *
 * Build: make rom4    Run: make hatari4
 * ponytail: root directory only, no subdirectories and no FAT chain walk yet — the next step needs
 * the chain to load a file, and that is where the cluster arithmetic gets exercised.
 */
#include "tt.h"

#define TARGET 0

#define sec ((u8 *)RAM_BUF)   /* 512-byte sector buffer in RAM, not ROM */

static u32 be32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static u16 le16(const u8 *p)
{
    return (u16)(p[0] | (p[1] << 8));
}

static u32 le32(const u8 *p)
{
    return ((u32)p[3] << 24) | ((u32)p[2] << 16) | ((u32)p[1] << 8) | p[0];
}

static void print_name(const u8 *e)
{
    int i;

    for (i = 0; i < 8 && e[i] != ' '; i++)
        putc_((char)e[i]);
    if (e[8] != ' ') {
        putc_('.');
        for (i = 8; i < 11 && e[i] != ' '; i++)
            putc_((char)e[i]);
    }
}

void cmain(void)
{
    u32 part_start = 0, lsec, fat_start, root_start, i;
    u16 bps, nroot, fatsz, res;
    u8 spc, nfats, id[4];
    int p, e, files = 0, dirs = 0;

    serial_init();
    puts_("SAM-TT firmware rom4\r\n");
    bus_error_jump = (u32)&&no_chip;
    scsi_bus_reset();

    if (!scsi_read(TARGET, 0, sec)) {
        puts_("root sector read failed\r\n");
        goto done;
    }

    for (p = 0; p < 4; p++) {
        const u8 *ent = sec + 0x1C6 + 12 * p;
        if (!(ent[0] & 1))                  /* bit 0 = partition exists */
            continue;
        id[0] = ent[1]; id[1] = ent[2]; id[2] = ent[3]; id[3] = 0;
        puts_("part ");
        putc_((char)('C' + p));
        puts_("  ");
        puts_((const char *)id);
        puts_("  start ");
        putdec(be32(ent + 4));
        puts_("  size ");
        putdec(be32(ent + 8) / 2048);
        puts_(" MB");
        if (ent[0] & 0x80)
            puts_("  (bootable)");
        puts_("\r\n");
        if (!part_start)
            part_start = be32(ent + 4);
    }
    if (!part_start) {
        puts_("no partitions\r\n");
        goto done;
    }

    if (!scsi_read(TARGET, part_start, sec)) {
        puts_("bpb read failed\r\n");
        goto done;
    }
    bps = le16(sec + 11);
    spc = sec[13];
    res = le16(sec + 14);
    nfats = sec[16];
    nroot = le16(sec + 17);
    fatsz = le16(sec + 22);
    lsec = bps / 512;                       /* physical blocks per logical sector */

    puts_("bpb    bytes/sec ");
    putdec(bps);
    puts_("  sec/clus ");
    putdec(spc);
    puts_("  fats ");
    putdec(nfats);
    puts_("  rootents ");
    putdec(nroot);
    puts_("\r\n");
    if (!lsec) {
        puts_("bpb looks wrong (sector size < 512)\r\n");
        goto done;
    }

    fat_start = part_start + (u32)res * lsec;
    root_start = fat_start + (u32)nfats * fatsz * lsec;

    puts_("root dir at block ");
    putdec(root_start);
    puts_("\r\n");

    for (i = 0; i < ((u32)nroot * 32 + 511) / 512; i++) {
        if (!scsi_read(TARGET, root_start + i, sec)) {
            puts_("root read failed\r\n");
            goto done;
        }
        for (e = 0; e < 512; e += 32) {
            u8 *d = sec + e;
            if (d[0] == 0x00)
                goto listed;
            if (d[0] == 0xE5 || (d[11] & 0x08) || d[11] == 0x0F)
                continue;                   /* deleted, volume label, long-name */
            if (d[11] & 0x10) {
                dirs++;
                puts_("  <dir> ");
                print_name(d);
                puts_("\r\n");
                continue;
            }
            files++;
            puts_("  ");
            print_name(d);
            puts_("  ");
            putdec(le32(d + 28));
            puts_(" bytes\r\n");
        }
    }
listed:
    puts_("listed ");
    putdec((u32)files);
    puts_(" files, ");
    putdec((u32)dirs);
    puts_(" dirs\r\n");

done:
    bus_error_jump = 0;
    puts_("fs scan done\r\n");
    return;

no_chip:
    bus_error_jump = 0;
    puts_("bus error touching the 5380\r\n");
}
