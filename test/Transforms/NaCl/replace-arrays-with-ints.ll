; RUN: opt -replace-aggregates-with-ints -S < %s | FileCheck %s

target datalayout = "p:32:32:32"

; functions taken from rustc output. References to other functions 'n things were removed.

define internal void @_ZN5c_str14check_for_null67hbf4e3101dcb4056349d6b8c20609ff49be8c536440089c7d70f397149cc8b8daaz4v0.0E(i32, i32, i32) {
"function top level":
  %__arg = alloca i8, i32 4, align 4
  %__arg.asint = ptrtoint i8* %__arg to i32
  %i = alloca i8, i32 4, align 4
  %3 = alloca i8, i32 12, align 8
  %.asint = ptrtoint i8* %3 to i32
  %4 = alloca i8, i32 4, align 4
  %5 = alloca i8, i32 8, align 8
  %.asint2 = ptrtoint i8* %5 to i32
  %__llmatch = alloca i8, i32 4, align 4
  %i2 = alloca i8, i32 4, align 4
  %p = alloca i8, i32 4, align 4
  %6 = alloca i8, i32 2, align 8
  %.asint3 = ptrtoint i8* %6 to i32
  %7 = alloca i8, i32 4, align 4
  %__self = alloca i8, i32 4, align 4
  %__llmatch5 = alloca i8, i32 4, align 4
  %c = alloca i8, align 1
  %8 = alloca i8, i32 2, align 8
  %.asint5 = ptrtoint i8* %8 to i32
  %__arg.bc = bitcast i8* %__arg to i32*
  store i32 %2, i32* %__arg.bc, align 1
  %.bc = bitcast i8* %4 to i32*
  %9 = load i32* %.bc, align 1
  store i32 %9, i32* %.bc, align 1
  %.bc14 = bitcast i8* %4 to i32*
  %10 = load i32* %.bc14, align 1
  %i.bc = bitcast i8* %i to i32*
  store i32 %.asint, i32* %i.bc, align 1
  br label %"`loop`"

next:                                             ; preds = %then, %"`loop`"
  ret void

"`loop`":                                         ; preds = %match_else6, %match_else, %"function top level"
  %i.bc15 = bitcast i8* %i to i32*
  %11 = load i32* %i.bc15, align 1
  %12 = load i8* %5, align 1
  %cond = icmp eq i8 %12, 0
  br i1 %cond, label %next, label %match_else

match_else:                                       ; preds = %"`loop`"
  %gep = add i32 %.asint2, 4
  %__llmatch.bc = bitcast i8* %__llmatch to i32*
  store i32 %gep, i32* %__llmatch.bc, align 1
  %__llmatch.bc16 = bitcast i8* %__llmatch to i32*
  %13 = load i32* %__llmatch.bc16, align 1
  %.asptr = inttoptr i32 %13 to i32*
  %14 = load i32* %.asptr, align 1
  %i2.bc = bitcast i8* %i2 to i32*
  store i32 %14, i32* %i2.bc, align 1
  %i2.bc17 = bitcast i8* %i2 to i32*
  %15 = load i32* %i2.bc17, align 1
  %p.bc = bitcast i8* %p to i32*
  store i32 undef, i32* %p.bc, align 1
  %p.bc18 = bitcast i8* %p to i32*
  %16 = load i32* %p.bc18, align 1
  %17 = load i32* %p.bc18, align 1
  %.asptr25 = inttoptr i32 %17 to i8*
  %18 = load i8* %.asptr25, align 1
  %19 = icmp eq i8 %18, 0
  %20 = zext i1 %19 to i8
  %21 = icmp ne i8 %20, 0
  br i1 %21, label %then, label %"`loop`"

then:                                             ; preds = %match_else
  %22 = alloca i8, i32 4, align 4
  %.asint6 = ptrtoint i8* %22 to i32
  %.bc19 = bitcast i8* %22 to i32*
  %23 = load i32* %.bc19, align 1
  %.bc20 = bitcast i8* %7 to i32*
  store i32 %23, i32* %.bc20, align 1
  %.bc21 = bitcast i8* %7 to i32*
  %24 = load i32* %.bc21, align 1
  %__self.bc = bitcast i8* %__self to i32*
  store i32 %24, i32* %__self.bc, align 1
  %__self.bc22 = bitcast i8* %__self to i32*
  %25 = load i32* %__self.bc22, align 1
  %26 = alloca i8, i32 2, align 8
  %.asint7 = ptrtoint i8* %26 to i32
  %expanded13 = ptrtoint [12 x i8]* null to i32
  %.field = load i8* %26, align 1
  %gep35 = add i32 %.asint7, 1
  %gep35.asptr = inttoptr i32 %gep35 to [0 x i8]*
; CHECK-NOT: %gep35.asptr = inttoptr i32 %gep35 to [0 x i8]*
  %.field12 = load [0 x i8]* %gep35.asptr, align 1
; CHECK-NOT: %.field12 = load [0 x i8]* %gep35.asptr, align 1
  %gep37 = add i32 %.asint7, 1
  %gep37.asptr = inttoptr i32 %gep37 to i8*
  %.field15 = load i8* %gep37.asptr, align 1
  store i8 %.field, i8* %6, align 1
  %gep40 = add i32 %.asint3, 1
  %gep40.asptr = inttoptr i32 %gep40 to [0 x i8]*
; CHECK-NOT: %gep40.asptr = inttoptr i32 %gep40 to [0 x i8]*
  store [0 x i8] %.field12, [0 x i8]* %gep40.asptr, align 1
; CHECK-NOT: store [0 x i8] %.field12, [0 x i8]* %gep40.asptr, align 1
  %gep42 = add i32 %.asint3, 1
  %gep42.asptr = inttoptr i32 %gep42 to i8*
  store i8 %.field15, i8* %gep42.asptr, align 1
  %.field21 = load i8* %6, align 1
  %gep45 = add i32 %.asint3, 1
  %gep45.asptr = inttoptr i32 %gep45 to [0 x i8]*
; CHECK-NOT: %gep45.asptr = inttoptr i32 %gep45 to [0 x i8]*
  %.field24 = load [0 x i8]* %gep45.asptr, align 1
; CHECK-NOT: %.field24 = load [0 x i8]* %gep45.asptr, align 1
  %gep47 = add i32 %.asint3, 1
  %gep47.asptr = inttoptr i32 %gep47 to i8*
  %.field27 = load i8* %gep47.asptr, align 1
  store i8 %.field21, i8* %8, align 1
  %gep50 = add i32 %.asint5, 1
  %gep50.asptr = inttoptr i32 %gep50 to [0 x i8]*
; CHECK-NOT: %gep50.asptr = inttoptr i32 %gep50 to [0 x i8]*
  store [0 x i8] %.field24, [0 x i8]* %gep50.asptr, align 1
; CHECK-NOT: store [0 x i8] %.field24, [0 x i8]* %gep50.asptr, align 1
  %gep52 = add i32 %.asint5, 1
  %gep52.asptr = inttoptr i32 %gep52 to i8*
  store i8 %.field27, i8* %gep52.asptr, align 1
  %27 = load i8* %8, align 1
  %cond10 = icmp eq i8 %27, 0
  br i1 %cond10, label %next, label %match_else6

match_else6:                                      ; preds = %then
  %gep55 = add i32 %.asint5, 1
  %__llmatch5.bc = bitcast i8* %__llmatch5 to i32*
  store i32 %gep55, i32* %__llmatch5.bc, align 1
  %__llmatch5.bc23 = bitcast i8* %__llmatch5 to i32*
  %28 = load i32* %__llmatch5.bc23, align 1
  %.asptr26 = inttoptr i32 %28 to i8*
  %29 = load i8* %.asptr26, align 1
  store i8 %29, i8* %c, align 1
  %p.bc24 = bitcast i8* %p to i32*
  %30 = load i32* %p.bc24, align 1
  %31 = load i8* %c, align 1
  %.asptr27 = inttoptr i32 %30 to i8*
  store i8 %31, i8* %.asptr27, align 1
  br label %"`loop`"
}

define internal i32 @"_ZN3str6traits23TotalOrd$__extensions__3cmp69h96bba0320007768f6adbf4b6f49860e1e3d6288b2613ce406ea5e56333029e20lBaD4v0.0E"(i32, i32) {
"function top level":
  %__make_return_pointer = alloca i8, align 1
  %__self = alloca i8, i32 4, align 4
  %__arg = alloca i8, i32 4, align 4
  %i = alloca i8, i32 4, align 4
  %2 = alloca i8, i32 40, align 8
  %.asint = ptrtoint i8* %2 to i32
  %3 = alloca i8, i32 20, align 8
  %.asint1 = ptrtoint i8* %3 to i32
  %4 = alloca i8, i32 20, align 8
  %.asint2 = ptrtoint i8* %4 to i32
  %5 = alloca i8, i32 3, align 8
  %.asint3 = ptrtoint i8* %5 to i32
  %__llmatch = alloca i8, i32 4, align 4
  %s_b = alloca i8, align 1
  %s_b.asint = ptrtoint i8* %s_b to i32
  %__llmatch2 = alloca i8, i32 4, align 4
  %o_b = alloca i8, align 1
  %o_b.asint = ptrtoint i8* %o_b to i32
  %6 = alloca i8, i32 3, align 8
  %.asint4 = ptrtoint i8* %6 to i32
  %7 = alloca i8, align 1
  %8 = alloca i8, align 1
  %9 = alloca i8, i32 4, align 4
  %10 = alloca i8, i32 4, align 4
  %.asint8 = ptrtoint i8* %10 to i32
  %11 = alloca i8, i32 4, align 4
  %12 = alloca i8, i32 4, align 4
  %.asint10 = ptrtoint i8* %12 to i32
  %__self.bc = bitcast i8* %__self to i32*
  store i32 %0, i32* %__self.bc, align 1
  %__arg.bc = bitcast i8* %__arg to i32*
  store i32 %1, i32* %__arg.bc, align 1
  %__self.bc19 = bitcast i8* %__self to i32*
  %13 = load i32* %__self.bc19, align 1
  %__arg.bc20 = bitcast i8* %__arg to i32*
  %14 = load i32* %__arg.bc20, align 1
  %i.bc = bitcast i8* %i to i32*
  store i32 %.asint, i32* %i.bc, align 1
  br label %"`loop`"

return:                                           ; preds = %match_case8, %match_case7, %match_case
  %15 = load i8* %__make_return_pointer, align 1
  %.ret_ext = zext i8 %15 to i32
  ret i32 %.ret_ext

"`loop`":                                         ; preds = %match_else, %"function top level"
  %i.bc21 = bitcast i8* %i to i32*
  %16 = load i32* %i.bc21, align 1
  %17 = alloca i8, i32 3, align 8
  %.asint11 = ptrtoint i8* %17 to i32
  %.field = load i8* %17, align 1
  %gep = add i32 %.asint11, 1
  %gep.asptr = inttoptr i32 %gep to [0 x i8]*
; CHECK-NOT: %gep.asptr = inttoptr i32 %gep to [0 x i8]*
  %.field11 = load [0 x i8]* %gep.asptr, align 1
; CHECK-NOT: %.field11 = load [0 x i8]* %gep.asptr, align 1
  %gep4 = add i32 %.asint11, 1
  %gep4.asptr = inttoptr i32 %gep4 to [2 x i8]*
; CHECK: %gep4.asptr = inttoptr i32 %gep4 to i16*
  %.field14 = load [2 x i8]* %gep4.asptr, align 1
; CHECK %.field14 = load i16* %gep4.asptr, align 1
  store i8 %.field, i8* %5, align 1
  %gep7 = add i32 %.asint3, 1
  %gep7.asptr = inttoptr i32 %gep7 to [0 x i8]*
; CHECK-NOT: %gep7.asptr = inttoptr i32 %gep7 to [0 x i8]*
  store [0 x i8] %.field11, [0 x i8]* %gep7.asptr, align 1
; CHECK-NOT: store [0 x i8] %.field11, [0 x i8]* %gep7.asptr, align 1
  %gep9 = add i32 %.asint3, 1
  %gep9.asptr = inttoptr i32 %gep9 to [2 x i8]*
; CHECK: %gep9.asptr = inttoptr i32 %gep9 to i16*
  store [2 x i8] %.field14, [2 x i8]* %gep9.asptr, align 1
; CHECK: store i16 %.field14, i16* %gep9.asptr, align 1
  %.field20 = load i8* %5, align 1
  %gep12 = add i32 %.asint3, 1
  %gep12.asptr = inttoptr i32 %gep12 to [0 x i8]*
; CHECK-NOT: %gep12.asptr = inttoptr i32 %gep12 to [0 x i8]*
  %.field23 = load [0 x i8]* %gep12.asptr, align 1
; CHECK-NOT: %.field23 = load [0 x i8]* %gep12.asptr, align 1
  %gep14 = add i32 %.asint3, 1
  %gep14.asptr = inttoptr i32 %gep14 to [2 x i8]*
; CHECK: %gep14.asptr = inttoptr i32 %gep14 to i16*
  %.field26 = load [2 x i8]* %gep14.asptr, align 1
; CHECK: %.field26 = load i16* %gep14.asptr, align 1
  store i8 %.field20, i8* %6, align 1
  %gep17 = add i32 %.asint4, 1
  %gep17.asptr = inttoptr i32 %gep17 to [0 x i8]*
; CHECK-NOT: %gep17.asptr = inttoptr i32 %gep17 to [0 x i8]*
  store [0 x i8] %.field23, [0 x i8]* %gep17.asptr, align 1
; CHECK-NOT: store [0 x i8] %.field23, [0 x i8]* %gep17.asptr, align 1
  %gep19 = add i32 %.asint4, 1
  %gep19.asptr = inttoptr i32 %gep19 to [2 x i8]*
; CHECK: %gep19.asptr = inttoptr i32 %gep19 to i16*
  store [2 x i8] %.field26, [2 x i8]* %gep19.asptr, align 1
; CHECK: store i16 %.field26, i16* %gep19.asptr, align 1
  %18 = load i8* %6, align 1
  %cond = icmp eq i8 %18, 0
  br i1 %cond, label %match_case, label %match_else

match_else:                                       ; preds = %"`loop`"
  %gep22 = add i32 %.asint4, 1
  %gep25 = add i32 %gep22, 1
  %__llmatch.bc = bitcast i8* %__llmatch to i32*
  store i32 %gep22, i32* %__llmatch.bc, align 1
  %__llmatch2.bc = bitcast i8* %__llmatch2 to i32*
  store i32 %gep25, i32* %__llmatch2.bc, align 1
  %__llmatch.bc22 = bitcast i8* %__llmatch to i32*
  %19 = load i32* %__llmatch.bc22, align 1
  %.asptr = inttoptr i32 %19 to i8*
  %20 = load i8* %.asptr, align 1
  store i8 %20, i8* %s_b, align 1
  %__llmatch2.bc23 = bitcast i8* %__llmatch2 to i32*
  %21 = load i32* %__llmatch2.bc23, align 1
  %.asptr31 = inttoptr i32 %21 to i8*
  %22 = load i8* %.asptr31, align 1
  store i8 %22, i8* %o_b, align 1
  %.ret_trunc = trunc i32 undef to i8
  store i8 %.ret_trunc, i8* %7, align 1
  %23  = load i8* %7, align 1
  %24 = load i8* %7, align 1
  store i8 %24, i8* %8, align 1
  %25 = load i8* %8, align 1
  switch i8 %25, label %"`loop`" [
    i8 1, label %match_case7
    i8 -1, label %match_case8
  ]

match_case:                                       ; preds = %"`loop`"
  %__self.bc24 = bitcast i8* %__self to i32*
  %26 = load i32* %__self.bc24, align 1
  %.bc = bitcast i8* %9 to i32*
  store i32 undef, i32* %.bc, align 1
  %.bc25 = bitcast i8* %9 to i32*
  %27 = load i32* %.bc25, align 1
  %28 = load i32* %.bc25, align 1
  %.bc26 = bitcast i8* %10 to i32*
  store i32 %28, i32* %.bc26, align 1
  %__arg.bc27 = bitcast i8* %__arg to i32*
  %29 = load i32* %__arg.bc27, align 1
  %.bc28 = bitcast i8* %11 to i32*
  store i32 undef, i32* %.bc28, align 1
  %.bc29 = bitcast i8* %11 to i32*
  %30 = load i32* %.bc29, align 1
  %31 = load i32* %.bc29, align 1
  %.bc30 = bitcast i8* %12 to i32*
  store i32 %31, i32* %.bc30, align 1
  %.ret_trunc32 = trunc i32 undef to i8
  store i8 %.ret_trunc32, i8* %__make_return_pointer, align 1
  br label %return

match_case7:                                      ; preds = %match_else
  store i8 1, i8* %__make_return_pointer, align 1
  br label %return

match_case8:                                      ; preds = %match_else
  store i8 -1, i8* %__make_return_pointer, align 1
  br label %return
}