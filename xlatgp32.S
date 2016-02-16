@ Convert a VGA video buffer to GP32 format
@  by Jeff F, 2002-Apr-4
@
@  This GCC assembly routine converts an 8-bit VGA style
@ video buffer to GP32 8-bit video frame buffer format.
@
@  The default Performance is set to 4. The smallest code
@ is obtained by setting this value to 1. The fastest and
@ largest code is obtained by setting this value to 80.
@ Here are the only valid values for Performance:
@   1,2,4,5,8,10,16,20,40,80
@ The percentage gain of speed improvements drops off
@ exponentially with each larger Performance setting
@ so 4 is probably fine for most cases.
@
@ Compile this file by using the following:
@  as -o xlatgp32.o xlatgp32.s
@ Then include xlatgp32.o in your link files list.
@
@ Reference it from C by using:
@  extern void XlatVGABuffer (unsigned char *src, unsigned char *dest);
@
@ Entry:
@  r0 = src, r1 = dest

.EQU Performance, 4

        .ALIGN
        .GLOBAL XlatVGABuffer
        .TYPE   XlatVGABuffer, function

XlatVGABuffer:
        stmfd r13!,{r4}
        mov r12,#240
        add r1,r1,#240
XlatLoop1:
        sub r1,r1,#1
        mov r3,r1
        mov r4,#80/Performance
XlatLoop2:
        .REPT Performance
        ldr r2,[r0],#4
        strb r2,[r3],#240
        mov r2,r2,lsr #8
        strb r2,[r3],#240
        mov r2,r2,lsr #8
        strb r2,[r3],#240
        mov r2,r2,lsr #8
        strb r2,[r3],#240
        .ENDR

        subs r4,r4,#1
        bne XlatLoop2

        subs r12,r12,#1
        bne XlatLoop1

        ldmfd r13!,{r4}
        bx lr
.fend1:
        .SIZE XlatVGABuffer, .fend1-XlatVGABuffer
