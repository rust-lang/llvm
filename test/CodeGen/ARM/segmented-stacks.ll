; RUN: llc < %s -mtriple=arm-linux-android -segmented-stacks -verify-machineinstrs | FileCheck %s -check-prefix=ARM-Linux-Android

; We used to crash with filetype=obj
; RUN: llc < %s -mtriple=arm-linux-android -segmented-stacks -filetype=obj


; Just to prevent the alloca from being optimized away
declare void @dummy_use(i32*, i32)

define void @test_basic() {
        %mem = alloca i32, i32 10
        call void @dummy_use (i32* %mem, i32 10)
	ret void

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

; ARM-Linux-Android:      mov     r4, #48
; ARM-Linux-Android-NEXT: mov     r5, #0
; ARM-Linux-Android-NEXT: stmdb   sp!, {lr}
; ARM-Linux-Android-NEXT: bl      __morestack
; ARM-Linux-Android-NEXT: ldm     sp!, {lr}
; ARM-Linux-Android-NEXT: pop     {r4, r5}
; ARM-Linux-Android-NEXT: mov     pc, lr

; ARM-Linux-Android:      pop     {r4, r5}

}

define i32 @test_nested(i32 * nest %closure, i32 %other) {
       %addend = load i32 * %closure
       %result = add i32 %other, %addend
       ret i32 %result

; ARM-Linux-Android:      test_nested:

; ARM-Linux-Android:      push    {r4, r5}
; ARM-Linux-Android-NEXT: mrc     p15, #0, r4, c13, c0, #3
; ARM-Linux-Android-NEXT: mov     r5, sp
; ARM-Linux-Android-NEXT: cmp     r4, #0
; ARM-Linux-Android-NEXT: mvneq   r4, #61440
; ARM-Linux-Android-NEXT: ldreq   r4, [r4, #-15]
; ARM-Linux-Android-NEXT: add     r4, r4, #252
; ARM-Linux-Android-NEXT: ldr     r4, [r4]
; ARM-Linux-Android-NEXT: cmp     r4, r5
; ARM-Linux-Android-NEXT: blo     .LBB1_2

; ARM-Linux-Android:      mov     r4, #0
; ARM-Linux-Android-NEXT: mov     r5, #0
; ARM-Linux-Android-NEXT: stmdb   sp!, {lr}
; ARM-Linux-Android-NEXT: bl      __morestack
; ARM-Linux-Android-NEXT: ldm     sp!, {lr}
; ARM-Linux-Android-NEXT: pop     {r4, r5}
; ARM-Linux-Android-NEXT: mov     pc, lr

; ARM-Linux-Android:      pop     {r4, r5}

}

define void @test_large() {
        %mem = alloca i32, i32 10000
        call void @dummy_use (i32* %mem, i32 0)
        ret void

; ARM-Linux-Android:      test_large:

; ARM-Linux-Android:      push    {r4, r5}
; ARM-Linux-Android-NEXT: mrc     p15, #0, r4, c13, c0, #3
; ARM-Linux-Android-NEXT: sub     r5, sp, #40192
; ARM-Linux-Android-NEXT: cmp     r4, #0
; ARM-Linux-Android-NEXT: mvneq   r4, #61440
; ARM-Linux-Android-NEXT: ldreq   r4, [r4, #-15]
; ARM-Linux-Android-NEXT: add     r4, r4, #252
; ARM-Linux-Android-NEXT: ldr     r4, [r4]
; ARM-Linux-Android-NEXT: cmp     r4, r5
; ARM-Linux-Android-NEXT: blo     .LBB2_2

; ARM-Linux-Android:      mov     r4, #40192
; ARM-Linux-Android-NEXT: mov     r5, #0
; ARM-Linux-Android-NEXT: stmdb   sp!, {lr}
; ARM-Linux-Android-NEXT: bl      __morestack
; ARM-Linux-Android-NEXT: ldm     sp!, {lr}
; ARM-Linux-Android-NEXT: pop     {r4, r5}
; ARM-Linux-Android-NEXT: mov     pc, lr

; ARM-Linux-Android:      pop     {r4, r5}

}

define fastcc void @test_fastcc() {
        %mem = alloca i32, i32 10
        call void @dummy_use (i32* %mem, i32 10)
        ret void

; ARM-Linux-Android:      test_fastcc:

; ARM-Linux-Android:      push    {r4, r5}
; ARM-Linux-Android-NEXT: mrc     p15, #0, r4, c13, c0, #3
; ARM-Linux-Android-NEXT: mov     r5, sp
; ARM-Linux-Android-NEXT: cmp     r4, #0
; ARM-Linux-Android-NEXT: mvneq   r4, #61440
; ARM-Linux-Android-NEXT: ldreq   r4, [r4, #-15]
; ARM-Linux-Android-NEXT: add     r4, r4, #252
; ARM-Linux-Android-NEXT: ldr     r4, [r4]
; ARM-Linux-Android-NEXT: cmp     r4, r5
; ARM-Linux-Android-NEXT: blo     .LBB3_2

; ARM-Linux-Android:      mov     r4, #48
; ARM-Linux-Android-NEXT: mov     r5, #0
; ARM-Linux-Android-NEXT: stmdb   sp!, {lr}
; ARM-Linux-Android-NEXT: bl      __morestack
; ARM-Linux-Android-NEXT: ldm     sp!, {lr}
; ARM-Linux-Android-NEXT: pop     {r4, r5}
; ARM-Linux-Android-NEXT: mov     pc, lr

; ARM-Linux-Android:      pop     {r4, r5}

}

define fastcc void @test_fastcc_large() {
        %mem = alloca i32, i32 10000
        call void @dummy_use (i32* %mem, i32 0)
        ret void

; ARM-Linux-Android:      test_fastcc_large:

; ARM-Linux-Android:      push    {r4, r5}
; ARM-Linux-Android-NEXT: mrc     p15, #0, r4, c13, c0, #3
; ARM-Linux-Android-NEXT: sub     r5, sp, #40192
; ARM-Linux-Android-NEXT: cmp     r4, #0
; ARM-Linux-Android-NEXT: mvneq   r4, #61440
; ARM-Linux-Android-NEXT: ldreq   r4, [r4, #-15]
; ARM-Linux-Android-NEXT: add     r4, r4, #252
; ARM-Linux-Android-NEXT: ldr     r4, [r4]
; ARM-Linux-Android-NEXT: cmp     r4, r5
; ARM-Linux-Android-NEXT: blo     .LBB4_2

; ARM-Linux-Android:      mov     r4, #40192
; ARM-Linux-Android-NEXT: mov     r5, #0
; ARM-Linux-Android-NEXT: stmdb   sp!, {lr}
; ARM-Linux-Android-NEXT: bl      __morestack
; ARM-Linux-Android-NEXT: ldm     sp!, {lr}
; ARM-Linux-Android-NEXT: pop     {r4, r5}
; ARM-Linux-Android-NEXT: mov     pc, lr

; ARM-Linux-Android:      pop     {r4, r5}

}
