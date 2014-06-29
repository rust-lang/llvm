; RUN: opt < %s -null-check-elimination -instsimplify -S | FileCheck %s

define i64 @test_arg_simple(i64* nonnull %p) {
entry:
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p, %entry ], [ %p1, %match_else ]
  %b0 = icmp eq i64* %p0, null
  br i1 %b0, label %return, label %match_else

; CHECK-LABEL: @test_arg_simple
; CHECK-NOT: , null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %b1 = icmp ugt i64 %i0, 10
  br i1 %b1, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_arg_simple_ne(i64* nonnull %p) {
entry:
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p, %entry ], [ %p1, %match_else ]
  %b0 = icmp ne i64* %p0, null
  br i1 %b0, label %match_else, label %return

; CHECK-LABEL: @test_arg_simple
; CHECK-NOT: , null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %b1 = icmp ugt i64 %i0, 10
  br i1 %b1, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_arg_simple_fail(i64* %p) {
entry:
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p, %entry ], [ %p1, %match_else ]
  %b0 = icmp eq i64* %p0, null
  br i1 %b0, label %return, label %match_else

; CHECK-LABEL: @test_arg_simple_fail
; CHECK: null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %b1 = icmp ugt i64 %i0, 10
  br i1 %b1, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_arg_simple_fail_control_dep(i64* nonnull %p) {
entry:
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p, %entry ], [ %p1, %match_else ]
  br i1 undef, label %loop_body2, label %match_else

loop_body2:
  %b0 = icmp eq i64* %p0, null
  br i1 %b0, label %return, label %match_else

; CHECK-LABEL: @test_arg_simple_fail_control_dep
; CHECK: null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %b1 = icmp ugt i64 %i0, 10
  br i1 %b1, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_simple(i64* %p) {
entry:
  %p0 = getelementptr inbounds i64* %p, i64 0
  br label %loop_body

loop_body:
  %p1 = phi i64* [ %p0, %entry ], [ %p2, %match_else ]
  %b0 = icmp eq i64* %p1, null
  br i1 %b0, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_simple
; CHECK-NOT: null

match_else:
  %i0 = load i64* %p1
  %p2 = getelementptr inbounds i64* %p1, i64 1
  %b1 = icmp ugt i64 %i0, 10
  br i1 %b1, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_simple_ne(i64* %p) {
entry:
  %p0 = getelementptr inbounds i64* %p, i64 0
  br label %loop_body

loop_body:
  %p1 = phi i64* [ %p0, %entry ], [ %p2, %match_else ]
  %b0 = icmp ne i64* %p1, null
  br i1 %b0, label %match_else, label %return

; CHECK-LABEL: @test_inbounds_simple
; CHECK-NOT: null

match_else:
  %i0 = load i64* %p1
  %p2 = getelementptr inbounds i64* %p1, i64 1
  %b1 = icmp ugt i64 %i0, 10
  br i1 %b1, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_simple_fail(i64* %p) {
entry:
  %p0 = getelementptr i64* %p, i64 0
  br label %loop_body

loop_body:
  %p1 = phi i64* [ %p0, %entry ], [ %p2, %match_else ]
  %b0 = icmp eq i64* %p1, null
  br i1 %b0, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_simple_fail
; CHECK: null

match_else:
  %i0 = load i64* %p1
  %p2 = getelementptr inbounds i64* %p1, i64 1
  %b1 = icmp ugt i64 %i0, 10
  br i1 %b1, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_simple_fail_control_dep(i64* %p) {
entry:
  %p0 = getelementptr i64* %p, i64 0
  br label %loop_body

loop_body:
  %p1 = phi i64* [ %p0, %entry ], [ %p2, %match_else ]
  br i1 undef, label %loop_body2, label %match_else

loop_body2:
  %b0 = icmp eq i64* %p1, null
  br i1 %b0, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_simple_fail_control_dep
; CHECK: null

match_else:
  %i0 = load i64* %p1
  %p2 = getelementptr inbounds i64* %p1, i64 1
  %b1 = icmp ugt i64 %i0, 10
  br i1 %b1, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_or(i64* %p, i64* %q) {
entry:
  %p0 = getelementptr inbounds i64* %p, i64 0
  br label %loop_body

loop_body:
  %p1 = phi i64* [ %p0, %entry ], [ %p2, %match_else ]
  %b0 = icmp eq i64* %p1, %q
  %b1 = icmp eq i64* %p1, null
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_or
; CHECK-NOT: null

match_else:
  %i0 = load i64* %p1
  %p2 = getelementptr inbounds i64* %p1, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_and(i64* %p, i64* %q) {
entry:
  %p0 = getelementptr inbounds i64* %p, i64 0
  br label %loop_body

loop_body:
  %p1 = phi i64* [ %p0, %entry ], [ %p2, %match_else ]
  %b0 = icmp eq i64* %p1, %q
  %b1 = icmp eq i64* %p1, null
  %b2 = and i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_and
; CHECK-NOT: null

match_else:
  %i0 = load i64* %p1
  %p2 = getelementptr inbounds i64* %p1, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_derived_load(i64* %p) {
entry:
  %p0 = getelementptr inbounds i64* %p, i64 0
  br label %loop_body

loop_body:
  %p1 = phi i64* [ %p0, %entry ], [ %p2, %match_else ]
  %b0 = icmp eq i64* %p1, null
  br i1 %b0, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_derived_load
; CHECK-NOT: null

match_else:
  %p2 = getelementptr inbounds i64* %p1, i64 1
  %i0 = load i64* %p2
  %b1 = icmp ugt i64 %i0, 10
  br i1 %b1, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_derived_load_fail(i64* %p) {
entry:
  %p0 = getelementptr inbounds i64* %p, i64 0
  br label %loop_body

loop_body:
  %p1 = phi i64* [ %p0, %entry ], [ %p2, %match_else ]
  %b0 = icmp eq i64* %p1, null
  br i1 %b0, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_derived_load_fail
; CHECK: icmp eq i64* %p1, null

match_else:
  %p2 = getelementptr inbounds i64* %p1, i64 1
  %p3 = getelementptr i64* %p1, i64 1
  %i0 = load i64* %p3
  %b1 = icmp ugt i64 %i0, 10
  br i1 %b1, label %return, label %loop_body

return:
  ret i64 0
}

