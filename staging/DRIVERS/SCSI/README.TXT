==============================================================================
FREEWARE SCSI DRIVERS FOR ATARI TT030 (ICD AdSCSI & AHDI)
==============================================================================

The Atari TT030 features an internal 8-bit narrow SCSI host bus controller
(NCR 5380 based).

Drivers provided / documented here:
  1. ICD AdSCSI (v6.5+ Freeware):
     - Highly recommended for TT030 internal SCSI & external ACSI/SCSI drives.
     - Supports SCSI IDs 0..7, extended partition tables, parity mode.
     - Files: ICDBOOT.PRG, ICDFMT.PRG, ICDFMT.CFG.

  2. Atari AHDI (v3.00+ Freeware):
     - Official Atari Hard Disk Interface driver.
     - Files: HDC_BOOT.PRG, AHDI.PRG.

Installation Instructions:
  - Copy ICDBOOT.PRG or AHDI.PRG into the AUTO folder of your boot drive (e.g. C:\AUTO\01_SCSI.PRG).
  - Ensure SCSI parity and bus termination are correctly configured on physical drives or BlueSCSI emulator.
