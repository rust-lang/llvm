//===- PromoteSimpleStructs.cpp - Expand out structs with a single element-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//


#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include <iostream>
#include <set>

using namespace llvm;

struct PromoteSimpleStructs : public ModulePass {
  static char ID;
  PromoteSimpleStructs() : ModulePass(ID) {
    initializePromoteSimpleStructsPass(*PassRegistry::getPassRegistry());
  }
    
  struct ConversionState {
    typedef std::set<Instruction*>::iterator iterator;
    std::set<Instruction*> m_replacements;
    PromoteSimpleStructs* m_p;

    Function* m_f;

    void convertOperands(User* From);

    void recordConverted(Instruction* I);
    void eraseConverted(Instruction* I);
    size_t convertedSize();
      
    Value* get(Value* From, Type** OldTy = NULL);

    void convertBlock(BasicBlock* Bb);
    Value* convertInstruction(Instruction* Inst);
    Value* convertGEPInstruction(GetElementPtrInst* Inst,
                                 Type* OriginalTy,
                                 Type* PromotedTy,
                                 Value* PointerOp,
                                 Type* PointerOpOriginalTy);
    Value* convertEVOrIVInstruction(Instruction* Inst,
                                    Type* OriginalTy,
                                    Type* PromotedTy,
                                    Value* AggOp,
                                    Type* AggOpOriginalTy);
    template <class T> void convertCall(T* Call);
    void possiblyConvertUsers(Instruction* Inst, Value* Replacement, Type* OriginalTy);

    ConversionState() {}
    ConversionState(PromoteSimpleStructs* P, Function* F) 
      : m_p(P), m_f(F) {
      const Function::arg_iterator end = F->arg_end();
      for(Function::arg_iterator i = F->arg_begin(); i != end; ++i) {
        Type* OriginalTy = i->getType();
        Type* PromotedTy = m_p->getPromotedType(OriginalTy);
        m_p->mutateAndReplace(i, i, OriginalTy, PromotedTy);
      }
    }
    ~ConversionState() {
      const PromoteSimpleStructs::origin_ty_iterator end = m_p->m_original_types.end();
      for(PromoteSimpleStructs::origin_ty_iterator i = m_p->m_original_types.begin();
          i != end;) {
        if(isa<Instruction>(i->first) || isa<Argument>(i->first))
          m_p->m_original_types.erase(i++);
        else
          ++i;
      }
    }
  };
  
  Module* m_module;
  typedef std::set<GlobalValue*>::iterator iterator;
  typedef std::map<Type*, Type*>::iterator ty_iterator;
  typedef std::map<Value*, Type*>::iterator origin_ty_iterator;
  typedef std::map<Constant*, Constant*>::iterator const_iterator;

  std::set<GlobalValue*> m_promoted;
  std::map<Type*, Type*> m_promoted_types;
  std::map<Constant*, Constant*> m_promoted_consts;
  std::map<Value*, Type*> m_original_types;
  std::stack<Constant*> m_delayed;

#ifndef NDEBUG
  void debug_print_all_original_types();
#endif

  GlobalValue* getPromoted(GlobalValue* F);
  Type* getPromotedType(Type* T);
  Type* getPromotedTypeImpl(Type* T);
  static bool shouldPromote(Type* T) {
    std::set<Type*> chain;
    return shouldPromote(T, chain);
  }
  static bool shouldPromote(Type* T, std::set<Type*>& Chain);
  static bool shouldPromote(Function* F);
  inline static bool isShallowPromotable(Type* T) {
    return (isa<StructType>(T) && cast<StructType>(T)->getNumElements() == 1) ||
      (isa<ArrayType>(T)       && cast<ArrayType>(T)->getNumElements() == 1) ||
      (isa<PointerType>(T)     && isShallowPromotable(T->getContainedType(0)));
  }
  inline Constant* getPromotedConstant(Use* U) {
    return getPromotedConstant(cast<Constant>(U));
  }
  Constant* getPromotedConstant(Constant* C);

  Type* getOriginalType(Value* V);
  void recordOriginalType(Value* NewV, Type* OldTy);
  void eraseOriginalType(Value* V) {
    origin_ty_iterator i = m_original_types.find(V);
    assert(i != m_original_types.end());
    m_original_types.erase(i);
  }
  void mutateAndReplace(Value* OldV, Value* NewV, Type* OldT, Type* NewT);

  void promoteGlobal(GlobalVariable* G);
  Function* promoteFunction(Function& F, const bool PrototypeOnly = false);

  bool isAggregateType(Type* T);

  bool runOnModule(Module& M);
};
char PromoteSimpleStructs::ID = 0;
INITIALIZE_PASS(PromoteSimpleStructs,
                "promote-simple-structs",
                "Promote out structs with a single element", 
                false, 
                false)

template <class T> 
const std::string ToStr(const T &V) {
  std::string S;
  raw_string_ostream OS(S);
  OS << const_cast<T &>(V);
  return OS.str();
}

#ifndef NDEBUG
// for use in gdb
void debug_printf(Value* V) {
  std::cout << ToStr(*V) << std::endl;
}
void debug_printf(Type* T) {
  std::cout << ToStr(*T) << std::endl;
}

void debug_collect_all_subtypes(Type* T, std::set<Type*>& Collection);
void debug_printf_all_subtypes(Type* T) {
  std::set<Type*> C;
  debug_collect_all_subtypes(T, C);
  const std::set<Type*>::iterator end = C.end();
  for(std::set<Type*>::iterator i = C.begin(); i != end; ++i) {
    debug_printf(*i);
  }
}
void debug_collect_all_subtypes(Type* T, std::set<Type*>& Collection) {
  if(Collection.count(T) > 0)
    return;
  else if(isa<StructType>(T)) {
    Collection.insert(T);
  }

  for(size_t i = 0; i < T->getNumContainedTypes(); ++i) {
    debug_collect_all_subtypes(T->getContainedType(i), Collection);
  }
}

void PromoteSimpleStructs::debug_print_all_original_types() {
  const origin_ty_iterator end = m_original_types.end();
  for(origin_ty_iterator i = m_original_types.begin(); i != end; ++i) {
    std::cout << "Value        : " << ToStr(*i->first) << std::endl;
    std::cout << "Original type: " << ToStr(*i->second) << std::endl;
  }
}
#endif
size_t PromoteSimpleStructs::ConversionState::convertedSize() {
  return m_replacements.size();
}
void PromoteSimpleStructs::ConversionState::eraseConverted(Instruction* I) {
  iterator i = m_replacements.find(I);
#ifndef NDEBUG
  if(i == m_replacements.end()) {
      errs() << "Value: " << ToStr(*I) << "\n";
      assert(i != m_replacements.end() && "Value not converted!");
      llvm_unreachable("Value not converted!");
  }
#endif
  m_replacements.erase(i);
}
void PromoteSimpleStructs::recordOriginalType(Value* NewV, Type* OldTy) {
  std::pair<origin_ty_iterator, bool> R = m_original_types.insert(std::make_pair(NewV, OldTy));
  if(!R.second && R.first->second != OldTy) {
    errs() << "New value    : " << ToStr(*NewV) << "\n";
    errs() << "Original type: " << ToStr(*R.first->second) << "\n";
    errs() << "Old type     : " << ToStr(*OldTy) << "\n";
    assert(0 && "Value already promoted!");
    llvm_unreachable("Value already promoted!");
  }
}
void PromoteSimpleStructs::mutateAndReplace(Value* OldV, Value* NewV, Type* OldT, Type* NewT) {
  recordOriginalType(NewV, OldT);

  if(OldT != NewT)
    OldV->mutateType(NewT);
  if(OldV != NewV) {
    OldV->replaceAllUsesWith(NewV);
    if(isa<Instruction>(OldV))
      cast<Instruction>(OldV)->eraseFromParent();
  }
}

GlobalValue* PromoteSimpleStructs::getPromoted(GlobalValue* F) {
  if(!m_promoted.count(F)) {
    if(isa<Function>(F)) {
      promoteFunction(*cast<Function>(F), true);
    } else if(isa<GlobalVariable>(F)) {
      promoteGlobal(cast<GlobalVariable>(F));
    }
  }
  return F;
}
Type* PromoteSimpleStructs::getPromotedType(Type* T) {
  const ty_iterator i = m_promoted_types.find(T);
  if(i == m_promoted_types.end()) {
    Type* NewT = getPromotedTypeImpl(T);
    m_promoted_types.insert(std::make_pair(T, NewT));
    m_promoted_types.insert(std::make_pair(NewT, NewT));
    return NewT;
  }
  else
    return i->second;
}
Type* PromoteSimpleStructs::getOriginalType(Value* V) {
  if(isa<Constant>(V) && !isa<GlobalValue>(V))
    return V->getType();

  origin_ty_iterator i = m_original_types.find(V);
  if(i == m_original_types.end()) {
      errs() << "Value: " << ToStr(*V) << "\n";
#ifndef NDEBUG
      assert(0 && "Couldn't find the original type!");
#endif
      llvm_unreachable("Couldn't find the original type!");
  } else
    return i->second;
}

Type* PromoteSimpleStructs::getPromotedTypeImpl(Type* T) {
  if(!shouldPromote(T))
    return T;

  if(FunctionType* FT = dyn_cast<FunctionType>(T)) {
    Type* RetT;
    RetT = getPromotedType(FT->getReturnType());

    std::vector<Type*> ArgTs;
    ArgTs.reserve(FT->getNumParams());
  
    const FunctionType::param_iterator i_end = FT->param_end();
    for(FunctionType::param_iterator i = FT->param_begin(); i != i_end; ++i) {
        ArgTs.push_back(getPromotedType(*i));
    }

    return FunctionType::get(RetT, ArgTs, FT->isVarArg());
  } else if(PointerType* PT = dyn_cast<PointerType>(T)) {
    Type* InnerInnerTy = PT->getElementType();
    if(shouldPromote(InnerInnerTy))
      return PointerType::get(getPromotedType(InnerInnerTy), 0);
    else
      return T;
  } else if(StructType* ST = dyn_cast<StructType>(T)) {
    if(ST->getNumElements() == 0)
      return T;
    if(ST->getNumElements() == 1)
      return getPromotedType(ST->getElementType(0));

    StructType* Struct;
    if(ST->hasName())
      Struct = StructType::create(T->getContext(), ST->getName());
    else
      Struct = StructType::create(T->getContext());
    // This is a requisite for recursive structures.
    m_promoted_types[T] = Struct;

    std::vector<Type*> ArgTs;
    ArgTs.reserve(ST->getNumElements());

    const StructType::element_iterator end = ST->element_end();
    for(StructType::element_iterator i = ST->element_begin(); i != end; ++i) {
      ArgTs.push_back(getPromotedType(*i));
    }
    Struct->setBody(ArgTs);
  
    m_promoted_types.erase(T);

    return Struct;
  } else if(ArrayType* AT = dyn_cast<ArrayType>(T)) {
    if(AT->getNumElements() == 1)
      return getPromotedType(AT->getElementType());

    if(shouldPromote(AT->getElementType()))
      return ArrayType::get(getPromotedType(AT->getElementType()), AT->getNumElements());
    else
      return T;
  } else {
    return T;
  }
}

struct PromotionChainJanitor {
  std::set<Type*>& chain;
  Type* t;
  PromotionChainJanitor(Type* T, std::set<Type*>& Chain) 
    : chain(Chain)
    , t(T) {
    assert(chain.count(T) == 0);
    chain.insert(T);
  }
  ~PromotionChainJanitor() {
    assert(chain.count(t) != 0);
    chain.erase(t);
  }
};

bool PromoteSimpleStructs::shouldPromote(Type* T, std::set<Type*>& Chain) {
  assert(T != NULL);
  if(Chain.count(T) > 0)
    return false;

  PromotionChainJanitor cleanup(T, Chain);

  if(isa<FunctionType>(T)) {
    FunctionType* FT = cast<FunctionType>(T);
    if(shouldPromote(FT->getReturnType(), Chain))
      return true;
      
    const FunctionType::param_iterator end = FT->param_end();
    for(FunctionType::param_iterator i = FT->param_begin(); i != end; ++i) {
      if(shouldPromote(*i, Chain))
        return true;
    }

    return false;
  } else if(isa<StructType>(T)) {
    StructType* ST = cast<StructType>(T);
    if(ST->getNumElements() == 1)
      return true;

    const StructType::element_iterator end = ST->element_end();
    for(StructType::element_iterator i = ST->element_begin(); i != end; ++i) {
      // short cut for recursive structures
      if(shouldPromote(*i, Chain))
        return true;
    }
    return false;
  }

  return (isa<PointerType>(T) && shouldPromote(cast<PointerType>(T)->getElementType(), Chain)) ||
    (isa<ArrayType>(T)        && (cast<ArrayType>(T)->getNumElements() == 1 ||
                                  shouldPromote(T->getContainedType(0), Chain)));
}
bool PromoteSimpleStructs::shouldPromote(Function* F) {
  return F && shouldPromote(F->getFunctionType());
}
bool PromoteSimpleStructs::isAggregateType(Type* T) {
  return T && (isa<StructType>(T) || isa<ArrayType>(T));
}

void PromoteSimpleStructs::ConversionState::recordConverted(Instruction* I) {
  if(!I) {
    errs() << __FUNCTION__ << ":\n";
    assert(0);
    llvm_unreachable("I is NULL");
  }
  const bool result = m_replacements.insert(I).second;
  assert(result && "Instruction already patched!");
  (void)result;
}
Value* PromoteSimpleStructs::ConversionState::get(Value* From, Type** OldTy) {
  if(isa<Argument>(From)) {
    if(OldTy != NULL)
        *OldTy = m_p->getOriginalType(From);
    return From;
  } else if(isa<Instruction>(From)) {
    Instruction* Inst = cast<Instruction>(From);
    if(m_replacements.count(Inst)) {
      if(OldTy != NULL)
        *OldTy = m_p->getOriginalType(From);
      return From;
    } else {
      if(OldTy != NULL)
        *OldTy = Inst->getType();
      return convertInstruction(Inst);
    }
  } else if(isa<GlobalValue>(From)) {
    Value* Promoted = m_p->getPromoted(cast<GlobalValue>(From));
    if(OldTy != NULL)
      *OldTy = m_p->getOriginalType(From);
    return Promoted;
  } else if(isa<Constant>(From)) {
    if(OldTy != NULL)
      *OldTy = From->getType();
    return m_p->getPromotedConstant(cast<Constant>(From));
  } else if(isa<MDNode>(From) || isa<MDString>(From) || isa<BasicBlock>(From)) {
    if(OldTy != NULL)
      *OldTy = From->getType();
    return From;
  }

  assert(0 && "Unhandled case!");
  llvm_unreachable("Unhandled case!");
}

void PromoteSimpleStructs::ConversionState::convertOperands(User* From) {
  unsigned j = 0;
  const User::op_iterator end = From->op_end();
  for(User::op_iterator i = From->op_begin(); i != end; ++i, ++j) {
    Value* R = get(*i);
    // sometimes constant prop will short circuit an expression
    // possibly yielding a global var; hence check the old operand
    // for global var-ness.
    if(isa<Constant>(R) && !isa<GlobalValue>(*i))
      From->setOperand(j, R);
  }
}

void PromoteSimpleStructs::ConversionState::convertBlock(BasicBlock* Bb) {
  const BasicBlock::iterator j_end = Bb->end();
  for(BasicBlock::iterator j = Bb->begin(); j != j_end;) {
    Instruction* Inst = cast<Instruction>(&*(j++));
    if(&*j == NULL) {
      // restart if our iterator was invalidated.
      // this is Okay because m_replacements will still hold
      // the state on whats converted and whats not,
      // though the inevitable waste is a bit unfortunate.
      j = Bb->begin();
      continue;
    }

    if(!m_replacements.count(Inst))
      convertInstruction(Inst);
  } // basicblock
}
void PromoteSimpleStructs::ConversionState::possiblyConvertUsers(Instruction* Inst,
                                                                 Value* Replacement,
                                                                 Type* OriginalTy) {
  const Value::use_iterator end = Inst->use_end();
  for(Value::use_iterator i = Inst->use_begin(); i != end;) {
    if(!isa<Instruction>(*i)) {
      ++i;
      continue;
    } else if(m_replacements.count(cast<Instruction>(*i))) {
      ++i;
      continue;
    } else if(isa<GetElementPtrInst>(*i)) {
      GetElementPtrInst* GEP = cast<GetElementPtrInst>(*i++);
      if(GEP->getPointerOperand() != Inst)
        continue;

      Type* GEPOriginalTy = GEP->getType();
      Type* GEPPromotedTy = m_p->getPromotedType(GEPOriginalTy);

      recordConverted(GEP);
      convertGEPInstruction(GEP, GEPOriginalTy, GEPPromotedTy, Replacement, OriginalTy);
      eraseConverted(GEP);
    } else if(isa<ExtractValueInst>(*i)) {
      ExtractValueInst* EV = cast<ExtractValueInst>(*i++);
      if(EV->getAggregateOperand() != Inst)
        continue;

      Type* EVOriginalTy = EV->getType();
      Type* EVPromotedTy = m_p->getPromotedType(EVOriginalTy);

      Value* Converted;
      recordConverted(EV);
      Converted = convertEVOrIVInstruction(EV,
                                           EVOriginalTy,
                                           EVPromotedTy,
                                           Replacement,
                                           OriginalTy);
      if(Converted != EV)
        eraseConverted(EV);

    } else if(isa<InsertValueInst>(*i)) {
      InsertValueInst* IV = cast<InsertValueInst>(*i++);
      if(IV->getAggregateOperand() != Inst)
        continue;

      Type* IVOriginalTy = IV->getType();
      Type* IVPromotedTy = m_p->getPromotedType(IVOriginalTy);
      
      Value* Converted;

      recordConverted(IV);
      Converted = convertEVOrIVInstruction(IV,
                                           IVOriginalTy,
                                           IVPromotedTy,
                                           Replacement,
                                           OriginalTy);
      if(Converted != IV)
        eraseConverted(IV);
    } else
      ++i;
  }
}
Value* PromoteSimpleStructs::ConversionState::convertEVOrIVInstruction(Instruction* I,
                                                                       Type* OriginalTy,
                                                                       Type* PromotedTy,
                                                                       Value* AggOp,
                                                                       Type* AggOpOriginalTy) {
  ArrayRef<unsigned> Indices;

  if(isa<ExtractValueInst>(I))
    Indices = cast<ExtractValueInst>(I)->getIndices();
  else if(isa<InsertValueInst>(I))
    Indices = cast<InsertValueInst>(I)->getIndices();
      
  std::vector<unsigned> NewIndices;
  std::vector<unsigned> OldIndices;
  OldIndices.reserve(Indices.size());
  NewIndices.reserve(Indices.size());

  const size_t end = Indices.size();
  for(size_t i = 0; i < end; ++i) {
    unsigned Idx = Indices[i];
    Type* Ty = ExtractValueInst::getIndexedType(AggOpOriginalTy, OldIndices);
    if(!isShallowPromotable(Ty))
      NewIndices.push_back(Idx);
    OldIndices.push_back(Idx);
  }
  Value* Converted;
  if(NewIndices.size() == 0) {
    if(isa<ExtractValueInst>(I))
      Converted = AggOp;
    else if(isa<InsertValueInst>(I))
      Converted = get(cast<InsertValueInst>(I)->getInsertedValueOperand());

    m_p->recordOriginalType(I, OriginalTy);
    I->mutateType(PromotedTy);
    possiblyConvertUsers(I, Converted, OriginalTy);
    m_p->eraseOriginalType(I);
    I->replaceAllUsesWith(Converted);
    I->eraseFromParent();
  } else if(NewIndices.size() == OldIndices.size()) {
    if(isa<ExtractValueInst>(I))
      I->setOperand(ExtractValueInst::getAggregateOperandIndex(), AggOp);
    else if(isa<InsertValueInst>(I)) {
      I->setOperand(InsertValueInst::getAggregateOperandIndex(), AggOp);
      I->setOperand(InsertValueInst::getInsertedValueOperandIndex(),
                    get(cast<InsertValueInst>(I)->getInsertedValueOperand()));
    }
    Converted = I;
    m_p->mutateAndReplace(I, I, OriginalTy, PromotedTy);
  } else {
    Instruction* NewI;
    if(isa<ExtractValueInst>(I))
      NewI = CopyDebug(ExtractValueInst::Create(AggOp,
                                                NewIndices,
                                                "",
                                                I),
                       I);
    else if(isa<InsertValueInst>(I))
      NewI =
        CopyDebug(InsertValueInst::Create(AggOp,
                                          get(cast<InsertValueInst>(I)->getInsertedValueOperand()),
                                          NewIndices,
                                          "",
                                          I),
                  I);

    recordConverted(NewI);
    Converted = NewI;
    m_p->mutateAndReplace(I, NewI, OriginalTy, PromotedTy);
  }
  return Converted;
}
Value* PromoteSimpleStructs::ConversionState::convertGEPInstruction(GetElementPtrInst* Inst,
                                                                    Type* OriginalTy,
                                                                    Type* PromotedTy,
                                                                    Value* PointerOp,
                                                                    Type* PointerOpOriginalTy) {
  std::vector<Value*> OldIndices;
  std::vector<Value*> NewIndices;
  OldIndices.reserve(Inst->getNumIndices());
  NewIndices.reserve(Inst->getNumIndices());

  bool SkipNext = false;

  const User::op_iterator end = Inst->idx_end();
  for(User::op_iterator i = Inst->idx_begin(); i != end; ++i) {
    assert(isa<Value>(*i) && "Woa. Internal error.");

    if(!SkipNext) {
      NewIndices.push_back(get(*i));
    } else {
      SkipNext = false;
    }
      
    OldIndices.push_back(cast<Value>(*i));

    Type* T = GetElementPtrInst::getIndexedType(PointerOpOriginalTy,
                                                OldIndices);
    if(isShallowPromotable(T)) {
      SkipNext = true;
    }
  }
  Value* Converted;
  if(NewIndices.size() != 0) {
    GetElementPtrInst* GEPI = GetElementPtrInst::Create(PointerOp,
                                                        NewIndices,
                                                        Inst->getName(),
                                                        Inst);
    GEPI->setIsInBounds(Inst->isInBounds());
    CopyDebug(Inst, GEPI);
    Converted = GEPI;
    recordConverted(GEPI);
    m_p->mutateAndReplace(Inst, GEPI, OriginalTy, PromotedTy);
  } else {
    assert(0 && "Invalid GEPi");
    errs() << "GEPi: " << ToStr(*Inst) << "\n";
    report_fatal_error("Invalid GEPi");
    /*Inst->mutateType(PointerOp->getType());
    possiblyConvertUsers(Inst, PointerOp, Inst->getType());
    Inst->replaceAllUsesWith(PointerOp);
    Inst->eraseFromParent();
    Converted = PointerOp;*/
  }
  return Converted;
}
template <class T>
void PromoteSimpleStructs::ConversionState::convertCall(T* Call) {
  const unsigned end = Call->getNumArgOperands();
  for(unsigned i = 0; i < end; ++i) {
    Value* V = Call->getArgOperand(i);
    Value* NewV = get(V);
    Call->setArgOperand(i, NewV);
  }
}
Value* PromoteSimpleStructs::ConversionState::convertInstruction(Instruction* I) {
  recordConverted(I);
  Value* Converted = NULL;
  Type* OriginalType = I->getType();
  Type* PromotedType = m_p->getPromotedType(OriginalType);

  if(isa<GetElementPtrInst>(I)) {
    GetElementPtrInst* Inst = cast<GetElementPtrInst>(I);
    Value* PointerOp = Inst->getPointerOperand();
    Type* PointerOpOriginalTy;
    PointerOp = get(PointerOp, &PointerOpOriginalTy);
    Converted = convertGEPInstruction(Inst,
                                      OriginalType,
                                      PromotedType,
                                      PointerOp,
                                      PointerOpOriginalTy);
  } else if(isa<ExtractValueInst>(I) || isa<InsertValueInst>(I)) {
    Value* AggOp;
    if(isa<ExtractValueInst>(I))
      AggOp = cast<ExtractValueInst>(I)->getAggregateOperand();
    else if(isa<InsertValueInst>(I))
      AggOp = cast<InsertValueInst>(I)->getAggregateOperand();

    Type* AggOpOriginalTy;
    AggOp = get(AggOp, &AggOpOriginalTy);

    Converted = convertEVOrIVInstruction(I,
                                         OriginalType,
                                         PromotedType,
                                         AggOp,
                                         AggOpOriginalTy);
  } else if(isa<PHINode>(I)) {
    PHINode* Phi = cast<PHINode>(I);
    m_p->mutateAndReplace(I, I, OriginalType, PromotedType);

    for(size_t l = 0; l < Phi->getNumIncomingValues(); ++l) {
      Value* NewIn = get(Phi->getIncomingValue(l));
      Phi->setIncomingValue(l, NewIn);
    }
    Converted = Phi;
  } else if(isa<CallInst>(I) || isa<InvokeInst>(I)) {
    m_p->mutateAndReplace(I, I, OriginalType, PromotedType);
    if(isa<CallInst>(I)) {
      CallInst* Call = cast<CallInst>(I);
      convertCall(Call);
    } else if(isa<InvokeInst>(I)) {
      InvokeInst* Invoke = cast<InvokeInst>(I);
      convertCall(Invoke);
    }
    Converted = I;
  } else {
    m_p->mutateAndReplace(I, I, OriginalType, PromotedType);
    convertOperands(I);
    Converted = I;
  }

  assert(Converted != NULL);
  if(Converted != I)
    eraseConverted(I);
  return Converted;
}
Constant* PromoteSimpleStructs::getPromotedConstant(Constant* C) {
  User::op_iterator i;
  Type* OriginalT;
  Type* NewT;

  std::pair<const_iterator, bool> ci =
    m_promoted_consts.insert(std::make_pair(C, (Constant*)NULL));
  if(!ci.second) {
    assert(ci.first->second != NULL && "Should not be null");
    return ci.first->second;
  }
  Constant*& NewC = ci.first->second;

  OriginalT = C->getType();
  if(!shouldPromote(C->getType())) {
    // constant expression still need their operands patched
    if(!isa<ConstantExpr>(C)   &&
       !isa<ConstantStruct>(C) &&
       !isa<ConstantArray>(C)  &&
       !isa<GlobalValue>(C)) {
      // we still need to record the 'original' type:
      recordOriginalType(C, C->getType());
      NewC = C;
      return C;
    } else
      NewT = OriginalT;
  } else
    NewT = getPromotedType(OriginalT);

  if(isa<GlobalValue>(C)) {
    NewC = getPromoted(cast<GlobalValue>(C));
  } else if(isa<UndefValue>(C)) { // fast path for this common const
    NewC = UndefValue::get(NewT);
  } else if(isa<ConstantExpr>(C)) {
    ConstantExpr* CE = cast<ConstantExpr>(C);
    unsigned Code = CE->getOpcode();

    i = C->op_begin();

    Constant* Agg = cast<Constant>(i++);
    Type* AggOriginalTy;
    const bool IsGlobal = isa<GlobalValue>(Agg);
    if(!IsGlobal)
      AggOriginalTy = Agg->getType();
    Agg = getPromotedConstant(Agg);
    if(IsGlobal)
      AggOriginalTy = getOriginalType(Agg);

    if(Code == Instruction::GetElementPtr) {
      assert(isa<PointerType>(AggOriginalTy) && "Original type isn't a pointer!");

      bool SkipNext = false;
      
      std::vector<Constant*> NewIndices;
      {
        std::vector<Constant*> OldIndices;
        OldIndices.reserve(C->getNumOperands());
        NewIndices.reserve(C->getNumOperands());

        const User::op_iterator end = C->op_end();
        for(; i != end; ++i) {
          Constant* C2 = cast<Constant>(*i);
          Constant* C3 = getPromotedConstant(C2);

          OldIndices.push_back(C2);

          if(!SkipNext)
            NewIndices.push_back(C3);
          else
            SkipNext = false;
          
          Type* T2 = GetElementPtrInst::getIndexedType(AggOriginalTy, OldIndices);
          if(isShallowPromotable(T2))
            SkipNext = true; 
        }
      }
      if(NewIndices.size() != 0)
        NewC = ConstantExpr::getGetElementPtr(Agg, NewIndices);
      else
        NewC = Agg;
    } else if(CE->hasIndices()) {
      bool SkipNext = false;

      Constant* Inserted = getPromotedConstant(cast<Constant>(i++));

      std::vector<unsigned> NewIndices;
      {
        std::vector<unsigned> OldIndices;
        OldIndices.reserve(C->getNumOperands());
        NewIndices.reserve(C->getNumOperands());

        const ArrayRef<unsigned> Indices = CE->getIndices();
        const size_t end = Indices.size();
        for(size_t i = 0; i < end; ++i) {
          unsigned Idx = Indices[i];

          OldIndices.push_back(Idx);

          if(!SkipNext)
            NewIndices.push_back(Idx);
          else
            SkipNext = false;

          Type* T2 = ExtractValueInst::getIndexedType(AggOriginalTy, OldIndices);
          if(isShallowPromotable(T2))
            SkipNext = true; 
        }
      }
      if(Code == Instruction::ExtractValue)
        NewC = ConstantExpr::getExtractValue(Agg, NewIndices);
      else if(Code == Instruction::InsertValue)
        NewC = ConstantExpr::getInsertValue(Agg,
                                            Inserted,
                                            NewIndices);
    } else {
      std::vector<Constant*> Consts;
      Consts.reserve(C->getNumOperands());
      const User::op_iterator end = C->op_end();
      for(i = C->op_begin(); i != end; ++i) {
        Constant* C2 = cast<Constant>(*i);
        Consts.push_back(getPromotedConstant(C2));
      }
      NewC = CE->getWithOperands(Consts, NewT);
    }
  } else if(isShallowPromotable(C->getType())) {
    if(isa<ConstantStruct>(C) || isa<ConstantArray>(C))
      NewC = getPromotedConstant(C->getAggregateElement((unsigned)0));
    else if(isa<ConstantAggregateZero>(C)) {
      if(isAggregateType(NewT) || isa<VectorType>(NewT)) {
        NewC = C;
      } else if(isa<PointerType>(NewT)) {
        NewC = ConstantPointerNull::get(cast<PointerType>(NewT));
      } else if(NewT->isIntegerTy()) {
        IntegerType* IntTy = cast<IntegerType>(NewT);
        NewC = ConstantInt::get(IntTy, 0, IntTy->getSignBit());
      } else if(NewT->isFloatingPointTy()) {
        NewC = ConstantFP::get(NewT, 0.0);
      } else {
        errs() << "Constant: " << ToStr(*C) << "\n";
        assert(0 && "Unhandled else");
        llvm_unreachable("Unhandled else");
      }
    } else if(isa<ConstantPointerNull>(C)) {
      NewC = ConstantPointerNull::get(cast<PointerType>(NewT));
    } else if(isa<ConstantDataSequential>(C))
      NewC = cast<ConstantDataSequential>(C)->getElementAsConstant(0);
    else
      NewC = getPromotedConstant(cast<Constant>(C->getOperand(0)));
  } else if(isa<ConstantDataSequential>(C)) {
    if(cast<ConstantDataSequential>(C)->getNumElements() == 1)
      NewC = cast<ConstantDataSequential>(C)->getElementAsConstant(0);
    else
      NewC = C;
  } else if(isa<ConstantStruct>(C) ||
            isa<ConstantArray>(C)) {
    std::vector<Constant*> Consts;
    Consts.reserve(C->getNumOperands());
    const User::op_iterator end = C->op_end();
    for(i = C->op_begin(); i != end; ++i) {
      Constant* C2 = cast<Constant>(*i);
      Consts.push_back(getPromotedConstant(C2));
    }
    if(isa<ConstantStruct>(C))
      NewC = ConstantStruct::get(cast<StructType>(NewT), Consts);
    else if(isa<ConstantArray>(C))
      NewC = ConstantArray::get(cast<ArrayType>(NewT), Consts);
  } else if(isa<BlockAddress>(C)) {
    BlockAddress* Addr = cast<BlockAddress>(C);

    // make sure the function type is promoted
    getPromoted(Addr->getFunction());
    NewC = C;
  } else if(isa<ConstantPointerNull>(C)) {
    NewC = ConstantPointerNull::get(cast<PointerType>(NewT));
  } else {
    NewC = C;
  }

  assert(NewC != NULL && "NewC is NULL");
  m_promoted_consts.insert(std::make_pair(NewC, NewC));
  return NewC;
}


void PromoteSimpleStructs::promoteGlobal(GlobalVariable* G) {
  std::pair<iterator, bool> result = m_promoted.insert(G);
  if(!result.second)
    return;

  Type* OriginalTy = G->getType();
  Type* NewTy = getPromotedType(OriginalTy);
  mutateAndReplace(G, G, OriginalTy, NewTy);
  if(G->hasInitializer()) {
    Constant* Old = G->getInitializer();
    Constant* Initer = getPromotedConstant(Old);
    G->setInitializer(Initer);
  }
  
  // sometimes we can't reach all the uses of a GV.
  // don't ask me how such a Constant would ever be unreachable
  // but asserts are thrown later.
  const Value::use_iterator end = G->use_end();
  for(Value::use_iterator i = G->use_begin(); i != end; ++i) {
    if(isa<Constant>(*i))
      m_delayed.push(cast<Constant>(*i));
  }
}

static size_t ConvertedFunctions;

Function* PromoteSimpleStructs::promoteFunction(Function& F, const bool PrototypeOnly) {
  if(F.isIntrinsic()) {
    m_promoted.insert(&F);
    return &F;
  }

  std::pair<iterator, bool> result = m_promoted.insert(&F);
  Type* NewTy;
  Function* NewF;
  if(result.second) {
    Type* OriginalTy = F.getType();
    NewTy = getPromotedType(OriginalTy);
    mutateAndReplace(&F, &F, OriginalTy, NewTy);
    NewF = &F;
  } else {
    NewF = &F;
    NewTy = NewF->getType();
  }

  if(PrototypeOnly)
    return NewF;

  ConversionState State(this, &F);

  const Function::iterator i_end = F.end();
  for(Function::iterator i = F.begin(); i != i_end; ++i) {
    State.convertBlock(&*i);
  } // function

  ConvertedFunctions++;

  return NewF;
}

bool PromoteSimpleStructs::runOnModule(Module& M) {
  m_module = &M;

  const Module::iterator i_end = M.end();
  for(Module::iterator i = M.begin(); i != i_end; ++i) {
    promoteFunction(*i, false);
  }
  const Module::global_iterator j_end = M.global_end();
  for(Module::global_iterator j = M.global_begin(); j != j_end; ++j) {
    promoteGlobal(j);
  }
  
  // remove dangling consts:
  {
    const const_iterator end = m_promoted_consts.end();
    for(const_iterator i = m_promoted_consts.begin(); i != end; ++i) {
      (*i).second->removeDeadConstantUsers();
    }
  }
  {
    const iterator end = m_promoted.end();
    for(iterator i = m_promoted.begin(); i != end; ++i) {
      (*i)->removeDeadConstantUsers();
    }
  }

  m_original_types.clear();
  m_promoted_types.clear();
  m_promoted.clear();
  m_promoted_consts.clear();
  while(!m_delayed.empty())
    m_delayed.pop();
  return true;
}

ModulePass *llvm::createPromoteSimpleStructsPass() {
  return new PromoteSimpleStructs();
}
