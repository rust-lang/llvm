; RUN: opt -S -promote-returned-structures < %s | FileCheck %s

; I have to admit testing this pass is problematic: type renaming throws a lot off.

@g1 = global { {}* ()* } { {}* ()* @_ZNf1 }
; CHECK: @g1 = global %0 { i8* ()* @_ZNf1 }
@g2 = global { { i32 } ()* } { { i32 } ()* @_ZNf2 }
; CHECK: @g2 = global %1 { void (%2*)* @_ZNf2 }
@g3 = global { { i32 } ({ i32 }*)* } { { i32 } ({ i32 }*)* @_ZNf3 }
; CHECK: @g3 = global %3 { void (%2*, %2*)* @_ZNf3 }

; leave {}* alone:
define {}* @_ZNf1() {
; CHECK-LABEL: define i8* @_ZNf1()
  ret {}* null
; CHECK: ret i8* null
}

define { i32 } @_ZNf2() {
  ret { i32 } zeroinitializer
}
; CHECK-LABEL: define void @_ZNf2(%2* sret)
; CHECK-NEXT: store %2 zeroinitializer, %2* %0
; CHECK-NEXT: ret void

; shift attributes right:
define { i32 } @_ZNf3({ i32 }* byval) {
  %a1 = load { i32 }* %0
  ret { i32 } %a1
}
; CHECK-LABEL: define void @_ZNf3(%2* sret, %2* byval)
; CHECK-NEXT: %a1 = load %2* %1
; CHECK-NEXT: store %2 %a1, %2* %0
; CHECK-NEXT: ret void