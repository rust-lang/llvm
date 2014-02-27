//===- ReplaceArraysWithInts.cpp - Replace remaining aggregate types with ints//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Replaces remaining BitCastInst|IntToPtrInst -> LoadInst
//   -> (ValOperand) StoreInst combos.
// This pass /should/ be safe because previous passes have reduced element
// access to pointer offsets, so all that remains is the movement of
// whole aggregate values.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Attributes.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Analysis/NaCl.h"
#include <set>


using namespace llvm;

template <class T> 
const std::string ToStr(const T &V) {
  std::string S;
  raw_string_ostream OS(S);
  OS << const_cast<T &>(V);
  return OS.str();
}

class ReplaceAggregatesWithInts :
  public FunctionPass {
public:
  static char ID;
  ReplaceAggregatesWithInts() : FunctionPass(ID), m_converted(0) {
    initializeReplaceAggregatesWithIntsPass(*PassRegistry::getPassRegistry());
  }

  virtual void getAnalysisUsage(AnalysisUsage &Info) const {
    Info.addRequired<DataLayout>();
  }

  size_t m_converted;

  Type* getReplacedTy(Type* Ty) {
    LLVMContext& C = Ty->getContext();
    unsigned Width;
    if(isa<ArrayType>(Ty)) {
      ArrayType* ATy = cast<ArrayType>(Ty);
      Type* ElemTy = ATy->getElementType();
      if(!ElemTy->isIntegerTy()) {
        errs() << "Type: " << ToStr(*ATy) << "\n";
        assert(0 && "Unsupported replacement!");
        report_fatal_error("Unsupported replacement!");
      }

      if(ATy->getNumElements() == 0)
        return NULL;
      else if(ATy->getNumElements() == 1)
        return ElemTy;
      else {
        unsigned Bits = ElemTy->getIntegerBitWidth();
        Width = Bits * ATy->getNumElements();
      }
    } else if(isa<StructType>(Ty)) {
      const DataLayout DL(getAnalysis<DataLayout>());
      Width = DL.getTypeSizeInBits(Ty);
    } else {
      errs() << "Type: " << ToStr(*Ty) << "\n";
      assert(0 && "This shouldn't be reached!");
      report_fatal_error("This shouldn't be reached!");
    }

    if(Width > 64) {
      errs() << "Type: " << ToStr(*Ty) << "\n";
      assert(0 && "Replacement would be too wide!");
      report_fatal_error("Replacement would be too wide!");
    } else if(Width == 0)
      return NULL;

    return Type::getIntNTy(C, Width);
  }

  virtual bool runOnFunction(Function& F) {
    std::set<Instruction*> ToErase;

    bool Changed = false;

    const Function::iterator End = F.end();
    for(Function::iterator I = F.begin(); I != End; ++I) {
      BasicBlock* BB = I;
      const BasicBlock::iterator End = BB->end();
      for(BasicBlock::iterator J = BB->begin(); J != End; ++J) {
        Instruction* Inst = J;

        if(isa<IntToPtrInst>(Inst) || isa<BitCastInst>(Inst)) {
          CastInst* Cast = cast<CastInst>(Inst);
          Type* Ty = Cast->getType();
          if(!isa<PointerType>(Ty))
            continue;

          Type* ContainedTy = Ty->getContainedType(0);
          if(!(isa<ArrayType>(ContainedTy) || isa<StructType>(ContainedTy)))
            continue;

          Type* NewTy = getReplacedTy(ContainedTy);
          if(NewTy == NULL) {
            const Value::use_iterator End = Cast->use_end();
            for(Value::use_iterator K = Cast->use_begin(); K != End;) {
              if(!(isa<StoreInst>(*K) || isa<LoadInst>(*K))) {
                errs() << "Inst: " << ToStr(*Cast) << "\n";
                errs() << "Use : " << ToStr(**K) << "\n";
                assert(0 && "Unknown use!");
                report_fatal_error("Unknown use!");
              }

              Instruction* KInst = cast<Instruction>(*K++);
              if(isa<LoadInst>(KInst)) {
                const Value::use_iterator End = KInst->use_end();
                for(Value::use_iterator L = KInst->use_begin(); L != End; ++L) {
                  if(!isa<StoreInst>(*L)) {
                    errs() << "Inst: " << ToStr(*KInst) << "\n";
                    errs() << "Use: " << ToStr(**L) << "\n";
                    assert(0 && "Non-StoreInst use!");
                    report_fatal_error("Non-StoreInst use!");
                  }

                  ToErase.insert(cast<Instruction>(*L));
                }
              }
              ToErase.insert(KInst);
            }
            ToErase.insert(Cast);
            Changed = true;
          } else {
            // mutate load types.
            const Value::use_iterator End = Cast->use_end();
            for(Value::use_iterator K = Cast->use_begin(); K != End; ++K) {
              if(isa<LoadInst>(*K)) {
                assert(K->getType() == ContainedTy);
                K->mutateType(NewTy);
                Changed = true;
              }
            }
            Cast->mutateType(PointerType::get(NewTy, 0));
          }

        } // BinOp
      } // BasicBlock::iterator
    } // Function::iterator

    const std::set<Instruction*>::iterator End2 = ToErase.end();
    for(std::set<Instruction*>::iterator I = ToErase.begin(); I != End2; ++I) {
      (*I)->dropAllReferences();
    }
    for(std::set<Instruction*>::iterator I = ToErase.begin(); I != End2; ++I) {
      (*I)->eraseFromParent();
    }

    ++m_converted;
    return Changed;
  }
};

char ReplaceAggregatesWithInts::ID = 0;
INITIALIZE_PASS(ReplaceAggregatesWithInts,
                "replace-aggregates-with-ints",
                "Replace remaining aggregates with a single integer", 
                false,
                false)

FunctionPass *llvm::createReplaceAggregatesWithIntsPass() {
  return new ReplaceAggregatesWithInts();
}
