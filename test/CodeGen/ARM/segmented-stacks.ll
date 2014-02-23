; RUN: llc < %s -mcpu=generic -march=arm -segmented-stacks -verify-machineinstrs | FileCheck %s -check-prefix=ARM-Generic

; We used to crash with filetype=obj
; RUN: llc < %s -mcpu=generic -march=arm -segmented-stacks -filetype=obj


; Just to prevent the alloca from being optimized away
declare void @dummy_use(i32*, i32)

define void @test_basic() {
        %mem = alloca i32, i32 10
        call void @dummy_use (i32* %mem, i32 10)
	ret void

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

; ARM-Generic:      mov     r4, #44
; ARM-Generic-NEXT: mov     r5, #0
; ARM-Generic-NEXT: stmdb   sp!, {lr}
; ARM-Generic-NEXT: bl      __morestack
; ARM-Generic-NEXT: ldm     sp!, {lr}
; ARM-Generic-NEXT: pop     {r4, r5}
; ARM-Generic-NEXT: mov     pc, lr

; ARM-Generic:      pop     {r4, r5}

}

define i32 @test_nested(i32 * nest %closure, i32 %other) {
       %addend = load i32 * %closure
       %result = add i32 %other, %addend
       ret i32 %result

; ARM-Generic:      test_nested:

; ARM-Generic:      push    {r4, r5}
; ARM-Generic-NEXT: mrc     p15, #0, r4, c13, c0, #3
; ARM-Generic-NEXT: mov     r5, sp
; ARM-Generic-NEXT: cmp     r4, #0
; ARM-Generic-NEXT: mvneq   r4, #61440
; ARM-Generic-NEXT: ldreq   r4, [r4, #-15]
; ARM-Generic-NEXT: add     r4, r4, #4
; ARM-Generic-NEXT: ldr     r4, [r4]
; ARM-Generic-NEXT: cmp     r4, r5
; ARM-Generic-NEXT: blo     .LBB1_2

; ARM-Generic:      mov     r4, #0
; ARM-Generic-NEXT: mov     r5, #0
; ARM-Generic-NEXT: stmdb   sp!, {lr}
; ARM-Generic-NEXT: bl      __morestack
; ARM-Generic-NEXT: ldm     sp!, {lr}
; ARM-Generic-NEXT: pop     {r4, r5}
; ARM-Generic-NEXT: mov     pc, lr

; ARM-Generic:      pop     {r4, r5}

}

define void @test_large() {
        %mem = alloca i32, i32 10000
        call void @dummy_use (i32* %mem, i32 0)
        ret void

; ARM-Generic:      test_large:

; ARM-Generic:      push    {r4, r5}
; ARM-Generic-NEXT: mrc     p15, #0, r4, c13, c0, #3
; ARM-Generic-NEXT: sub     r5, sp, #40192
; ARM-Generic-NEXT: cmp     r4, #0
; ARM-Generic-NEXT: mvneq   r4, #61440
; ARM-Generic-NEXT: ldreq   r4, [r4, #-15]
; ARM-Generic-NEXT: add     r4, r4, #4
; ARM-Generic-NEXT: ldr     r4, [r4]
; ARM-Generic-NEXT: cmp     r4, r5
; ARM-Generic-NEXT: blo     .LBB2_2

; ARM-Generic:      mov     r4, #40192
; ARM-Generic-NEXT: mov     r5, #0
; ARM-Generic-NEXT: stmdb   sp!, {lr}
; ARM-Generic-NEXT: bl      __morestack
; ARM-Generic-NEXT: ldm     sp!, {lr}
; ARM-Generic-NEXT: pop     {r4, r5}
; ARM-Generic-NEXT: mov     pc, lr

; ARM-Generic:      pop     {r4, r5}

}

define fastcc void @test_fastcc() {
        %mem = alloca i32, i32 10
        call void @dummy_use (i32* %mem, i32 10)
        ret void

; ARM-Generic:      test_fastcc:

; ARM-Generic:      push    {r4, r5}
; ARM-Generic-NEXT: mrc     p15, #0, r4, c13, c0, #3
; ARM-Generic-NEXT: mov     r5, sp
; ARM-Generic-NEXT: cmp     r4, #0
; ARM-Generic-NEXT: mvneq   r4, #61440
; ARM-Generic-NEXT: ldreq   r4, [r4, #-15]
; ARM-Generic-NEXT: add     r4, r4, #4
; ARM-Generic-NEXT: ldr     r4, [r4]
; ARM-Generic-NEXT: cmp     r4, r5
; ARM-Generic-NEXT: blo     .LBB3_2

; ARM-Generic:      mov     r4, #44
; ARM-Generic-NEXT: mov     r5, #0
; ARM-Generic-NEXT: stmdb   sp!, {lr}
; ARM-Generic-NEXT: bl      __morestack
; ARM-Generic-NEXT: ldm     sp!, {lr}
; ARM-Generic-NEXT: pop     {r4, r5}
; ARM-Generic-NEXT: mov     pc, lr

; ARM-Generic:      pop     {r4, r5}

}

define fastcc void @test_fastcc_large() {
        %mem = alloca i32, i32 10000
        call void @dummy_use (i32* %mem, i32 0)
        ret void

; ARM-Generic:      test_fastcc_large:

; ARM-Generic:      push    {r4, r5}
; ARM-Generic-NEXT: mrc     p15, #0, r4, c13, c0, #3
; ARM-Generic-NEXT: sub     r5, sp, #40192
; ARM-Generic-NEXT: cmp     r4, #0
; ARM-Generic-NEXT: mvneq   r4, #61440
; ARM-Generic-NEXT: ldreq   r4, [r4, #-15]
; ARM-Generic-NEXT: add     r4, r4, #4
; ARM-Generic-NEXT: ldr     r4, [r4]
; ARM-Generic-NEXT: cmp     r4, r5
; ARM-Generic-NEXT: blo     .LBB4_2

; ARM-Generic:      mov     r4, #40192
; ARM-Generic-NEXT: mov     r5, #0
; ARM-Generic-NEXT: stmdb   sp!, {lr}
; ARM-Generic-NEXT: bl      __morestack
; ARM-Generic-NEXT: ldm     sp!, {lr}
; ARM-Generic-NEXT: pop     {r4, r5}
; ARM-Generic-NEXT: mov     pc, lr

; ARM-Generic:      pop     {r4, r5}

}
