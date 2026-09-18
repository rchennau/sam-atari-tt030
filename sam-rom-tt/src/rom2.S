| rom2 — third custom-firmware milestone: caches, FPU, memory-controller readout, RAM sizing.
| Everything is reported over the MFP serial port, so a real-TT run on the cable produces the same
| lines as `make hatari2` and the two can be diffed.
|
| Deliberately does NOT write the memory controller. The correct value for this machine's RAM fit
| is hardware-dependent, an unverified write can make ST RAM unreadable, and the firmware currently
| runs on a machine TOS already configured or that Hatari configures for us. rom2 reports what the
| register holds; programming it belongs to the first real-hardware session, with the value read
| here as the reference.
|
| Build: make rom2    Run: make hatari2

        .equ    MFP,      0xFFFFFA00
        .equ    MFP_TDDR, MFP+0x25
        .equ    MFP_TCDCR,MFP+0x1D
        .equ    MFP_UCR,  MFP+0x29
        .equ    MFP_TSR,  MFP+0x2D
        .equ    MFP_UDR,  MFP+0x2F
        .equ    MEMCTRL,  0x00FF8001      | ST/STE/TT memory configuration register (byte)
        .equ    NUMBUF,   0x00000400
        .equ    STACK,    0x00000800
        .equ    STRAM,    0x00000800
        .equ    TTRAM,    0x01000000
        .equ    MB,       0x00100000
        .equ    SIG,      0x5AA50000
        .equ    CACR_BOTH,0x00000101      | 68030: bit 0 enable instruction cache, bit 8 enable data cache

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
        movec   %a0,%vbr
        bsr     serial_init
        lea     banner,%a0
        bsr     puts

| --- memory controller: report, do not write (see header) ---
        lea     mc_msg,%a0
        bsr     puts
        moveq   #0,%d0
        move.b  MEMCTRL,%d0
        bsr     puthex2

| --- caches: enable, then read CACR back so the line is evidence, not intent ---
        move.l  #CACR_BOTH,%d0
        movec   %d0,%cacr
        movec   %cacr,%d0
        lea     cacr_msg,%a0
        bsr     puts
        bsr     puthex8

| --- FPU: a line-F exception means absent. Vector 11 sets d7 and skips to fpu_none. ---
        moveq   #0,%d7
        lea     fpu_msg,%a0
        bsr     puts
        lea     fpu_done,%a5              | where the line-F handler resumes
        fnop
        lea     yes_str,%a0
        bra     fpu_print
fpu_none:
        lea     no_str,%a0
fpu_print:
        bsr     puts
fpu_done:

| --- RAM sizing (as rom1) ---
        move.l  #STRAM,%d1
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

| probe: d1 = base. Returns d0 = megabytes that answered. Return address in a5 so the bus-error
| handler can complete the call after resetting the stack.
probe:
        move.l  (%sp)+,%a5
        moveq   #0,%d2
        move.l  %d1,%a1
probe_loop:
        move.l  %d2,%d3
        or.l    #SIG,%d3
        move.l  %d3,(%a1)
        move.l  (%a1),%d4
        cmp.l   %d3,%d4
        bne     probe_end
        addq.l  #1,%d2
        cmp.l   #512,%d2
        beq     probe_end
        add.l   #MB,%a1
        bra     probe_loop
probe_end:
probe_fail:
        move.l  %d2,%d0
        jmp     (%a5)

bus_error:
        lea     STACK,%sp
        jmp     probe_fail

| Line-F (vector 11): no FPU answered the fnop. Resume at the label a5 points to, via fpu_none.
line_f:
        lea     STACK,%sp
        bra     fpu_none

serial_init:
        move.b  #0x88,MFP_UCR
        move.b  #1,MFP_TDDR
        move.b  #1,MFP_TCDCR
        move.b  #5,MFP+0x2B
        rts

puts:   move.b  (%a0)+,%d0
        beq     puts_end
        bsr     putc
        bra     puts
puts_end:
        rts

putc:   btst    #7,MFP_TSR
        beq     putc
        move.b  %d0,MFP_UDR
        rts

| puthex2/puthex8: d0 printed as 2 or 8 hex digits, then CR LF.
puthex2:
        moveq   #1,%d5
        bra     puthex
puthex8:
        moveq   #7,%d5
puthex:
        movem.l %d0-%d3/%d5,-(%sp)
        move.l  %d5,%d3
puthex_digit:
        move.l  %d3,%d2
        lsl.l   #2,%d2                    | digit index * 4 = shift count
        move.l  %d0,%d1
        lsr.l   %d2,%d1
        and.l   #0x0F,%d1
        cmp.b   #10,%d1
        blt     puthex_num
        add.b   #'a'-10,%d1
        bra     puthex_emit
puthex_num:
        add.b   #'0',%d1
puthex_emit:
        movem.l %d0-%d3,-(%sp)
        move.b  %d1,%d0
        bsr     putc
        movem.l (%sp)+,%d0-%d3
        dbra    %d3,puthex_digit
        lea     crlf,%a0
        bsr     puts
        movem.l (%sp)+,%d0-%d3/%d5
        rts

| putdec_mb: d0 (< 65536) as decimal, then " MB\r\n". Buffer is in low RAM; ROM is not writable.
putdec_mb:
        movem.l %d0-%d1/%a0,-(%sp)
        lea     NUMBUF+11,%a0
        clr.b   -(%a0)
putdec_digit:
        and.l   #0x0000FFFF,%d0
        divu.w  #10,%d0
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

| Exception table in ROM: 0/1 reset, 2/3 probe recovery, 11 line-F (no FPU), rest halt.
        .align  4
vectors:
        .long   STACK
        .long   0x00E00030
        .long   bus_error
        .long   bus_error
        .rept   7
        .long   trap_halt
        .endr
        .long   line_f                    | vector 11
        .rept   244
        .long   trap_halt
        .endr
trap_halt:
        bra     trap_halt

banner:   .ascii  "SAM-TT firmware rom2\r\n"
          .byte   0
mc_msg:   .ascii  "memctrl "
          .byte   0
cacr_msg: .ascii  "cacr    "
          .byte   0
fpu_msg:  .ascii  "fpu     "
          .byte   0
yes_str:  .ascii  "present\r\n"
          .byte   0
no_str:   .ascii  "absent\r\n"
          .byte   0
st_msg:   .ascii  "ST RAM  "
          .byte   0
tt_msg:   .ascii  "TT RAM  "
          .byte   0
mb_suffix:.ascii  " MB\r\n"
          .byte   0
crlf:     .ascii  "\r\n"
          .byte   0
done_msg: .ascii  "bring-up done\r\n"
          .byte   0
          .even
romend:
