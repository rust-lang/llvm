//===---- IRReader.cpp - Reader for LLVM IR files -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/IRReader/IRReader.h"
#include "llvm-c/Core.h"
#include "llvm-c/IRReader.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"

using namespace llvm;

namespace llvm {
  extern bool TimePassesIsEnabled;
}

static const char *const TimeIRParsingGroupName = "LLVM IR Parsing";
static const char *const TimeIRParsingName = "Parse IR";


Module *llvm::getLazyIRModule(MemoryBuffer *Buffer, SMDiagnostic &Err,
                              LLVMContext &Context, FileFormat Format) {
  if (Format == LLVMFormat) {
    if (isBitcode((const unsigned char *)Buffer->getBufferStart(),
                  (const unsigned char *)Buffer->getBufferEnd())) {
      std::string ErrMsg;
      ErrorOr<Module *> ModuleOrErr = getLazyBitcodeModule(Buffer, Context);
      if (error_code EC = ModuleOrErr.getError()) {
        Err = SMDiagnostic(Buffer->getBufferIdentifier(), SourceMgr::DK_Error,
                           EC.message());
        // ParseBitcodeFile does not take ownership of the Buffer in the
        // case of an error.
        delete Buffer;
        return NULL;
      }
      return ModuleOrErr.get();
    }

    return ParseAssembly(Buffer, 0, Err, Context);
  } else if ((Format == PNaClFormat) &&
      isNaClBitcode((const unsigned char *)Buffer->getBufferStart(),
                    (const unsigned char *)Buffer->getBufferEnd())) {
    std::string ErrMsg;
    Module *M = getNaClLazyBitcodeModule(Buffer, Context, &ErrMsg);
    if (M == 0)
      Err = SMDiagnostic(Buffer->getBufferIdentifier(), SourceMgr::DK_Error,
                         ErrMsg);
    // NaClParseBitcodeFile does not take ownership of the Buffer.
    delete Buffer;
    return M;
  } else {
    Err = SMDiagnostic(Buffer->getBufferIdentifier(), SourceMgr::DK_Error,
                       "Did not specify correct format for file");
    return 0;
  }
}

Module *llvm::getLazyIRFileModule(const std::string &Filename, SMDiagnostic &Err,
                                  LLVMContext &Context, FileFormat Format) {
  OwningPtr<MemoryBuffer> File;
  if (error_code ec = MemoryBuffer::getFileOrSTDIN(Filename, File)) {
    Err = SMDiagnostic(Filename, SourceMgr::DK_Error,
                       "Could not open input file: " + ec.message());
    return 0;
  }

  return getLazyIRModule(File.take(), Err, Context, Format);
}

Module *llvm::ParseIR(MemoryBuffer *Buffer, SMDiagnostic &Err,
                      LLVMContext &Context, FileFormat Format) {
  NamedRegionTimer T(TimeIRParsingName, TimeIRParsingGroupName,
                     TimePassesIsEnabled);
  if (Format == LLVMFormat) {
    if (isBitcode((const unsigned char *)Buffer->getBufferStart(),
                  (const unsigned char *)Buffer->getBufferEnd())) {
      ErrorOr<Module *> ModuleOrErr = parseBitcodeFile(Buffer, Context);
      Module *M = 0;
      if (error_code EC = ModuleOrErr.getError())
        Err = SMDiagnostic(Buffer->getBufferIdentifier(), SourceMgr::DK_Error,
                           EC.message());
      else
        M = ModuleOrErr.get();
      // parseBitcodeFile does not take ownership of the Buffer.
      delete Buffer;
      return M;
    }

    return ParseAssembly(Buffer, 0, Err, Context);
  } else if ((Format == PNaClFormat) &&
      isNaClBitcode((const unsigned char *)Buffer->getBufferStart(),
                    (const unsigned char *)Buffer->getBufferEnd())) {
    std::string ErrMsg;
    Module *M = NaClParseBitcodeFile(Buffer, Context, &ErrMsg);
    if (M == 0)
      Err = SMDiagnostic(Buffer->getBufferIdentifier(), SourceMgr::DK_Error,
                         ErrMsg);
    // NaClParseBitcodeFile does not take ownership of the Buffer.
    delete Buffer;
    return M;
  } else {
    Err = SMDiagnostic(Buffer->getBufferIdentifier(), SourceMgr::DK_Error,
                       "Did not specify correct format for file");
    return 0;
  }
}

Module *llvm::ParseIRFile(const std::string &Filename, SMDiagnostic &Err,
                          LLVMContext &Context, FileFormat Format) {
  OwningPtr<MemoryBuffer> File;
  if (error_code ec = MemoryBuffer::getFileOrSTDIN(Filename, File)) {
    Err = SMDiagnostic(Filename, SourceMgr::DK_Error,
                       "Could not open input file: " + ec.message());
    return 0;
  }

  return ParseIR(File.take(), Err, Context, Format);
}

//===----------------------------------------------------------------------===//
// C API.
//===----------------------------------------------------------------------===//

LLVMBool LLVMParseIRInContext(LLVMContextRef ContextRef,
                              LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutM,
                              char **OutMessage) {
  SMDiagnostic Diag;

  *OutM = wrap(ParseIR(unwrap(MemBuf), Diag, *unwrap(ContextRef)));

  if(!*OutM) {
    if (OutMessage) {
      std::string buf;
      raw_string_ostream os(buf);

      Diag.print(NULL, os, false);
      os.flush();

      *OutMessage = strdup(buf.c_str());
    }
    return 1;
  }

  return 0;
}
