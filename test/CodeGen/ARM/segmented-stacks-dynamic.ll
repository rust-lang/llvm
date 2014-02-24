; RUN: llc < %s -mtriple=arm-linux-android -segmented-stacks -verify-machineinstrs | FileCheck %s -check-prefix=ARM-Linux-Android
; RUN: llc < %s -mtriple=arm-linux-android -segmented-stacks -filetype=obj

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

; ARM-Linux-Android:      test_basic:

; ARM-Linux-Android:      push    {r4, r5}
; ARM-Linux-Android-NEXT: mrc     p15, #0, r4, c13, c0, #3
; ARM-Linux-Android-NEXT: mov     r5, sp
; ARM-Linux-Android-NEXT: cmp     r4, #0
; ARM-Linux-Android-NEXT: mvneq   r4, #61440
; ARM-Linux-Android-NEXT: ldreq   r4, [r4, #-15]
; ARM-Linux-Android-NEXT: add     r4, r4, #252
; ARM-Linux-Android-NEXT: ldr     r4, [r4]
; ARM-Linux-Android-NEXT: cmp     r4, r5
; ARM-Linux-Android-NEXT: blo     .LBB0_2

; ARM-Linux-Android:      mov     r4, #24
; ARM-Linux-Android-NEXT: mov     r5, #0
; ARM-Linux-Android-NEXT: stmdb   sp!, {lr}
; ARM-Linux-Android-NEXT: bl      __morestack
; ARM-Linux-Android-NEXT: ldm     sp!, {lr}
; ARM-Linux-Android-NEXT: pop     {r4, r5}
; ARM-Linux-Android-NEXT: mov     pc, lr

; ARM-Linux-Android:      pop     {r4, r5}

}
