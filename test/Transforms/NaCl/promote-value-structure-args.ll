; RUN: opt -S -promote-structure-arguments < %s | FileCheck %s

; I have to admit testing this pass is problematic: type renaming throws a lot off.

%struct1 = type { i32 }
%struct2 = type { %struct1 (%struct1)* }

@g1 = global %struct2 { %struct1 (%struct1)* @f }
; CHECK: @g1 = global %struct2.1 { %struct1.0 (%struct1.0*)* @f }

define %struct1 @f(%struct1) {
       ret %struct1 %0
}
; CHECK-LABEL: define %struct1.0 @f(%struct1.0* byval)
; CHECK-NEXT: load %struct1.0* %0
; CHECK-NEXT: ret %struct1.0