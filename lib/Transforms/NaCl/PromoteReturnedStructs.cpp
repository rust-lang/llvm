//===- PromoteReturnedStructs.cpp - Promote returned structures to sret args==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// ** THIS PASS DESTROYS ABI RULES **
// Getting this right is tricky; we can't rely on internal linkage specifications to avoid 
// changing the ABI, ie Pepper PPB interfaces. Currently, we special case the return
// type {}*, replacing it with i8*.
//
// TODO(diamond): some PPB interface functions return structures; this pass breaks
// that particular ABI detail.
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
#include <map>
#include <iostream>

using namespace llvm;

class PromoteReturnedStructs : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  PromoteReturnedStructs();

  typedef std::map<Type*, Type*>::iterator ty_iterator;
  std::map<Type*, Type*> m_types;

  typedef std::set<GlobalValue*>::iterator gv_iterator;
  std::set<GlobalValue*> m_globals;

  typedef std::map<Constant*, Constant*>::iterator const_iterator;
  std::map<Constant*, Constant*> m_consts;

  // we can't move the return type of functions like malloc PPP_GetInterface etc.
  bool isProtected(Function* F) {
    return !F->getName().startswith("_ZN");
  }

  // Rust uses {}* as void pointers.
  bool isVoidPtrTy(Type* Ty) {
    return Ty->isPointerTy() &&
      Ty->getContainedType(0)->isStructTy() &&
      (cast<StructType>(Ty->getContainedType(0))->isOpaque() ||
       cast<StructType>(Ty->getContainedType(0))->getNumElements() == 0);
  }

  void promoteFunction(Function* F);
  void promoteGlobalVariable(GlobalVariable* G);
  
  Type* promoteType(Type* Ty, const bool InRetPos = false);
  Constant* promoteConstant(Constant* C);
  void promoteOperands(User* U);
  Value* promoteOperand(Value* Op);

  void promoteGlobal(GlobalValue* V);

  bool shouldPromote(Type* Ty);

  template <class T>
  void promoteCallInst(T* Inst);
  template <class T>
  void promoteCallArgs(T* Inst);

  bool runOnModule(Module& M);
};

char PromoteReturnedStructs::ID = 0;
INITIALIZE_PASS(PromoteReturnedStructs, "promote-returned-structures",
                "Promote returned structures to sret arguments",
                false, false)

// I'mma leave this here; it should get optimized away anyhow.
static size_t ActualPromotedFunctions1;

PromoteReturnedStructs::PromoteReturnedStructs()
: ModulePass(ID) {
  initializePromoteReturnedStructsPass(*PassRegistry::getPassRegistry());
}

bool PromoteReturnedStructs::shouldPromote(Type* Ty) {
  return Ty != NULL && !Ty->isVoidTy() &&
    !(isVoidPtrTy(Ty)) &&
    (Ty->isAggregateType() ||
     (isa<PointerType>(Ty) && shouldPromote(Ty->getContainedType(0))));
}
Type* PromoteReturnedStructs::promoteType(Type* Ty, const bool InRetPos) {
  if(Ty == NULL)
    return NULL;

  ty_iterator i = m_types.find(Ty);
  if(i != m_types.end()) {
    assert(i->second != NULL && "promoteType");
    return i->second;
  }
  Type* NewTy = NULL;

  if(isa<PointerType>(Ty)) {
    if(InRetPos && isVoidPtrTy(Ty)) {
      LLVMContext& C = Ty->getContext();
      NewTy = Type::getInt8Ty(C)->getPointerTo();
    } else {
      Type* InnerTy = Ty->getContainedType(0);
      Type* NewInnerTy = promoteType(InnerTy);
      NewTy = PointerType::get(NewInnerTy, 0);
    }
  } else if(isa<StructType>(Ty) && Ty->getStructNumElements() != 0) {
    StructType* STy = cast<StructType>(Ty);
    
    StructType* NewSTy;
    if(STy->hasName())
      NewSTy = StructType::create(Ty->getContext(), STy->getName());
    else
      NewSTy = StructType::create(Ty->getContext());

    NewTy = NewSTy;
    m_types[Ty] = NewTy;
    m_types[NewTy] = NewTy;

    std::vector<Type*> Types;
    Types.reserve(STy->getNumElements());

    for(unsigned j = 0; j < STy->getNumElements(); ++j) {
      Type* OldTy2 = STy->getElementType(j);
      Type* NewTy2 = promoteType(OldTy2);
      Types.push_back(NewTy2);
    }
    NewSTy->setBody(Types, STy->isPacked());
    return NewTy;
  } else if(isa<FunctionType>(Ty)) {
    FunctionType* FTy = cast<FunctionType>(Ty);
    Type* RetTy = FTy->getReturnType();

    const bool PromoteRet = shouldPromote(RetTy);

    Type* NewRetTy = promoteType(RetTy, !PromoteRet);

    std::vector<Type*> Args;
    Args.reserve(FTy->getNumParams() + 1);

    if(PromoteRet)
      Args.push_back(PointerType::get(NewRetTy, 0));

    for(unsigned j = 0; j < FTy->getNumParams(); ++j) {
      Type* OldTy2 = FTy->getParamType(j);
      Type* NewTy2 = promoteType(OldTy2);
      Args.push_back(NewTy2);
    }
    
    if(PromoteRet)
      NewTy = FunctionType::get(Type::getVoidTy(Ty->getContext()), Args, FTy->isVarArg());
    else
      NewTy = FunctionType::get(NewRetTy, Args, FTy->isVarArg());
  } else if(isa<ArrayType>(Ty)) {
    ArrayType* ATy = cast<ArrayType>(Ty);
    Type* ElementTy = ATy->getElementType();
    Type* NewElemTy = promoteType(ElementTy);
    NewTy = ArrayType::get(NewElemTy, ATy->getNumElements());
  } else {
    NewTy = Ty;
  }
  
  assert(NewTy != NULL);
  m_types.insert(std::make_pair(Ty, NewTy));
  if(Ty != NewTy)
    m_types.insert(std::make_pair(NewTy, NewTy));
  return NewTy;
}
void PromoteReturnedStructs::promoteFunction(Function* F) {
  if(F->isIntrinsic())
    return;

  Type* RetTy = F->getReturnType();  
  const bool IsProtected = isProtected(F);
  const bool ShouldPromote = !IsProtected && shouldPromote(RetTy);

  Argument* RetArg = NULL;
  if(ShouldPromote) {
    RetTy = promoteType(RetTy);
    RetTy = PointerType::get(RetTy, 0);
    
    // first create the new argument
    RetArg = new Argument(RetTy, "");
    F->getArgumentList().insert(F->arg_begin(), RetArg);

    // then shift the existing attrs:
    AttributeSet Attrs = F->getAttributes();
    AttributeSet NewAttrs;
    for(unsigned i = 0; i < Attrs.getNumSlots(); ++i) {
      unsigned Index = Attrs.getSlotIndex(i);
      AttrBuilder B(Attrs, Index);
      if(Index != AttributeSet::FunctionIndex && Index != AttributeSet::ReturnIndex) {
        Index += 1;
      } else if(Index == AttributeSet::ReturnIndex) {
        AttributeSet RetSet = AttributeFuncs::typeIncompatible(RetTy,
                                                               1);
        B.removeAttributes(RetSet, 1);
        NewAttrs = NewAttrs.addAttributes(F->getContext(),
                                          1,
                                          AttributeSet::get(F->getContext(), 1, B));

        // now the new return attrs:
        B = AttrBuilder(Attrs, 0);
        RetSet = AttributeFuncs::typeIncompatible(Type::getVoidTy(F->getContext()),
                                                  0);
        B.removeAttributes(RetSet, 0);
        NewAttrs = NewAttrs.addAttributes(F->getContext(),
                                          0,
                                          AttributeSet::get(F->getContext(), 0, B));
        continue;
      } else { /* function index */ }
      NewAttrs = NewAttrs.addAttributes(F->getContext(),
                                        Index,
                                        AttributeSet::get(F->getContext(), Index, B));
    }
    Type* OldTy = F->Value::getType();
    Type* NewTy = promoteType(OldTy);
    F->mutateType(NewTy);

    F->setAttributes(NewAttrs);
    F->addAttribute(1, Attribute::StructRet);
  } else {
    std::vector<Type*> Args;
    Args.reserve(F->getFunctionType()->getNumParams());

    for(unsigned j = 0; j < F->getFunctionType()->getNumParams(); ++j) {
      Type* OldTy2 = F->getFunctionType()->getParamType(j);
      Type* NewTy2 = promoteType(OldTy2);
      Args.push_back(NewTy2);
    }

    FunctionType* FTy = FunctionType::get(promoteType(F->getReturnType(), true),
                                          Args, F->isVarArg());
    F->mutateType(FTy->getPointerTo());
  }

  const Function::arg_iterator arg_end = F->arg_end();
  for(Function::arg_iterator i = F->arg_begin(); i != arg_end; ++i) {
    Type* OldTy = i->getType();
    Type* NewTy = promoteType(OldTy);
    i->mutateType(NewTy);
  }

  const Function::iterator end = F->end();
  for(Function::iterator i = F->begin(); i != end; ++i) {
    BasicBlock::iterator end = i->end();
    for(BasicBlock::iterator j = i->begin(); j != end;) {
      if(isa<CallInst>(*j)) {
        CallInst* Call = cast<CallInst>(j++);
        promoteCallInst<CallInst>(Call);
      } else if(isa<InvokeInst>(*j)) {
        InvokeInst* Invoke = cast<InvokeInst>(j++);
        promoteCallInst<InvokeInst>(Invoke);
      } else {
        promoteOperands(j);
        Type* Ty = j->getType();
        Type* NewTy = promoteType(Ty);
        j->mutateType(NewTy);
        ++j;
      }
    }
    
    TerminatorInst* Terminator = i->getTerminator();
    if(isa<ReturnInst>(Terminator)) {
      LLVMContext& C = F->getContext();
      ReturnInst* Ret = cast<ReturnInst>(Terminator);
      Value* RetVal = Ret->getReturnValue();
      if(ShouldPromote) {
        StoreInst* Store = CopyDebug(new StoreInst(RetVal, RetArg, Ret), Terminator);
        Store->setAlignment(F->getParamAlignment(1));
        CopyDebug(ReturnInst::Create(C, Ret->getParent()), Terminator);
        Ret->dropAllReferences();
        Ret->eraseFromParent();
      } else if(isVoidPtrTy(Terminator->getType())) {
        Type* NewTy = promoteType(Terminator->getType(), true);
        BitCastInst* BitCast = CopyDebug(new BitCastInst(RetVal, NewTy, "", Ret), Ret);
        CopyDebug(ReturnInst::Create(C, BitCast, Ret->getParent()), Ret);
        Ret->dropAllReferences();
        Ret->eraseFromParent();
      }
    }
  }
}
void PromoteReturnedStructs::promoteGlobalVariable(GlobalVariable* G) {
  Type* OriginalTy = G->getType();
  Type* PromotedTy = promoteType(OriginalTy);
  G->mutateType(PromotedTy);
  if(G->hasInitializer()) {
    Constant* OldC = G->getInitializer();
    Constant* NewC = promoteConstant(OldC);
    G->setInitializer(NewC);
  }
}
Constant* PromoteReturnedStructs::promoteConstant(Constant* C) {
  if(isa<GlobalValue>(C)) {
    GlobalValue* V = cast<GlobalValue>(C);
    promoteGlobal(V);
    return C;
  }

  std::pair<const_iterator, bool> i
    = m_consts.insert(std::make_pair(C, (Constant*)NULL));
  // If i.first->second is NULL, we've encountered a recursion.
  // See the comment in the first branch.
  if(!i.second && i.first->second != NULL) {
    return i.first->second;
  }

  Constant*& NewC = i.first->second;
  if(isa<ConstantExpr>(C) ||
     isa<ConstantStruct>(C) ||
     isa<ConstantArray>(C)) {
    std::vector<Constant*> Consts;
    Consts.reserve(C->getNumOperands());
    const User::value_op_iterator end = C->value_op_end();
    for(User::value_op_iterator i = C->value_op_begin(); i != end; ++i) {
      Constant* OldC2 = cast<Constant>(*i);
      Constant* NewC2 = promoteConstant(OldC2);

      // the promotion of one of the operands caused us to circle back around to this const.
      // the only way this can happen is through a global, which means the second time around
      // would skip the global causing the recursion, allowing the promotion to finish.
      // if all that happens, our reference into the map will reflect the promotion,
      // NewC != NULL, and we can just return.
      if(NewC != NULL)
        return NewC;

      Consts.push_back(NewC2);
    }
    
    Type* Ty = C->getType();
    Type* NewTy = promoteType(Ty);
    if(ConstantExpr* CE = dyn_cast<ConstantExpr>(C)) {
      NewC = CE->getWithOperands(Consts, NewTy);
    } else if(isa<ConstantStruct>(C)) {
      StructType* STy = cast<StructType>(NewTy);
      NewC = ConstantStruct::get(STy, Consts);
    } else if(isa<ConstantArray>(C)) {
      ArrayType* ATy = cast<ArrayType>(NewTy);
      NewC = ConstantArray::get(ATy, Consts);
    }
  } else if(isa<UndefValue>(C)) {
    NewC = UndefValue::get(promoteType(C->getType()));
  } else if(isa<ConstantPointerNull>(C)) {
    Type* OldTy = C->getType();
    PointerType* NewTy = cast<PointerType>(promoteType(OldTy));
    NewC = ConstantPointerNull::get(NewTy);
  } else if(isa<ConstantAggregateZero>(C)) {
    NewC = ConstantAggregateZero::get(promoteType(C->getType()));
  } else if(isa<ConstantDataArray>(C)) {
    NewC = C;
  } else {
    assert(!shouldPromote(C->getType()));
    NewC = C;
  }

  assert(NewC != NULL);
  if(C != NewC)
    m_consts.insert(std::make_pair(NewC, NewC)).second;
  return NewC;
}
void PromoteReturnedStructs::promoteOperands(User* U) {
  unsigned pos = 0;
  const User::value_op_iterator end = U->value_op_end();
  for(User::value_op_iterator k = U->value_op_begin(); k != end; ++k, ++pos) {
    Value* V = *k;
    U->setOperand(pos, promoteOperand(V));
  }
}
Value* PromoteReturnedStructs::promoteOperand(Value* V) {
  if(isa<Constant>(V)) {
    Constant* C = cast<Constant>(V);
    Constant* NewC = promoteConstant(C);
    return NewC;
  } else
    return V;
}
void PromoteReturnedStructs::promoteGlobal(GlobalValue* V) {
  if(m_globals.insert(V).second) {
    if(isa<Function>(V)) {
      Function* F = cast<Function>(V);
      promoteFunction(F);
      ++ActualPromotedFunctions1;
    } else if(isa<GlobalVariable>(V)) {
      GlobalVariable* G = cast<GlobalVariable>(V);
      promoteGlobalVariable(G);
    }
  }
}

template <class T>
void PromoteReturnedStructs::promoteCallInst(T* Inst) {
  Value* Called = Inst->getCalledValue();
  if(isa<Function>(Called)) {
    Function* F = cast<Function>(Called);
    if(F->isIntrinsic())
      return;
    else if(isProtected(F) || isVoidPtrTy(Inst->getType())) {
      promoteCallArgs(Inst);
      return;
    }
  }

  if(isa<GlobalValue>(Called)) {
    GlobalValue* G = cast<GlobalValue>(Called);
    promoteGlobal(G);
  }
  FunctionType* FTy = cast<FunctionType>(Called->getType()->getContainedType(0));
  Type* InstTy = Inst->getType();
  if(shouldPromote(Inst->getType())) {
    Type* AllocaTy = promoteType(InstTy);
    AllocaInst* Alloca = CopyDebug(new AllocaInst(AllocaTy, NULL, "", Inst), Inst);
    std::vector<Value*> Args;
    Args.reserve(FTy->getNumParams() + 1);
    Args.push_back(Alloca);
    for(unsigned i = 0; i < Inst->getNumArgOperands(); ++i) {
      Value* V = Inst->getArgOperand(i);
      if(isa<Constant>(V)) {
        Constant* C = cast<Constant>(V);
        V = promoteConstant(C);
      }
      Args.push_back(V);
    }
    Value* BaseCall = NULL;
    // Note we don't set the calling convention;
    // PNaCl just overrides it anyway.
    if(isa<CallInst>(Inst)) {
      CallInst* OldCall = cast<CallInst>(Inst);
      CallInst* Call =  CopyDebug(CallInst::Create(Called, Args, "", Inst), Inst);
      if(OldCall->canReturnTwice())
        Call->setCanReturnTwice();
      if(OldCall->cannotDuplicate())
        Call->setCannotDuplicate();
      BaseCall = Call;
    }
    else if(isa<InvokeInst>(Inst)) {
      // should optimize to a no-op.
      InvokeInst* Inst = cast<InvokeInst>(Inst);
      BaseCall = CopyDebug(InvokeInst::Create(Called,
                                              Inst->getNormalDest(),
                                              Inst->getUnwindDest(),
                                              Args,
                                              "",
                                              Inst),
                           Inst);
    }
    T* Call = cast<T>(BaseCall);
    if(Inst->doesNotThrow())
      Call->setDoesNotThrow();
    if(Inst->isNoInline())
      Call->setIsNoInline();
    if(Inst->doesNotAccessMemory())
      Call->setDoesNotAccessMemory();
    if(Inst->doesNotReturn())
      Call->setDoesNotReturn();
    if(Inst->onlyReadsMemory())
      Call->setOnlyReadsMemory();

    LoadInst* Ret;
    if(Call->getNextNode() != NULL)
      Ret = new LoadInst(Alloca,
                         "",
                         Call->getNextNode());
    else
      Ret = new LoadInst(Alloca,
                         "",
                         Call->getParent());
    
    CopyDebug(Ret, Call);

    if(isa<Function>(Called))
      Ret->setAlignment(cast<Function>(Called)->getParamAlignment(1));
    Inst->mutateType(AllocaTy);
    Inst->replaceAllUsesWith(Ret);
    Inst->dropAllReferences();
    Inst->eraseFromParent();
  } else {
    promoteCallArgs(Inst);
  }
}
template <class T>
void PromoteReturnedStructs::promoteCallArgs(T* Inst) {
  unsigned end = Inst->getNumArgOperands();
  for(unsigned k = 0; k < end; ++k) {
    Value* V = Inst->getArgOperand(k);
    Value* NewV = promoteOperand(V);
    Inst->setArgOperand(k, NewV);
  }
  Type* NewTy = promoteType(Inst->getType(), true);
  Inst->mutateType(NewTy);
}
bool PromoteReturnedStructs::runOnModule(Module& M) {
  // I'mma leave this here; it should get optimized away anyhow.
  size_t Promoted = 0;
  {
    const Module::iterator end = M.end();
    for(Module::iterator i = M.begin(); i != end; ++i) {
      promoteGlobal(i);
      Promoted++;
    }
  }
  {
    const Module::global_iterator end = M.global_end();
    for(Module::global_iterator i = M.global_begin(); i != end; ++i) {
      promoteGlobal(i);
    }
  }
  // remove dangling consts:
  {
    const const_iterator end = m_consts.end();
    for(const_iterator i = m_consts.begin(); i != end; ++i) {
      (*i).second->removeDeadConstantUsers();
    }
  }
  {
    const gv_iterator end = m_globals.end();
    for(gv_iterator i = m_globals.begin(); i != end; ++i) {
      (*i)->removeDeadConstantUsers();
    }
  }

  m_globals.clear();
  m_types.clear();
  m_consts.clear();
  return true;
}

ModulePass *llvm::createPromoteReturnedStructsPass() {
  return new PromoteReturnedStructs();
}
