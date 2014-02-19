; RUN: llc < %s -mcpu=generic -mtriple=thumb-linux-android -segmented-stacks -verify-machineinstrs | FileCheck %s -check-prefix=Thumb-Linux-Android
; RUN: llc < %s -mcpu=generic -mtriple=thumb-linux-android -segmented-stacks -filetype=obj

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

; Thumb-Linux-Android:      test_basic:

; Thumb-Linux-Android:      push {r4, r5}
; Thumb-Linux-Android-NEXT: mov	r5, sp
; Thumb-Linux-Android-NEXT: ldr r4, .LCPI0_0
; Thumb-Linux-Android-NEXT: ldr r4, [r4]
; Thumb-Linux-Android-NEXT: cmp	r4, r5
; Thumb-Linux-Android-NEXT: blo	.LBB0_2

; Thumb-Linux-Android:      mov r4, #16
; Thumb-Linux-Android-NEXT: mov r5, #0
; Thumb-Linux-Android-NEXT: push {lr}
; Thumb-Linux-Android-NEXT: bl	__morestack
; Thumb-Linux-Android-NEXT: pop {r4}
; Thumb-Linux-Android-NEXT: mov lr, r4
; Thumb-Linux-Android-NEXT: pop	{r4, r5}
; Thumb-Linux-Android-NEXT: mov	pc, lr

; Thumb-Linux-Android:      pop	{r4, r5}

}
