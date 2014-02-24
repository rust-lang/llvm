; RUN: llc < %s -mtriple=thumb-unknown-unknown -segmented-stacks -verify-machineinstrs | FileCheck %s -check-prefix=Thumb-Generic
; RUN: llc < %s -mtriple=thumb-unknown-unknown -segmented-stacks -filetype=obj

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

; Thumb-Generic:      test_basic:

; Thumb-Generic:      push {r4, r5}
; Thumb-Generic-NEXT: mov	r5, sp
; Thumb-Generic-NEXT: ldr r4, .LCPI0_0
; Thumb-Generic-NEXT: ldr r4, [r4]
; Thumb-Generic-NEXT: cmp	r4, r5
; Thumb-Generic-NEXT: blo	.LBB0_2

; Thumb-Generic:      mov r4, #20
; Thumb-Generic-NEXT: mov r5, #0
; Thumb-Generic-NEXT: push {lr}
; Thumb-Generic-NEXT: bl	__morestack
; Thumb-Generic-NEXT: pop {r4}
; Thumb-Generic-NEXT: mov lr, r4
; Thumb-Generic-NEXT: pop	{r4, r5}
; Thumb-Generic-NEXT: mov	pc, lr

; Thumb-Generic:      pop	{r4, r5}

}
