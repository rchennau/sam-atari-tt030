==============================================================================
ATARI TT030 ECOSYSTEM INTEGRATION - PHASE 0 BOOT & DRIVER FLOPPY
==============================================================================

This floppy disk contains essential boot managers, SCSI storage drivers,
network drivers, and BlueSCSI configuration files for Atari TT030 setup.

Directory Overview:
  /AUTO/           - Boot selectors (XBOOT III, ST-Menu, Selectric)
  /DRIVERS/SCSI/   - ICD AdSCSI & AHDI drivers for ACSI/SCSI host adapters
  /DRIVERS/NET/    - STiNG TCP/IP stack & DaynaPORT SCSI Ethernet drivers
  /BLUESCSI/       - bluescsi.ini configuration tailored for TT030

TOS Boot Sequence Notes:
  - Programs in the AUTO directory run in ASCII alphabetical order on boot.
  - Recommended order: SCSI Driver -> Network Stack -> Boot Selector.
