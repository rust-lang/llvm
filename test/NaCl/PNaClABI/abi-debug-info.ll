; RUN: pnacl-abicheck < %s | FileCheck %s
; RUN: pnacl-abicheck -pnaclabi-allow-debug-metadata < %s | \
; RUN:   FileCheck %s --check-prefix=DBG
; XFAIL: *
; DBG-NOT: disallowed


declare void @llvm.dbg.declare(metadata, metadata)
declare void @llvm.dbg.value(metadata, i64, metadata)

; CHECK: Function llvm.dbg.declare is a disallowed LLVM intrinsic
; CHECK: Function llvm.dbg.value is a disallowed LLVM intrinsic

; We need to force LLParser to leave a call to llvm.dbg.value && llvm.dbg.declare
; This was taken from 2010-05-03-OriginDIE.ll
%struct.anon = type { i64, i32, i32, i32, [1 x i32] }
%struct.gpm_t = type { i32, i8*, [16 x i8], i32, i64, i64, i64, i64, i64, i64, i32, i16, i16, [8 x %struct.gpmr_t] }
%struct.gpmr_t = type { [48 x i8], [48 x i8], [16 x i8], i64, i64, i64, i64, i16 }
%struct.gpt_t = type { [8 x i8], i32, i32, i32, i32, i64, i64, i64, i64, [16 x i8], %struct.anon }
define fastcc void @gpt2gpm(%struct.gpm_t* %gpm, %struct.gpt_t* %gpt) nounwind optsize ssp {
entry:
  %data_addr.i18 = alloca i64, align 8            ; <i64*> [#uses=1]
  %data_addr.i17 = alloca i64, align 8            ; <i64*> [#uses=2]
  %data_addr.i16 = alloca i64, align 8            ; <i64*> [#uses=0]
  %data_addr.i15 = alloca i32, align 4            ; <i32*> [#uses=0]
  %data_addr.i = alloca i64, align 8              ; <i64*> [#uses=0]
  %0 = getelementptr inbounds %struct.gpm_t* %gpm, i32 0, i32 2, i32 0 ; <i8*> [#uses=1]
  %1 = getelementptr inbounds %struct.gpt_t* %gpt, i32 0, i32 9, i32 0 ; <i8*> [#uses=1]
  call void @uuid_LtoB(i8* %0, i8* %1) nounwind, !dbg !0
  %a9 = load volatile i64* %data_addr.i18, align 8 ; <i64> [#uses=1]
  %a10 = call i64 @llvm.bswap.i64(i64 %a9) nounwind ; <i64> [#uses=1]
  %a11 = getelementptr inbounds %struct.gpt_t* %gpt, i32 0, i32 8, !dbg !7 ; <i64*> [#uses=1]
  %a12 = load i64* %a11, align 4, !dbg !7         ; <i64> [#uses=1]
  call void @llvm.dbg.declare(metadata !{i64* %data_addr.i17}, metadata !8) nounwind, !dbg !14
  store i64 %a12, i64* %data_addr.i17, align 8
  call void @llvm.dbg.value(metadata !6, i64 0, metadata !15) nounwind
  call void @llvm.dbg.value(metadata !18, i64 0, metadata !19) nounwind
  call void @llvm.dbg.declare(metadata !6, metadata !23) nounwind
  call void @llvm.dbg.value(metadata !{i64* %data_addr.i17}, i64 0, metadata !34) nounwind
  %a13 = load volatile i64* %data_addr.i17, align 8 ; <i64> [#uses=1]
  %a14 = call i64 @llvm.bswap.i64(i64 %a13) nounwind ; <i64> [#uses=2]
  %a15 = add i64 %a10, %a14, !dbg !7              ; <i64> [#uses=1]
  %a16 = sub i64 %a15, %a14                       ; <i64> [#uses=1]
  %a17 = getelementptr inbounds %struct.gpm_t* %gpm, i32 0, i32 5, !dbg !7 ; <i64*> [#uses=1]
  store i64 %a16, i64* %a17, align 4, !dbg !7
  ret void, !dbg !7
}

; FileCheck gives an error if its input file is empty, so ensure that
; the output of pnacl-abicheck is non-empty by generating at least one
; error.
declare void @bad_func(ppc_fp128 %bad_arg)
; DBG: Function bad_func has disallowed type: void (ppc_fp128)
