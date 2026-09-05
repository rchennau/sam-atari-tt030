==============================================================================
STiNG & DAYNAPORT NETWORK DRIVER PACKAGES FOR ATARI TT030
==============================================================================

Package Overview:
  1. STiNG TCP/IP Stack (v1.26+):
     - Complete TCP/IP network stack for TOS / FreeMiNT.
     - Modules: STING.PRG (AUTO folder), STING.ACC (Desk Accessory), INETD.PRG.
  2. DaynaPORT SCSI Ethernet Driver (DAYNAPRT.STX):
     - Hardware network module for DaynaPORT SCSI-to-Ethernet adapters or
       BlueSCSI v2 / RaSCSI / PiSCSI emulation of DaynaPORT (SCSI ID 3 or 4).

Network Setup Checklist:
  - Copy STING.PRG to C:\AUTO\
  - Place DAYNAPRT.STX into C:\STING\
  - Edit C:\STING\STING.INF to set IP Address, Subnet Mask, Gateway, and DNS.
  - Test connectivity using SAM TT-Bridge HTTP Client (TTBRIDGE.PRG).
