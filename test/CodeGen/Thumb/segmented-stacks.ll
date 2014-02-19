; RUN: llc < %s -mcpu=generic -mtriple=thumb-linux-android -segmented-stacks -verify-machineinstrs | FileCheck %s -check-prefix=Thumb-Linux-Android

; We used to crash with filetype=obj
; RUN: llc < %s -mcpu=generic -mtriple=thumb-linux-android -segmented-stacks -filetype=obj


; Just to prevent the alloca from being optimized away
declare void @dummy_use(i32*, i32)

define void @test_basic() {
        %mem = alloca i32, i32 10
        call void @dummy_use (i32* %mem, i32 10)
	ret void

; Thumb-Linux-Android:      test_basic:

; Thumb-Linux-Android:      push    {r4, r5}
; Thumb-Linux-Android-NEXT: mov     r5, sp
; Thumb-Linux-Android-NEXT: ldr     r4, .LCPI0_0
; Thumb-Linux-Android-NEXT: ldr     r4, [r4]
; Thumb-Linux-Android-NEXT: cmp     r4, r5
; Thumb-Linux-Android-NEXT: blo     .LBB0_2

; Thumb-Linux-Android:      mov     r4, #48
; Thumb-Linux-Android-NEXT: mov     r5, #0
; Thumb-Linux-Android-NEXT: push    {lr}
; Thumb-Linux-Android-NEXT: bl      __morestack
; Thumb-Linux-Android-NEXT: pop     {r4}
; Thumb-Linux-Android-NEXT: mov     lr, r4
; Thumb-Linux-Android-NEXT: pop     {r4, r5}
; Thumb-Linux-Android-NEXT: mov     pc, lr

; Thumb-Linux-Android:      pop     {r4, r5}

}

define i32 @test_nested(i32 * nest %closure, i32 %other) {
       %addend = load i32 * %closure
       %result = add i32 %other, %addend
       ret i32 %result

; Thumb-Linux-Android:      test_nested:

; Thumb-Linux-Android:      push    {r4, r5}
; Thumb-Linux-Android-NEXT: mov     r5, sp
; Thumb-Linux-Android-NEXT: ldr     r4, .LCPI1_0
; Thumb-Linux-Android-NEXT: ldr     r4, [r4]
; Thumb-Linux-Android-NEXT: cmp     r4, r5
; Thumb-Linux-Android-NEXT: blo     .LBB1_2

; Thumb-Linux-Android:      mov     r4, #0
; Thumb-Linux-Android-NEXT: mov     r5, #0
; Thumb-Linux-Android-NEXT: push    {lr}
; Thumb-Linux-Android-NEXT: bl      __morestack
; Thumb-Linux-Android-NEXT: pop     {r4}
; Thumb-Linux-Android-NEXT: mov     lr, r4
; Thumb-Linux-Android-NEXT: pop     {r4, r5}
; Thumb-Linux-Android-NEXT: mov     pc, lr

; Thumb-Linux-Android:      pop     {r4, r5}

}

define void @test_large() {
        %mem = alloca i32, i32 10000
        call void @dummy_use (i32* %mem, i32 0)
        ret void

; Thumb-Linux-Android:      test_large:

; Thumb-Linux-Android:      push    {r4, r5}
; Thumb-Linux-Android-NEXT: mov     r5, sp
; Thumb-Linux-Android-NEXT: sub     r5, #40192
; Thumb-Linux-Android-NEXT: ldr     r4, .LCPI2_2
; Thumb-Linux-Android-NEXT: ldr     r4, [r4]
; Thumb-Linux-Android-NEXT: cmp     r4, r5
; Thumb-Linux-Android-NEXT: blo     .LBB2_2

; Thumb-Linux-Android:      mov     r4, #40192
; Thumb-Linux-Android-NEXT: mov     r5, #0
; Thumb-Linux-Android-NEXT: push    {lr}
; Thumb-Linux-Android-NEXT: bl      __morestack
; Thumb-Linux-Android-NEXT: pop     {r4}
; Thumb-Linux-Android-NEXT: mov     lr, r4
; Thumb-Linux-Android-NEXT: pop     {r4, r5}
; Thumb-Linux-Android-NEXT: mov     pc, lr

; Thumb-Linux-Android:      pop     {r4, r5}

}

define fastcc void @test_fastcc() {
        %mem = alloca i32, i32 10
        call void @dummy_use (i32* %mem, i32 10)
        ret void

; Thumb-Linux-Android:      test_fastcc:

; Thumb-Linux-Android:      push    {r4, r5}
; Thumb-Linux-Android-NEXT: mov     r5, sp
; Thumb-Linux-Android-NEXT: ldr     r4, .LCPI3_0
; Thumb-Linux-Android-NEXT: ldr     r4, [r4]
; Thumb-Linux-Android-NEXT: cmp     r4, r5
; Thumb-Linux-Android-NEXT: blo     .LBB3_2

; Thumb-Linux-Android:      mov     r4, #48
; Thumb-Linux-Android-NEXT: mov     r5, #0
; Thumb-Linux-Android-NEXT: push    {lr}
; Thumb-Linux-Android-NEXT: bl      __morestack
; Thumb-Linux-Android-NEXT: pop     {r4}
; Thumb-Linux-Android-NEXT: mov     lr, r4
; Thumb-Linux-Android-NEXT: pop     {r4, r5}
; Thumb-Linux-Android-NEXT: mov     pc, lr

; Thumb-Linux-Android:      pop     {r4, r5}

}

define fastcc void @test_fastcc_large() {
        %mem = alloca i32, i32 10000
        call void @dummy_use (i32* %mem, i32 0)
        ret void

; Thumb-Linux-Android:      test_fastcc_large:

; Thumb-Linux-Android:      push    {r4, r5}
; Thumb-Linux-Android-NEXT: mov     r5, sp
; Thumb-Linux-Android-NEXT: sub     r5, #40192
; Thumb-Linux-Android-NEXT: ldr     r4, .LCPI4_2
; Thumb-Linux-Android-NEXT: ldr     r4, [r4]
; Thumb-Linux-Android-NEXT: cmp     r4, r5
; Thumb-Linux-Android-NEXT: blo     .LBB4_2

; Thumb-Linux-Android:      mov     r4, #40192
; Thumb-Linux-Android-NEXT: mov     r5, #0
; Thumb-Linux-Android-NEXT: push    {lr}
; Thumb-Linux-Android-NEXT: bl      __morestack
; Thumb-Linux-Android-NEXT: pop     {r4}
; Thumb-Linux-Android-NEXT: mov     lr, r4
; Thumb-Linux-Android-NEXT: pop     {r4, r5}
; Thumb-Linux-Android-NEXT: mov     pc, lr

; Thumb-Linux-Android:      pop     {r4, r5}

}
