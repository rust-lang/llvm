; RUN: llc < %s -mtriple=i686-pc-windows-msvc | FileCheck %s

define < 3 x i32 > @clobber() {
  %1 = alloca i32
  %2 = load volatile i32, i32* %1
  ; CHECK-NOT: popl %esp
  ; CHECK: addl $4, %esp
  ret < 3 x i32 > undef
}
