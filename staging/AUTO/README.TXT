==============================================================================
AUTO FOLDER PROGRAM SELECTION (ATARI TT030 / TOS 3.06)
==============================================================================

Programs placed in C:\AUTO\ (or A:\AUTO\ on floppy boot) execute sequentially
during system boot.

Candidates Included / Recommended:
  1. XBOOT III (XBOOT.PRG):
     Interactive boot manager allowing profile switching (e.g. Clean TOS,
     FreeMiNT environment, Network-enabled TOS).
  2. ST-Menu (STMENU.PRG):
     Lightweight desktop menu / gem loader for targeted startup routines.
  3. Selectric (SELECTR.PRG):
     CPX configuration module & boot selection utility.

Execution Ordering Tip:
  Prefix executable names with numbers if exact ordering is required:
    01_ICD.PRG     (SCSI Hard Disk Driver)
    02_STING.PRG   (Network Driver Initialization)
    03_XBOOT.PRG   (Boot Selector Menu)
