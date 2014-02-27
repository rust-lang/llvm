; RUN: pnacl-abicheck < %s | FileCheck %s
; XFAIL: *

module asm "foo"
; CHECK: Module contains disallowed top-level inline assembly
