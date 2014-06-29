; RUN: opt < %s -null-check-elimination -instsimplify -S | FileCheck %s

define i64 @test_arg_multiple(i64* nonnull %p, i64* nonnull %q) {
entry:
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p, %entry ], [ %p1, %match_else ]
  %q0 = phi i64* [ %q, %entry ], [ %q1, %match_else ]
  %b0 = icmp eq i64* %p0, null
  %b1 = icmp eq i64* %q0, null
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_arg_multiple
; CHECK-NOT: , null

match_else:
  %i0 = load i64* %p0
  %i1 = load i64* %q0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %q1 = getelementptr inbounds i64* %q0, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_arg_multiple_fail(i64* nonnull %p, i64* %q) {
entry:
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p, %entry ], [ %p1, %match_else ]
  %q0 = phi i64* [ %q, %entry ], [ %q1, %match_else ]
  %b0 = icmp eq i64* %p0, null
  %b1 = icmp eq i64* %q0, null
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_arg_multiple_fail
; CHECK-NOT: icmp eq i64* %p0, null
; CHECK: icmp eq i64* %q0, null

match_else:
  %i0 = load i64* %p0
  %i1 = load i64* %q0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %q1 = getelementptr inbounds i64* %q0, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_multiple(i64* %p, i64* %q) {
entry:
  %p0 = getelementptr inbounds i64* %p, i64 0
  %q0 = getelementptr inbounds i64* %q, i64 0
  br label %loop_body

loop_body:
  %p1 = phi i64* [ %p0, %entry ], [ %p2, %match_else ]
  %q1 = phi i64* [ %q0, %entry ], [ %q2, %match_else ]
  %b0 = icmp eq i64* %p1, null
  %b1 = icmp eq i64* %q1, null
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_multiple
; CHECK-NOT: , null

match_else:
  %i0 = load i64* %p1
  %i1 = load i64* %q1
  %p2 = getelementptr inbounds i64* %p1, i64 1
  %q2 = getelementptr inbounds i64* %q1, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_multiple_fail(i64* %p, i64* %q) {
entry:
  %p0 = getelementptr i64* %p, i64 0
  %q0 = getelementptr inbounds i64* %q, i64 0
  br label %loop_body

loop_body:
  %p1 = phi i64* [ %p0, %entry ], [ %p2, %match_else ]
  %q1 = phi i64* [ %q0, %entry ], [ %q2, %match_else ]
  %b0 = icmp eq i64* %p1, null
  %b1 = icmp eq i64* %q1, null
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_multiple_fail
; CHECK: icmp eq i64* %p1, null
; CHECK-NOT: icmp eq i64* %q1, null

match_else:
  %i0 = load i64* %p1
  %i1 = load i64* %q1
  %p2 = getelementptr inbounds i64* %p1, i64 1
  %q2 = getelementptr inbounds i64* %q1, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_recurrence_or(i64* %p, i64 %len) {
entry:
  %q = getelementptr inbounds i64* %p, i64 %len
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p, %entry ], [ %p1, %match_else ]
  %b0 = icmp eq i64* %p0, %q
  %b1 = icmp eq i64* %p0, null
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_recurrence_or
; CHECK-NOT: null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_recurrence_or_fail1(i64* %p, i64 %len) {
entry:
  %q = getelementptr i64* %p, i64 %len
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p, %entry ], [ %p1, %match_else ]
  %b0 = icmp eq i64* %p0, %q
  %b1 = icmp eq i64* %p0, null
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_recurrence_or_fail1
; CHECK: icmp eq i64* %p0, null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_recurrence_or_fail2(i64* %p, i64 %len) {
entry:
  %q = getelementptr inbounds i64* %p, i64 %len
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p, %entry ], [ %p1, %match_else ]
  %b0 = icmp eq i64* %p0, %q
  %b1 = icmp eq i64* %p0, null
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_recurrence_or_fail2
; CHECK: icmp eq i64* %p0, null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr i64* %p0, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_recurrence_and(i64* %p, i64 %len) {
entry:
  %q = getelementptr inbounds i64* %p, i64 %len
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p, %entry ], [ %p1, %match_else ]
  %b0 = icmp eq i64* %p0, %q
  %b1 = icmp eq i64* %p0, null
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_recurrence_and
; CHECK-NOT: null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_recurrence_or_rev(i64* %p, i64 %len) {
entry:
  %q = getelementptr inbounds i64* %p, i64 %len
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p1, %match_else ], [ %p, %entry ]
  %b0 = icmp eq i64* %p0, null
  %b1 = icmp eq i64* %q, %p0
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_recurrence_or_rev
; CHECK-NOT: null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_recurrence_or_rev_fail1(i64* %p, i64 %len) {
entry:
  %q = getelementptr i64* %p, i64 %len
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p1, %match_else ], [ %p, %entry ]
  %b0 = icmp eq i64* %p0, null
  %b1 = icmp eq i64* %q, %p0
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_recurrence_or_rev_fail1
; CHECK: icmp eq i64* %p0, null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_recurrence_or_rev_fail2(i64* %p, i64 %len) {
entry:
  %q = getelementptr inbounds i64* %p, i64 %len
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p1, %match_else ], [ %p, %entry ]
  %b0 = icmp eq i64* %p0, null
  %b1 = icmp eq i64* %q, %p0
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_recurrence_or_rev_fail2
; CHECK: icmp eq i64* %p0, null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr i64* %p0, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

define i64 @test_inbounds_recurrence_and_rev(i64* %p, i64 %len) {
entry:
  %q = getelementptr inbounds i64* %p, i64 %len
  br label %loop_body

loop_body:
  %p0 = phi i64* [ %p1, %match_else ], [ %p, %entry ]
  %b0 = icmp eq i64* %p0, null
  %b1 = icmp eq i64* %q, %p0
  %b2 = or i1 %b0, %b1
  br i1 %b2, label %return, label %match_else

; CHECK-LABEL: @test_inbounds_recurrence_and_rev
; CHECK-NOT: null

match_else:
  %i0 = load i64* %p0
  %p1 = getelementptr inbounds i64* %p0, i64 1
  %b3 = icmp ugt i64 %i0, 10
  br i1 %b3, label %return, label %loop_body

return:
  ret i64 0
}

