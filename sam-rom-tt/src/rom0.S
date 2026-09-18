| rom0 — smallest custom TT030 firmware: say hello on the serial port, then halt.
| Proves the chain (ROM header -> reset entry -> MFP USART) before any EPROM is burned.
| No TOS: no GEMDOS/BIOS/VDI. Header layout taken from the real tos306uk.img
| (longword 0 = BRA + OS version, longword 1 = reset PC, longword 2 = ROM base).
|
| Build: ../Makefile (m68k-atari-mint-gcc -nostdlib + objcopy -O binary, padded to 512 KB)
| Serial: MFP 68901 USART, 9600 8N1, the port TOS calls Modem 1.

        .equ    MFP,      0xFFFFFA00
        .equ    MFP_TDDR, MFP+0x25
        .equ    MFP_TCDCR,MFP+0x1D
        .equ    MFP_UCR,  MFP+0x29
        .equ    MFP_TSR,  MFP+0x2D
        .equ    MFP_UDR,  MFP+0x2F
        .equ    STACK,    0x00001000

        .text
        .globl  _start
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
        move.b  #0x88,MFP_UCR
        move.b  #1,MFP_TDDR
        move.b  #1,MFP_TCDCR
        move.b  #5,MFP+0x2B
        lea     msg,%a0
tx:     move.b  (%a0)+,%d0
        beq     done
wait:   btst    #7,MFP_TSR
        beq     wait
        move.b  %d0,MFP_UDR
        bra     tx
done:   bra     done

msg:    .ascii  "SAM-TT custom ROM 0.1 alive\r\n"
        .byte   0
        .even
romend:
