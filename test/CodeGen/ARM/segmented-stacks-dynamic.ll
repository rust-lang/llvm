; RUN: llc < %s -mcpu=generic -march=arm -segmented-stacks -verify-machineinstrs | FileCheck %s -check-prefix=ARM-Generic
; RUN: llc < %s -mcpu=generic -march=arm -segmented-stacks -filetype=obj

; Just to prevent the alloca from being optimized away
declare void @dummy_use(i32*, i32)

define i32 @test_basic(i32 %l) {
        %mem = alloca i32, i32 %l
        call void @dummy_use (i32* %mem, i32 %l)
        %terminate = icmp eq i32 %l, 0
        br i1 %terminate, label %true, label %false

true:
        ret i32 0

false:
        %newlen = sub i32 %l, 1
        %retvalue = call i32 @test_basic(i32 %newlen)
        ret i32 %retvalue

; ARM-Generic:      test_basic:

; ARM-Generic:      push    {r4, r5}
; ARM-Generic-NEXT: mrc     p15, #0, r4, c13, c0, #3
; ARM-Generic-NEXT: mov     r5, sp
; ARM-Generic-NEXT: cmp     r4, #0
; ARM-Generic-NEXT: mvneq   r4, #61440
; ARM-Generic-NEXT: ldreq   r4, [r4, #-15]
; ARM-Generic-NEXT: add     r4, r4, #4
; ARM-Generic-NEXT: ldr     r4, [r4]
; ARM-Generic-NEXT: cmp     r4, r5
; ARM-Generic-NEXT: blo     .LBB0_2

; ARM-Generic:      mov     r4, #16
; ARM-Generic-NEXT: mov     r5, #0
; ARM-Generic-NEXT: stmdb   sp!, {lr}
; ARM-Generic-NEXT: bl      __morestack
; ARM-Generic-NEXT: ldm     sp!, {lr}
; ARM-Generic-NEXT: pop     {r4, r5}
; ARM-Generic-NEXT: mov     pc, lr

; ARM-Generic:      pop     {r4, r5}

}
