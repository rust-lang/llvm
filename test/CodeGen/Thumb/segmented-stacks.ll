; RUN: llc < %s -mcpu=generic -march=thumb -segmented-stacks -verify-machineinstrs | FileCheck %s -check-prefix=Thumb-Generic

; We used to crash with filetype=obj
; RUN: llc < %s -mcpu=generic -march=thumb -segmented-stacks -filetype=obj


; Just to prevent the alloca from being optimized away
declare void @dummy_use(i32*, i32)

define void @test_basic() {
        %mem = alloca i32, i32 10
        call void @dummy_use (i32* %mem, i32 10)
	ret void

; Thumb-Generic:      test_basic:

; Thumb-Generic:      push    {r4, r5}
; Thumb-Generic-NEXT: mov     r5, sp
; Thumb-Generic-NEXT: ldr     r4, .LCPI0_0
; Thumb-Generic-NEXT: ldr     r4, [r4]
; Thumb-Generic-NEXT: cmp     r4, r5
; Thumb-Generic-NEXT: blo     .LBB0_2

; Thumb-Generic:      mov     r4, #44
; Thumb-Generic-NEXT: mov     r5, #0
; Thumb-Generic-NEXT: push    {lr}
; Thumb-Generic-NEXT: bl      __morestack
; Thumb-Generic-NEXT: pop     {r4}
; Thumb-Generic-NEXT: mov     lr, r4
; Thumb-Generic-NEXT: pop     {r4, r5}
; Thumb-Generic-NEXT: mov     pc, lr

; Thumb-Generic:      pop     {r4, r5}

}

define i32 @test_nested(i32 * nest %closure, i32 %other) {
       %addend = load i32 * %closure
       %result = add i32 %other, %addend
       ret i32 %result

; Thumb-Generic:      test_nested:

; Thumb-Generic:      push    {r4, r5}
; Thumb-Generic-NEXT: mov     r5, sp
; Thumb-Generic-NEXT: ldr     r4, .LCPI1_0
; Thumb-Generic-NEXT: ldr     r4, [r4]
; Thumb-Generic-NEXT: cmp     r4, r5
; Thumb-Generic-NEXT: blo     .LBB1_2

; Thumb-Generic:      mov     r4, #0
; Thumb-Generic-NEXT: mov     r5, #0
; Thumb-Generic-NEXT: push    {lr}
; Thumb-Generic-NEXT: bl      __morestack
; Thumb-Generic-NEXT: pop     {r4}
; Thumb-Generic-NEXT: mov     lr, r4
; Thumb-Generic-NEXT: pop     {r4, r5}
; Thumb-Generic-NEXT: mov     pc, lr

; Thumb-Generic:      pop     {r4, r5}

}

define void @test_large() {
        %mem = alloca i32, i32 10000
        call void @dummy_use (i32* %mem, i32 0)
        ret void

; Thumb-Generic:      test_large:

; Thumb-Generic:      push    {r4, r5}
; Thumb-Generic-NEXT: mov     r5, sp
; Thumb-Generic-NEXT: sub     r5, #40192
; Thumb-Generic-NEXT: ldr     r4, .LCPI2_2
; Thumb-Generic-NEXT: ldr     r4, [r4]
; Thumb-Generic-NEXT: cmp     r4, r5
; Thumb-Generic-NEXT: blo     .LBB2_2

; Thumb-Generic:      mov     r4, #40192
; Thumb-Generic-NEXT: mov     r5, #0
; Thumb-Generic-NEXT: push    {lr}
; Thumb-Generic-NEXT: bl      __morestack
; Thumb-Generic-NEXT: pop     {r4}
; Thumb-Generic-NEXT: mov     lr, r4
; Thumb-Generic-NEXT: pop     {r4, r5}
; Thumb-Generic-NEXT: mov     pc, lr

; Thumb-Generic:      pop     {r4, r5}

}

define fastcc void @test_fastcc() {
        %mem = alloca i32, i32 10
        call void @dummy_use (i32* %mem, i32 10)
        ret void

; Thumb-Generic:      test_fastcc:

; Thumb-Generic:      push    {r4, r5}
; Thumb-Generic-NEXT: mov     r5, sp
; Thumb-Generic-NEXT: ldr     r4, .LCPI3_0
; Thumb-Generic-NEXT: ldr     r4, [r4]
; Thumb-Generic-NEXT: cmp     r4, r5
; Thumb-Generic-NEXT: blo     .LBB3_2

; Thumb-Generic:      mov     r4, #44
; Thumb-Generic-NEXT: mov     r5, #0
; Thumb-Generic-NEXT: push    {lr}
; Thumb-Generic-NEXT: bl      __morestack
; Thumb-Generic-NEXT: pop     {r4}
; Thumb-Generic-NEXT: mov     lr, r4
; Thumb-Generic-NEXT: pop     {r4, r5}
; Thumb-Generic-NEXT: mov     pc, lr

; Thumb-Generic:      pop     {r4, r5}

}

define fastcc void @test_fastcc_large() {
        %mem = alloca i32, i32 10000
        call void @dummy_use (i32* %mem, i32 0)
        ret void

; Thumb-Generic:      test_fastcc_large:

; Thumb-Generic:      push    {r4, r5}
; Thumb-Generic-NEXT: mov     r5, sp
; Thumb-Generic-NEXT: sub     r5, #40192
; Thumb-Generic-NEXT: ldr     r4, .LCPI4_2
; Thumb-Generic-NEXT: ldr     r4, [r4]
; Thumb-Generic-NEXT: cmp     r4, r5
; Thumb-Generic-NEXT: blo     .LBB4_2

; Thumb-Generic:      mov     r4, #40192
; Thumb-Generic-NEXT: mov     r5, #0
; Thumb-Generic-NEXT: push    {lr}
; Thumb-Generic-NEXT: bl      __morestack
; Thumb-Generic-NEXT: pop     {r4}
; Thumb-Generic-NEXT: mov     lr, r4
; Thumb-Generic-NEXT: pop     {r4, r5}
; Thumb-Generic-NEXT: mov     pc, lr

; Thumb-Generic:      pop     {r4, r5}

}
