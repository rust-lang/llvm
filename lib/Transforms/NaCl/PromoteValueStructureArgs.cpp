//===- PromoteStructureArguments.cpp - Promote structure values to byval --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
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

using namespace llvm;

class PromoteStructureArgs :
  public ModulePass {
public:
  static char ID;
  PromoteStructureArgs() 
    : ModulePass(ID) {
    initializePromoteStructureArgsPass(*PassRegistry::getPassRegistry());
  }
  typedef std::map<Type*, Type*>::iterator ty_iterator;
  std::map<Type*, Type*> m_types;

  typedef std::set<GlobalValue*>::iterator gv_iterator;
  std::set<GlobalValue*> m_globals;

  typedef std::map<Constant*, Constant*>::iterator const_iterator;
  std::map<Constant*, Constant*> m_consts;

  void promoteFunction(Function* F);
  void promoteGlobalVariable(GlobalVariable* G);
  
  Type* promoteType(Type* Ty);
  Constant* promoteConstant(Constant* C);
  void promoteOperands(User* U);
  Value* promoteOperand(Value* V);

  void promoteGlobal(GlobalValue* V);

  bool shouldPromoteParam(Type* Ty);

  template <class T>
  void promoteCallInst(T* Inst, Function* ParentF);

  bool runOnModule(Module& M);
};

char PromoteStructureArgs::ID = 0;
INITIALIZE_PASS(PromoteStructureArgs, "promote-structure-arguments",
                "Promote by value structure arguments to use byval",
                false, false)

size_t ActualPromotedFunctions2;

ModulePass* llvm::createPromoteStructureArgsPass() {
  return new PromoteStructureArgs();
}

bool PromoteStructureArgs::shouldPromoteParam(Type* Ty) {
  return isa<StructType>(Ty) || isa<ArrayType>(Ty);
}

Type* PromoteStructureArgs::promoteType(Type* Ty) {
  if(Ty == NULL)
    return NULL;

  ty_iterator i = m_types.find(Ty);
  if(i != m_types.end()) {
    assert(i->second != NULL && "promoteType");
    return i->second;
  }
  Type* NewTy = NULL;

  if(isa<PointerType>(Ty)) {
    Type* InnerTy = Ty->getContainedType(0);
    Type* NewInnerTy = promoteType(InnerTy);
    NewTy = PointerType::get(NewInnerTy, 0);
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
    Type* NewRetTy = promoteType(RetTy);

    std::vector<Type*> Args;
    Args.reserve(FTy->getNumParams());

    for(unsigned j = 0; j < FTy->getNumParams(); ++j) {
      Type* OldTy2 = FTy->getParamType(j);
      Type* NewTy2 = promoteType(OldTy2);
      if(shouldPromoteParam(NewTy2))
        Args.push_back(PointerType::get(NewTy2, 0));
      else
        Args.push_back(NewTy2);
    }

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
void PromoteStructureArgs::promoteFunction(Function* F) {
  if(F->isIntrinsic())
    return;
  FunctionType* FTy = F->getFunctionType();
  FunctionType* NewFTy = cast<FunctionType>(promoteType(FTy));
  Function::arg_iterator arg_i = F->arg_begin();
  for(unsigned i = 0; i < FTy->getNumParams(); ++i, ++arg_i) {
    Type* Ty = FTy->getParamType(i);
    Type* NewTy = NewFTy->getParamType(i);

    Argument* Arg = arg_i;
    Arg->mutateType(NewTy);

    if(!shouldPromoteParam(Ty))
      continue;

    F->addAttribute(i + 1, Attribute::ByVal);
    if(F->size() == 0)
      continue;

    LoadInst* Load = new LoadInst(Arg, "", F->getEntryBlock().getFirstNonPHI());

    // sigh.

    // for the asserts in Value::replaceAllUsesWith()
    Arg->mutateType(Load->getType()); 
    Arg->replaceAllUsesWith(Load);
    Arg->mutateType(NewTy);
    // now reset Load's pointer operand to Arg
    Load->setOperand(0, Arg);
  }

  Type* OldTy = F->Value::getType();
  Type* NewTy = promoteType(OldTy);
  F->mutateType(NewTy);

  const Function::iterator end = F->end();
  for(Function::iterator i = F->begin(); i != end; ++i) {
    BasicBlock::iterator end = i->end();
    for(BasicBlock::iterator j = i->begin(); j != end;) {
      Type* Ty = j->getType();
      if(isa<CallInst>(*j)) {
        CallInst* Call = cast<CallInst>(j++);
        promoteCallInst<CallInst>(Call, F);
      } else if(isa<InvokeInst>(*j)) {
        InvokeInst* Invoke = cast<InvokeInst>(j++);
        promoteCallInst<InvokeInst>(Invoke, F);
      } else {
        promoteOperands(j);
        Type* NewTy = promoteType(Ty);
        j->mutateType(NewTy);
        ++j;
      }
    }
  }
}
void PromoteStructureArgs::promoteGlobalVariable(GlobalVariable* G) {
  Type* OriginalTy = G->getType();
  Type* PromotedTy = promoteType(OriginalTy);
  G->mutateType(PromotedTy);
  if(G->hasInitializer()) {
    Constant* OldC = G->getInitializer();
    Constant* NewC = promoteConstant(OldC);
    G->setInitializer(NewC);
  }
}
Constant* PromoteStructureArgs::promoteConstant(Constant* C) {
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
      // NewC won't be NULL, and we can just return.
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
  } else {
    NewC = C;
  }

  assert(NewC != NULL);
  if(C != NewC) {
    const bool NewCInserted = m_consts.insert(std::make_pair(NewC, NewC)).second;
    assert(NewCInserted);
    (void)NewCInserted;
  }
  return NewC;
}
void PromoteStructureArgs::promoteOperands(User* U) {
  unsigned pos = 0;
  const User::value_op_iterator end = U->value_op_end();
  for(User::value_op_iterator k = U->value_op_begin(); k != end; ++k, ++pos) {
    Value* NewV = promoteOperand(*k);
    U->setOperand(pos, NewV);
  }
}
Value* PromoteStructureArgs::promoteOperand(Value* V) {
  if(isa<Constant>(V)) {
    Constant* C = cast<Constant>(V);
    Constant* NewC = promoteConstant(C);
    return NewC;
  } else
    return V;
}
void PromoteStructureArgs::promoteGlobal(GlobalValue* V) {
  if(m_globals.insert(V).second) {
    if(isa<Function>(V)) {
      Function* F = cast<Function>(V);
      promoteFunction(F);
      ++ActualPromotedFunctions2;
    } else if(isa<GlobalVariable>(V)) {
      GlobalVariable* G = cast<GlobalVariable>(V);
      promoteGlobalVariable(G);
    }
  }
}

template <class T>
void PromoteStructureArgs::promoteCallInst(T* Inst, Function* ParentF) {
  Value* Called = Inst->getCalledValue();
  if(isa<Function>(Called) && cast<Function>(Called)->isIntrinsic())
    return;

  if(isa<GlobalValue>(Called)) {
    GlobalValue* G = cast<GlobalValue>(Called);
    promoteGlobal(G);
  }

  const unsigned end = Inst->getNumArgOperands();
  for(unsigned i = 0; i != end; ++i) {
    Value* V = Inst->getArgOperand(i);
    Value* NewOp = promoteOperand(V);
    Type* Ty = V->getType();
    if(!shouldPromoteParam(Ty)) {
      Inst->setArgOperand(i, NewOp);
      continue;
    }

    if(isa<LoadInst>(V)) {
      LoadInst* Load = cast<LoadInst>(V);
      if(Load->isUnordered()) {
        Value* VPtr = Load->getPointerOperand();
        Value* NewVPtr = promoteOperand(VPtr);
        Inst->setArgOperand(i, NewVPtr);
        continue;
      }
    }

    AllocaInst* Alloca = new AllocaInst(promoteType(Ty),
                                        NULL,
                                        "",
                                        ParentF->getEntryBlock().getFirstNonPHI());
    CopyDebug(new StoreInst(NewOp, Alloca, Inst), Inst);
    Inst->setArgOperand(i, Alloca);
  }

  Type* Ty = Inst->getType();
  Type* NewTy = promoteType(Ty);
  Inst->mutateType(NewTy);
}

bool PromoteStructureArgs::runOnModule(Module& M) {
  size_t Promoted = 0;
  {
    const Module::iterator end = M.end();
    for(Module::iterator i = M.begin(); i != end; ++i) {
      promoteGlobal(i);
      ++Promoted;
    }
  }

  Promoted = 0;
  {
    const Module::global_iterator end = M.global_end();
    for(Module::global_iterator i = M.global_begin(); i != end; ++i) {
      promoteGlobal(i);
      ++Promoted;
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
