; RUN: opt %s -pnacl-abi-simplify-preopt -S | FileCheck %s

; Checks that PNaCl ABI pre-opt simplification correctly internalizes
; symbols except _start.

target datalayout = "p:32:32:32"

define void @main() {
; CHECK: define internal void @main
  ret void
}

define external void @foobarbaz() {
; CHECK: define internal void @foobarbaz
  ret void
}

define void @_start() {
; CHECK: define void @_start
  ret void
}

