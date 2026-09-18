| start3 — ROM entry stub for rom3.c: header, vectors, stack, VBR, then C.
| Register facts for the TT's 5380 come from EmuTOS 1.4 (GPL) bios/scsi.c; the code here is ours.

        .equ    STACK,    0x00000800

        .text
        .globl  _start
        .equ    BEJ,      0x00000404      | low-RAM cell: where a bus error resumes (0 = halt).
                                          | It must NOT live in .data — that would be in ROM.
_start:
        bra.b   entry
        .word   0x0306
        .long   0x00E00030
        .long   0x00E00000
        .long   romend
        .long   0
        .long   0
        .long   0
        .long   0
        .long   0
        .long   0
        .long   0
        .long   0
entry:
        move.w  #0x2700,%sr
        lea     STACK,%sp
        lea     vectors,%a0
        movec   %a0,%vbr
        clr.l   BEJ
        jsr     _cmain
halt:   bra     halt

| Bus/address error: resume at bus_error_jump if C set one, else halt. The probe in rom1/rom2 proved
| this pattern; here it keeps a missing SCSI chip from wedging the firmware.
bus_error:
        lea     STACK,%sp
        move.l  BEJ,%a0
        move.l  %a0,%d0
        beq     halt
        jmp     (%a0)

        .align  4
vectors:
        .long   STACK
        .long   0x00E00030
        .long   bus_error
        .long   bus_error
        .rept   252
        .long   halt
        .endr
romend:
