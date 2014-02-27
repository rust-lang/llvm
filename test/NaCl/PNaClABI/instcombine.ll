; Disabled for the time being b/c PromoteIntegers will remove any odd sized ints
; and b/c we don't currently support translation for PNaCl so there are no platform 
; idiosyncrasies to account for.
; R;UN: opt < %s -instcombine -S | FileCheck %s
; RUN: true
; Test that instcombine does not introduce non-power-of-two integers into
; the module

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

; This test is a counterpart to icmp_shl16 in
; test/Transforms/InstCombine/icmp.ll, which should still pass.
; CHECK-LABEL: @icmp_shl31
; CHECK-NOT: i31
define i1 @icmp_shl31(i32 %x) {
  %shl = shl i32 %x, 1
  %cmp = icmp slt i32 %shl, 36
  ret i1 %cmp
}

; Check that we don't introduce i4, which is a power of 2 but still not allowed.
; CHECK-LABEL: @icmp_shl4
; CHECK-NOT: i4
define i1 @icmp_shl4(i32 %x) {
  %shl = shl i32 %x, 28
  %cmp = icmp slt i32 %shl, 1073741824
  ret i1 %cmp
}
