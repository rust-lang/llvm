; RUN: llc -mtriple=i386-pc-linux-gnu < %s -o - | FileCheck --check-prefix=X86-Linux %s
; RUN: llc -mtriple=x86_64-pc-linux-gnu < %s -o - | FileCheck --check-prefix=X64-Linux %s

declare void @use([40096 x i8]*)

define void @test() "probe-stack" {
  %array = alloca [40096 x i8], align 16
  call void @use([40096 x i8]* %array)
  ret void

; X86-Linux-LABEL: test:
; X86-Linux:       movl $40124, %eax
; X86-Linux-NEXT:  calll __probestack
; X86-Linux-NEXT:  subl %eax, %esp

; X64-Linux-LABEL: test:
; X64-Linux:       movabsq $40104, %rax
; X64-Linux-NEXT:  callq __probestack
; X64-Linux-NEXT:  subq %rax, %rsp
	
}

declare void @use_fast([4096 x i8]*)

define void @test_fast() "probe-stack" {
  %array = alloca [4096 x i8], align 16
  call void @use_fast([4096 x i8]* %array)
  ret void

; X86-Linux-LABEL: test_fast:
; X86-Linux:       subl    $4124, %esp
; X86-Linux-NEXT:  orl     $0, 28(%esp)

; X64-Linux-LABEL: test_fast:
; X64-Linux:       subq    $4104, %rsp
; X64-Linux-NEXT:  orq     $0, 8(%rsp)
}
