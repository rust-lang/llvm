; RUN: opt -promote-simple-structs -S %s | FileCheck %s

%not_promoted = type { i32, i64, i32* }
%nested1 = type { %not_promoted }
%nested2 = type { %nested1 }
%promoted1 = type { i32* }
%promoted2 = type { i64  }
%promoted3 = type { %not_promoted (%promoted1)*, i64 (%promoted2*)* }
%promoted4 = type { %promoted1, %promoted3 }
%linked_list = type { %promoted1, %linked_list* }
; CHECK: %"enum.trie::Child<()>.3" = type { i8, [3 x i8], i32 }
%"enum.trie::Child<()>" = type { i8, [3 x i8], [1 x i32] }
; C;HECK: %"struct.trie::TrieNode<()>.2" = type { i32, [16 x %"enum.trie::Child<()>.3"] }
%"struct.trie::TrieNode<()>" = type { i32, [16 x %"enum.trie::Child<()>"] }
; C;HECK: %tydesc = type { i32, i32, void ({}*, i8*)*, void ({}*, i8*)*, void ({}*, i8*)*, void ({}*, i8*)*, i32, { i8*, i32 } }
%tydesc = type { i32, i32, void ({}*, i8*)*, void ({}*, i8*)*, void ({}*, i8*)*, void ({}*, i8*)*, i32, { i8*, i32 } }
%"enum.option::Option<(*libc::types::common::c95::c_void,~local_data::LocalData:Send,local_data::LoanState)>" = type { i8, [3 x i8], [4 x i32] }
%"enum.libc::types::common::c95::c_void" = type {}

@g1 = global i32 42, align 4

; CHECK-LABEL: define %not_promoted @not_promoted_fun1(%not_promoted* %a1, i32 %a2)
define %not_promoted @not_promoted_fun1(%not_promoted* %a1, i32 %a2) {
; CHECK: %1 = load %not_promoted* %a1
       %1 = load %not_promoted* %a1
; CHECK: %2 = extractvalue %not_promoted %1, 0
       %2 = extractvalue %not_promoted %1, 0
; CHECK: %3 = add i32 %2, %a2
       %3 = add i32 %2, %a2
; CHECK: %4 = insertvalue %not_promoted %1, i32 %3, 0
       %4 = insertvalue %not_promoted %1, i32 %3, 0
; CHECK: ret %not_promoted %4
       ret %not_promoted %4
}

; CHECK-LABEL: define i32* @f1(i32* %a1)
define %promoted1 @f1(i32* %a1) {
; CHECK: ret i32* %a1
       %1 = insertvalue %promoted1 undef, i32* %a1, 0
       ret %promoted1 %1
}
; CHECK-LABEL: define i32* @f2(i32* %a1)
define i32* @f2(%promoted1 %a1) {
       %1 = extractvalue %promoted1 %a1, 0
; CHECK: ret i32* %a1
       ret i32* %1
}

; CHECK-LABEL: define i64 @f3(i64* %a1)
define i64 @f3(%promoted2* %a1) {
; CHECK: %1 = load i64* %a1
       %1 = load %promoted2* %a1
; CHECK-NOT: %2 = extractvalue %promoted2 %1, 0
       %2 = extractvalue %promoted2 %1, 0
; CHECK: ret i64 %1
       ret i64 %2
}

; CHECK-LABEL: define i64 @f4(i64** %a1)
define i64 @f4(%promoted2** %a1) {
; CHECK: %1 = load i64** %a1
       %1 = load %promoted2** %a1
; CHECK: %2 = load i64* %1
       %2 = load %promoted2*  %1
; CHECK-NOT: %3 = extractvalue %promoted2 %2, 0
       %3 = extractvalue %promoted2 %2, 0
; CHECK: ret i64 %2
       ret i64 %3
}

; CHECK-LABEL: define i32* @f5()
define %promoted1 @f5() {
       %1 = insertvalue %promoted1 undef, i32* @g1, 0
; CHECK: ret i32* @g1
       ret %promoted1 %1
}
; CHECK-LABEL: define %not_promoted @f6(i32* %a1, i64 %a2)
define %not_promoted @f6(%promoted1 %a1, %promoted2 %a2) {
; CHECK: %1 = call i32* @f2(i32* %a1)
       %1 = call i32* @f2(%promoted1 %a1)
; CHECK: %2 = insertvalue %not_promoted undef, i32* %1, 2
       %2 = insertvalue %not_promoted undef, i32* %1, 2
; CHECK: %3 = alloca i64
       %3 = alloca %promoted2
; CHECK: store i64 %a2, i64* %3
       store %promoted2 %a2, %promoted2* %3
; CHECK: %4 = call i64 @f3(i64* %3)
       %4 = call i64 @f3(%promoted2* %3)
; CHECK: %5 = insertvalue %not_promoted %2, i64 %4, 1
       %5 = insertvalue %not_promoted %2, i64 %4, 1
; CHECK: %6 = insertvalue %not_promoted %5, i32 10, 0
       %6 = insertvalue %not_promoted %5, i32 10, 0
; CHECK: ret %not_promoted %6
       ret %not_promoted %6
}
; CHECK-LABEL: define %not_promoted @f7(i32* %a1)
define %not_promoted @f7(%promoted1 %a1) {
Entry:
; CHECK: %0 = call i32* @f2(i32* %a1)
        %0 = call i32* @f2(%promoted1 %a1)
        %1 = load i32* %0
        %2 = icmp eq i32 %1, 0
        br i1 %2, label %Null, label %NotNull

Null:
; CHECK: %3 = call i32* @f1(i32* @g1)
        %3 = call %promoted1 @f1(i32* @g1)
; CHECK: %4 = call %not_promoted @f7(i32* %3)
        %4 = call %not_promoted @f7(%promoted1 %3)
        br label %Exit

NotNull:
        %5 = phi i32* [ %0, %Entry ]
; CHECK: %6 = call i32* @f1(i32* %5)
        %6 = call %promoted1 @f1(i32* %5)
; CHECK-NOT: %7 = insertvalue %promoted1 undef, i32* %5, 0
        %7 = insertvalue %promoted1 undef, i32* %5, 0
; CHECK-NOT: %8 = insertvalue %promoted2 undef, i64 16, 0
        %8 = insertvalue %promoted2 undef, i64 16, 0
; CHECK: %7 = call %not_promoted @f6(i32* %5, i64 16)
        %9 = call %not_promoted @f6(%promoted1 %7, %promoted2 %8)
        br label %Exit

Exit:
        %10 = phi %not_promoted [ %4, %Null ], [ %9, %NotNull ]
; CHECK: %9 = phi i32* [ %3, %Null ], [ %6, %NotNull ]
        %11 = phi %promoted1 [ %3, %Null ], [ %6, %NotNull ]
; CHECK: %10 = call i32* @f2(i32* %9)
        %12 = call i32* @f2(%promoted1 %11)
        %13 = load i32* %12
        %14 = insertvalue %not_promoted %10, i32 %13, 0
        ret %not_promoted %14
}
; CHECK-LABEL: define %not_promoted @f8(%not_promoted (i32*)* %a1)
define %not_promoted @f8(%not_promoted (%promoted1)* %a1) {
       %1 = alloca i32
       store i32 42, i32* %1
; CHECK: %2 = call i32* @f1(i32* %1)
       %2 = call %promoted1 @f1(i32* %1)
; CHECK: %3 = call %not_promoted %a1(i32* %2)
       %3 = call %not_promoted %a1(%promoted1 %2)
       ret %not_promoted %3
}

define %not_promoted @f9() {
; CHECK: %1 = call %not_promoted @f8(%not_promoted (i32*)* @f7)
       %1 = call %not_promoted @f8(%not_promoted (%promoted1)* @f7)
       ret %not_promoted %1
}
; CHECK-LABEL: define %promoted3.0 @f10()
define %promoted3 @f10() {
; CHECK: %1 = insertvalue %promoted3.0 undef, %not_promoted (i32*)* @f7, 0
       %1 = insertvalue %promoted3 undef, %not_promoted (%promoted1)* @f7, 0
; CHECK: %2 = insertvalue %promoted3.0 %1, i64 (i64*)* @f3, 1
       %2 = insertvalue %promoted3 %1, i64 (%promoted2*)* @f3, 1
; CHECK: ret %promoted3.0 %2
       ret %promoted3 %2
}
; CHECK-LABEL: define %not_promoted @f11()
define %not_promoted @f11() {
; CHECK: %1 = call %promoted3.0 @f10()
       %1 = call %promoted3 @f10()
; CHECK: %2 = extractvalue %promoted3.0 %1, 0
       %2 = extractvalue %promoted3 %1, 0
; CHECK: %3 = extractvalue %promoted3.0 %1, 1
       %3 = extractvalue %promoted3 %1, 1
; CHECK: %4 = call %not_promoted @f8(%not_promoted (i32*)* %2)
       %4 = call %not_promoted @f8(%not_promoted (%promoted1)* %2)
; CHECK: ret %not_promoted %4
       ret %not_promoted %4
}
define %promoted1 @f12() {
       %1 = bitcast %promoted3 ()* @f10 to i32*
       %2 = call %promoted1 @f1(i32* %1)
       %3 = call %not_promoted @f7(%promoted1 %2)
       ret %promoted1 %2
}
define void @f13() {
       %1 = call %promoted1 @f12()
       %2 = call i32* @f2(%promoted1 %1)
       %3 = bitcast i32* %2 to %promoted3 ()*
       %4 = call %promoted3 %3()
       ret void
}
define void @f14(%linked_list* %a1, %linked_list* %a2) {
       %1 = load %linked_list* %a1
       %2 = insertvalue %linked_list %1, %linked_list* %a2, 1
       store %linked_list %2, %linked_list* %a1
       ret void
}

; Function Attrs: inlinehint uwtable
define internal void @_ZN4trie8TrieNode3new69h3210031e2fef4109c0163f68203dffa433c4a299f6bad390a11a2f2c49d0df2cJyaj8v0.9.preE(%"struct.trie::TrieNode<()>"* noalias sret, { i32, %tydesc*, i8*, i8*, i8 }*) unnamed_addr #4 {
"function top level":
  %2 = getelementptr inbounds %"struct.trie::TrieNode<()>"* %0, i32 0, i32 0
  store i32 0, i32* %2
  %3 = getelementptr inbounds %"struct.trie::TrieNode<()>"* %0, i32 0, i32 1
  %4 = getelementptr inbounds [16 x %"enum.trie::Child<()>"]* %3, i32 0, i32 0
  %5 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 0
  %6 = getelementptr inbounds %"enum.trie::Child<()>"* %5, i32 0, i32 0
  store i8 2, i8* %6
  %7 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 1
  %8 = getelementptr inbounds %"enum.trie::Child<()>"* %7, i32 0, i32 0
  store i8 2, i8* %8
  %9 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 2
  %10 = getelementptr inbounds %"enum.trie::Child<()>"* %9, i32 0, i32 0
  store i8 2, i8* %10
  %11 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 3
  %12 = getelementptr inbounds %"enum.trie::Child<()>"* %11, i32 0, i32 0
  store i8 2, i8* %12
  %13 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 4
  %14 = getelementptr inbounds %"enum.trie::Child<()>"* %13, i32 0, i32 0
  store i8 2, i8* %14
  %15 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 5
  %16 = getelementptr inbounds %"enum.trie::Child<()>"* %15, i32 0, i32 0
  store i8 2, i8* %16
  %17 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 6
  %18 = getelementptr inbounds %"enum.trie::Child<()>"* %17, i32 0, i32 0
  store i8 2, i8* %18
  %19 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 7
  %20 = getelementptr inbounds %"enum.trie::Child<()>"* %19, i32 0, i32 0
  store i8 2, i8* %20
  %21 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 8
  %22 = getelementptr inbounds %"enum.trie::Child<()>"* %21, i32 0, i32 0
  store i8 2, i8* %22
  %23 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 9
  %24 = getelementptr inbounds %"enum.trie::Child<()>"* %23, i32 0, i32 0
  store i8 2, i8* %24
  %25 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 10
  %26 = getelementptr inbounds %"enum.trie::Child<()>"* %25, i32 0, i32 0
  store i8 2, i8* %26
  %27 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 11
  %28 = getelementptr inbounds %"enum.trie::Child<()>"* %27, i32 0, i32 0
  store i8 2, i8* %28
  %29 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 12
  %30 = getelementptr inbounds %"enum.trie::Child<()>"* %29, i32 0, i32 0
  store i8 2, i8* %30
  %31 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 13
  %32 = getelementptr inbounds %"enum.trie::Child<()>"* %31, i32 0, i32 0
  store i8 2, i8* %32
  %33 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 14
  %34 = getelementptr inbounds %"enum.trie::Child<()>"* %33, i32 0, i32 0
  store i8 2, i8* %34
  %35 = getelementptr inbounds %"enum.trie::Child<()>"* %4, i32 15
  %36 = getelementptr inbounds %"enum.trie::Child<()>"* %35, i32 0, i32 0
  store i8 2, i8* %36
  ret void
}
; Function Attrs: inlinehint uwtable
define internal %"enum.libc::types::common::c95::c_void"* @_ZN7reflect14MovePtrAdaptor5align4anon7expr_fn6zxa7a9E({ i32, %tydesc*, i8*, i8*, i8 }*, %"enum.libc::types::common::c95::c_void"*) unnamed_addr #4 {
; CHECK-LABEL: @_ZN7reflect14MovePtrAdaptor5align4anon7expr_fn6zxa7a9E
"function top level":
  %__arg = alloca %"enum.libc::types::common::c95::c_void"*
  %p = alloca %"enum.libc::types::common::c95::c_void"*
; CHECK: %__debuginfo_env_ptr = alloca i32**
  %__debuginfo_env_ptr = alloca { i32* }*
  %2 = alloca i32
  store %"enum.libc::types::common::c95::c_void"* %1, %"enum.libc::types::common::c95::c_void"** %__arg
  %3 = load %"enum.libc::types::common::c95::c_void"** %__arg
  store %"enum.libc::types::common::c95::c_void"* %3, %"enum.libc::types::common::c95::c_void"** %p
  %4 = bitcast { i32, %tydesc*, i8*, i8*, i8 }* %0 to { i32, %tydesc*, i8*, i8*, { i32* } }*
  %5 = getelementptr inbounds { i32, %tydesc*, i8*, i8*, { i32* } }* %4, i32 0, i32 4
  store { i32* }* %5, { i32* }** %__debuginfo_env_ptr
  %6 = getelementptr inbounds { i32* }* %5, i32 0, i32 0
  %7 = load i32** %6
  %8 = load %"enum.libc::types::common::c95::c_void"** %p
  %9 = ptrtoint %"enum.libc::types::common::c95::c_void"* %8 to i32
  %10 = load i32* %7
  %11 = load i32* %7
  store i32 %11, i32* %2
  %12 = load i32* %2
  %13 = inttoptr i32 %12 to %"enum.libc::types::common::c95::c_void"*
  ret %"enum.libc::types::common::c95::c_void"* %13
}

define internal void @"_ZN142unboxed_vec$LT$option..Option$LT$$LP$$RP$libc..types..common..c95..c_void$C$$UP$local_data..LocalData.Send$C$local_data..LoanState$RP$$GT$$GT$9glue_drop67hdc2acc74788e8d0863032f96df4ac96f58cf283c1470b3e70ab84f6398ec1bdeaJE"({}*, { i32, i32, [0 x %"enum.option::Option<(*libc::types::common::c95::c_void,~local_data::LocalData:Send,local_data::LoanState)>"] }*) unnamed_addr {
"function top level":
  %2 = getelementptr inbounds { i32, i32, [0 x %"enum.option::Option<(*libc::types::common::c95::c_void,~local_data::LocalData:Send,local_data::LoanState)>"] }* %1, i32 0, i32 0
  %3 = load i32* %2
  %4 = getelementptr inbounds { i32, i32, [0 x %"enum.option::Option<(*libc::types::common::c95::c_void,~local_data::LocalData:Send,local_data::LoanState)>"] }* %1, i32 0, i32 2, i32 0
  %5 = bitcast %"enum.option::Option<(*libc::types::common::c95::c_void,~local_data::LocalData:Send,local_data::LoanState)>"* %4 to i8*
  %6 = getelementptr inbounds i8* %5, i32 %3
  %7 = bitcast i8* %6 to %"enum.option::Option<(*libc::types::common::c95::c_void,~local_data::LocalData:Send,local_data::LoanState)>"*
  br label %iter_vec_loop_header

iter_vec_loop_header:                             ; preds = %iter_vec_loop_body, %"function top level"
  %8 = phi %"enum.option::Option<(*libc::types::common::c95::c_void,~local_data::LocalData:Send,local_data::LoanState)>"* [ %4, %"function top level" ], [ %10, %iter_vec_loop_body ]
  %9 = icmp ult %"enum.option::Option<(*libc::types::common::c95::c_void,~local_data::LocalData:Send,local_data::LoanState)>"* %8, %7
  br i1 %9, label %iter_vec_loop_body, label %iter_vec_next

iter_vec_loop_body:                               ; preds = %iter_vec_loop_header
  %10 = getelementptr inbounds %"enum.option::Option<(*libc::types::common::c95::c_void,~local_data::LocalData:Send,local_data::LoanState)>"* %8, i32 1
  br label %iter_vec_loop_header

iter_vec_next:                                    ; preds = %iter_vec_loop_header
  ret void
}
define internal void @"_ZN51_$x5btrie..Child$LT$$LP$$RP$$GT$$C$$x20..$x2016$x5d9glue_drop67hf75fc21ffe9fcb2cdb87b4fc93776499375d61a8fd9b32988157b4dec027e4e8arE"({}*, [16 x %"enum.trie::Child<()>"]*) unnamed_addr {
"function top level":
  %2 = getelementptr inbounds [16 x %"enum.trie::Child<()>"]* %1, i32 0, i32 0
  %3 = bitcast %"enum.trie::Child<()>"* %2 to i8*
  %4 = getelementptr inbounds i8* %3, i32 128
  %5 = bitcast i8* %4 to %"enum.trie::Child<()>"*
  br label %iter_vec_loop_header

iter_vec_loop_header:                             ; preds = %iter_vec_loop_body, %"function top level"
  %6 = phi %"enum.trie::Child<()>"* [ %2, %"function top level" ], [ %8, %iter_vec_loop_body ]
  %7 = icmp ult %"enum.trie::Child<()>"* %6, %5
  br i1 %7, label %iter_vec_loop_body, label %iter_vec_next

iter_vec_loop_body:                               ; preds = %iter_vec_loop_header
  %8 = getelementptr inbounds %"enum.trie::Child<()>"* %6, i32 1
  br label %iter_vec_loop_header

iter_vec_next:                                    ; preds = %iter_vec_loop_header
  ret void
}