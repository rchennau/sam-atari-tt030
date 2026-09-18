| rom1 — second custom-firmware milestone: size RAM and report it over serial.
| TOS spends ~80 s of its ~96 s boot testing 64 MB; this writes one pattern per megabyte instead.
|
| Sizes ST RAM (from 0x000800 up, below the ROM's own stack) and TT RAM (0x01000000 up) by writing
| a signature to the first longword of each megabyte and reading it back, stopping at the first
| megabyte that does not hold it. Aliasing is caught too: a wrapped address reads back the signature
| of an earlier megabyte, so the signature includes the address.
|
| Absent memory raises a bus error rather than reading back wrong data (measured 2026-09-18: without
| a handler Hatari wedges on the first missing megabyte), so the firmware installs its own vector
| table in ROM via VBR and recovers from vector 2/3 by abandoning the faulting instruction: the
| handler resets the stack and jumps to probe_fail, which returns the count so far. No RAM needed.
|
| Build: make rom1   Run: make hatari1

        .equ    MFP,      0xFFFFFA00
        .equ    MFP_TDDR, MFP+0x25
        .equ    MFP_TCDCR,MFP+0x1D
        .equ    MFP_UCR,  MFP+0x29
        .equ    MFP_TSR,  MFP+0x2D
        .equ    MFP_UDR,  MFP+0x2F
        .equ    NUMBUF,   0x00000400      | scratch in low ST RAM (ROM is not writable)
        .equ    STACK,    0x00000800      | below it is ours to probe
        .equ    STRAM,    0x00000800
        .equ    TTRAM,    0x01000000
        .equ    MB,       0x00100000
        .equ    SIG,      0x5AA50000      | signature | megabyte index, so an alias is visible

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
        lea     vectors,%a0
        movec   %a0,%vbr                  | our exception table, in ROM
        bsr     serial_init
        lea     banner,%a0
        bsr     puts

        move.l  #STRAM,%d1                | ST RAM: start of the first megabyte we may touch
        bsr     probe
        move.l  %d0,%d6
        lea     st_msg,%a0
        bsr     puts
        move.l  %d6,%d0
        bsr     putdec_mb

        move.l  #TTRAM,%d1
        bsr     probe
        move.l  %d0,%d6
        lea     tt_msg,%a0
        bsr     puts
        move.l  %d6,%d0
        bsr     putdec_mb

        lea     done_msg,%a0
        bsr     puts
halt:   bra     halt

| probe: d1 = base address. Returns d0 = megabytes that answered.
| Writes SIG|index to base+n*MB, reads it back; stops at the first mismatch.
| The return address is kept in a5 so the bus-error handler can finish the call after resetting sp.
probe:
        move.l  (%sp)+,%a5
        moveq   #0,%d2                    | megabyte counter
        move.l  %d1,%a1
probe_loop:
        move.l  %d2,%d3
        or.l    #SIG,%d3
        move.l  %d3,(%a1)
        move.l  (%a1),%d4
        cmp.l   %d3,%d4
        bne     probe_end
        addq.l  #1,%d2
        cmp.l   #512,%d2                  | stop at 512 MB, far past any TT
        beq     probe_end
        add.l   #MB,%a1                   | lea with a 1 MB displacement needs 68020+; add is fine everywhere
        bra     probe_loop
probe_end:
probe_fail:                               | the bus-error handler lands here
        move.l  %d2,%d0
        jmp     (%a5)

| Vector 2 (bus error) / 3 (address error): the probe touched memory that is not there. Abandon the
| instruction — the exception frame is of no use, so reset the stack and finish the probe.
bus_error:
        lea     STACK,%sp
        jmp     probe_fail

serial_init:
        move.b  #0x88,MFP_UCR             | 8N1, /16
        move.b  #1,MFP_TDDR
        move.b  #1,MFP_TCDCR
        move.b  #5,MFP+0x2B               | transmitter enable
        rts

| puts: a0 = NUL-terminated string
puts:   move.b  (%a0)+,%d0
        beq     puts_end
        bsr     putc
        bra     puts
puts_end:
        rts

| putc: d0 = character
putc:   btst    #7,MFP_TSR
        beq     putc
        move.b  %d0,MFP_UDR
        rts

| putdec_mb: d0 = value (< 65536), prints "<n> MB\r\n". Buffer is in low RAM, not ROM.
putdec_mb:
        movem.l %d0-%d1/%a0,-(%sp)
        lea     NUMBUF+11,%a0
        clr.b   -(%a0)
putdec_digit:
        and.l   #0x0000FFFF,%d0
        divu.w  #10,%d0                   | d0 low = quotient, high word = remainder
        move.l  %d0,%d1
        swap    %d1
        add.b   #'0',%d1
        move.b  %d1,-(%a0)
        and.l   #0x0000FFFF,%d0
        tst.w   %d0
        bne     putdec_digit
        bsr     puts
        lea     mb_suffix,%a0
        bsr     puts
        movem.l (%sp)+,%d0-%d1/%a0
        rts

| Exception vectors in ROM: 0/1 are the reset SSP+PC, 2/3 recover the probe, the rest halt.
        .align  4
vectors:
        .long   STACK
        .long   0x00E00030
        .long   bus_error
        .long   bus_error
        .rept   252
        .long   trap_halt
        .endr
trap_halt:
        bra     trap_halt

banner:   .ascii  "SAM-TT firmware rom1\r\n"
          .byte   0
st_msg:   .ascii  "ST RAM  "
          .byte   0
tt_msg:   .ascii  "TT RAM  "
          .byte   0
mb_suffix:.ascii  " MB\r\n"
          .byte   0
done_msg: .ascii  "sizing done\r\n"
          .byte   0
          .even
romend:
