//===-- NullCheckElimination.cpp - Null Check Elimination Pass ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
using namespace llvm;

#define DEBUG_TYPE "null-check-elimination"

namespace {
  struct NullCheckElimination : public FunctionPass {
    static char ID;
    NullCheckElimination() : FunctionPass(ID) {
      initializeNullCheckEliminationPass(*PassRegistry::getPassRegistry());
    }
    bool runOnFunction(Function &F) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesCFG();
    }

  private:
    static const unsigned kPhiLimit = 16;
    typedef SmallPtrSet<PHINode*, kPhiLimit> SmallPhiSet;

    enum CmpKind {
      /// A null check of an unconditionally nonnull-or-poison value.
      NullCheckDefiniteCmp,

      /// A null check of the phi representing a nontrivial inbounds recurrence,
      /// which is not known to be unconditionally nonnull-or-poison.
      NullCheckRecurrenceCmp,

      /// A comparison of the phi representing a nontrivial inbounds recurrence
      /// with an inbounds GEP derived from the base of the recurrence, which
      /// will typically represent a bound on the recurrence.
      RecurrencePhiBoundCmp,
    };

    enum CmpPred {
      CmpEq,
      CmpNe,
    };

    struct CmpDesc {
      CmpDesc(CmpKind k, CmpPred p, Use *u, Value *v)
        : kind(k), pred(p), use(u), ptrValue(v) { }

      CmpKind kind;
      CmpPred pred;
      Use *use;
      Value *ptrValue;
    };

    typedef SmallVector<CmpDesc, 4> CmpDescVec;

    bool isNonNullOrPoisonPhi(SmallPhiSet *VisitedPhis, PHINode*);
    Value *isNontrivialInBoundsRecurrence(PHINode*);

    bool classifyCmp(CmpDescVec*, Use*);
    bool findRelevantCmps(CmpDescVec*, Use*);

    bool blockContainsLoadDerivedFrom(BasicBlock*, Value*);

    /// Tracks values that are unconditionally nonnull-or-poison by definition,
    /// but not values that are known nonnull-or-poison in a given context by
    /// their uses, e.g. in recurrences.
    DenseSet<Value*> NonNullOrPoisonValues;

    /// Tracks values that are bases of nontrivial inbounds recurrences.
    DenseSet<Value*> InBoundsRecurrenceBases;

    /// Maps phis that correspond to nontrivial inbounds recurrences to their
    /// base values.
    DenseMap<Value*, Value*> InBoundsRecurrenceBaseMap;
  };
}

char NullCheckElimination::ID = 0;
INITIALIZE_PASS_BEGIN(NullCheckElimination,
                      "null-check-elimination",
                      "Null Check Elimination",
                      false, false)
INITIALIZE_PASS_END(NullCheckElimination,
                    "null-check-elimination",
                    "Null Check Elimination",
                    false, false)

FunctionPass *llvm::createNullCheckEliminationPass() {
  return new NullCheckElimination();
}

static GetElementPtrInst *castToInBoundsGEP(Value *V) {
  auto *GEP = dyn_cast<GetElementPtrInst>(V);
  if (!GEP || !GEP->isInBounds())
    return nullptr;
  return GEP;
}

static bool isZeroConstant(Value *V) {
  auto *C = dyn_cast<Constant>(V);
  return C && C->isZeroValue();
}

bool NullCheckElimination::runOnFunction(Function &F) {
  if (skipOptnoneFunction(F))
    return false;

  bool Changed = false;

  // Collect arguments with the `nonnull` attribute.
  for (auto &Arg : F.args()) {
    if (Arg.hasNonNullAttr())
      NonNullOrPoisonValues.insert(&Arg);
  }

  // Collect instructions that definitely produce nonnull-or-poison values. At
  // the moment, this is restricted to inbounds GEPs, and phis that are derived
  // entirely from nonnull-or-poison-values (including other phis that are
  // themselves derived from the same).
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *GEP = castToInBoundsGEP(&I)) {
        NonNullOrPoisonValues.insert(GEP);
      } else if (auto *PN = dyn_cast<PHINode>(&I)) {
        SmallPhiSet VisitedPHIs;
        if (isNonNullOrPoisonPhi(&VisitedPHIs, PN))
          NonNullOrPoisonValues.insert(PN);

        if (auto *BaseV = isNontrivialInBoundsRecurrence(PN)) {
          InBoundsRecurrenceBases.insert(BaseV);
          InBoundsRecurrenceBaseMap[PN] = BaseV;
        }
      }
    }
  }

  for (auto &BB : F) {
    // This could also be extended to handle SwitchInst, but using a SwitchInst
    // for a null check seems unlikely.
    auto *BI = dyn_cast<BranchInst>(BB.getTerminator());
    if (!BI || BI->isUnconditional())
      continue;

    // The first operand of a conditional branch is the condition.
    CmpDescVec Cmps;
    if (!findRelevantCmps(&Cmps, &BI->getOperandUse(0)))
      continue;

    for (auto &Cmp : Cmps) {
      // We are only tracking comparisons of inbounds recurrence phis with their
      // bounds so that we can eliminate null checks based on them, which are of
      // kind NullCheckRecurrenceCmp. We can't use a lone RecurrencePhiBoundCmp
      // to perform any optimizations.
      if (Cmp.kind == RecurrencePhiBoundCmp)
        continue;

      if (Cmp.kind == NullCheckRecurrenceCmp) {
        // Look for a matching RecurrencePhiBoundCmp. If one exists, then we can
        // be sure that this branch condition depends on the recurrence. Since
        // both the bounds and the recurrence successor value are inbounds, and
        // they are both derived from the base, the base being null would imply
        // that the bounds and recurrence successor values are poison.
        bool FoundMatchingCmp = false;
        for (auto &OtherCmp : Cmps) {
          if (OtherCmp.kind == RecurrencePhiBoundCmp &&
              OtherCmp.ptrValue == Cmp.ptrValue) {
            FoundMatchingCmp = true;
            break;
          }
        }
        if (!FoundMatchingCmp)
          continue;
      }

      BasicBlock *NonNullBB;
      if (Cmp.pred == CmpEq) {
        // If the comparison instruction is checking for equality with null then
        // the pointer is nonnull on the `false` branch.
        NonNullBB = BI->getSuccessor(1);
      } else {
        // Otherwise, if the comparison instruction is checking for inequality
        // with null, the pointer is nonnull on the `true` branch.
        NonNullBB = BI->getSuccessor(0);
      }

      // This is a crude approximation of control dependence: if the branch
      // target has a single predecessor edge, then it must be control-
      // dependent on the branch.
      if (!NonNullBB->getSinglePredecessor())
        continue;

      // Due to the semantics of poison values in LLVM, we have to check that
      // there is actually some externally visible side effect that is dependent
      // on the poison value. Since poison values are otherwise treated as
      // undef, and a load of undef is undefined behavior (which is externally
      // visible), it suffices to look for a load of the nonnull-or-poison
      // value.
      //
      // This could be extended to any block control-dependent on this branch of
      // the null check, it's unclear if that will actually catch more cases in
      // real code.
      if (blockContainsLoadDerivedFrom(NonNullBB, Cmp.ptrValue)) {
        Type *BoolTy = Type::getInt1Ty(F.getContext());
        Value *NewV = ConstantInt::get(BoolTy, Cmp.pred == CmpNe);
        Cmp.use->set(NewV);
        Changed = true;
      }
    }
  }

  NonNullOrPoisonValues.clear();
  InBoundsRecurrenceBases.clear();
  InBoundsRecurrenceBaseMap.clear();

  return Changed;
}

/// Checks whether a phi is derived from known nonnnull-or-poison values,
/// including other phis that are derived from the same. May return `false`
/// conservatively in some cases, e.g. if exploring a large cycle of phis.
///
/// This function may also insert any inbounds GEPs that it finds into
/// NonNullOrPoisonValues.
bool
NullCheckElimination::isNonNullOrPoisonPhi(SmallPhiSet *VisitedPhis,
                                           PHINode *PN) {
  // If we've already seen this phi, return `true`, even though it may not be
  // nonnull, since some other operand in a cycle of phis may invalidate the
  // optimistic assumption that the entire cycle is nonnull, including this phi.
  if (!VisitedPhis->insert(PN))
    return true;

  // Use a sensible limit to avoid iterating over long chains of phis that are
  // unlikely to be nonnull.
  if (VisitedPhis->size() >= kPhiLimit)
    return false;

  unsigned numOperands = PN->getNumOperands();
  for (unsigned i = 0; i < numOperands; ++i) {
    Value *SrcValue = PN->getOperand(i);
    if (NonNullOrPoisonValues.count(SrcValue)) {
      continue;
    } else if (auto *GEP = castToInBoundsGEP(SrcValue)) {
      NonNullOrPoisonValues.insert(GEP);
    } else if (auto *SrcPN = dyn_cast<PHINode>(SrcValue)) {
      if (!isNonNullOrPoisonPhi(VisitedPhis, SrcPN))
        return false;
    } else {
      return false;
    }
  }

  return true;
}

/// Determines whether a phi corresponds to an inbounds recurrence where the
/// base is not a known nonnull-or-poison value. Returns the base value, or
/// null if the phi doesn't correspond to such a recurrence.
Value *NullCheckElimination::isNontrivialInBoundsRecurrence(PHINode *PN) {
  if (PN->getNumOperands() != 2)
    return nullptr;

  Value *BaseV;
  GetElementPtrInst *SuccessorI;
  if (auto *GEP = castToInBoundsGEP(PN->getOperand(0))) {
    BaseV = PN->getOperand(1);
    SuccessorI = GEP;
  } else if (auto *GEP = castToInBoundsGEP(PN->getOperand(1))) {
    BaseV = PN->getOperand(0);
    SuccessorI = GEP;
  } else {
    return nullptr;
  }

  if (NonNullOrPoisonValues.count(BaseV) || SuccessorI->getOperand(0) != PN)
    return nullptr;

  return BaseV;
}

/// Determines whether an ICmpInst is one of the forms that is relevant to
/// null check elimination, and then adds a CmpDesc to Cmps when applicable.
/// The ICmpInst is passed as a Use so this Use can be placed into the CmpDesc,
/// but the Use parameter must be a Use of an ICmpInst.
bool NullCheckElimination::classifyCmp(CmpDescVec *Cmps, Use *U) {
  auto *CI = cast<ICmpInst>(U);
  if (!CI->isEquality())
    return false;

  CmpPred Pred = (CI->getPredicate() == llvm::CmpInst::ICMP_EQ) ? CmpEq : CmpNe;
  Value *Op0 = CI->getOperand(0);
  Value *Op1 = CI->getOperand(1);

  if (NonNullOrPoisonValues.count(Op0)) {
    if (isZeroConstant(Op1)) {
      Cmps->push_back(CmpDesc(NullCheckDefiniteCmp, Pred, U, Op0));
      return true;
    }

    auto it = InBoundsRecurrenceBaseMap.find(Op1);
    if (it == InBoundsRecurrenceBaseMap.end())
      return false;

    auto *GEP = castToInBoundsGEP(Op0);
    if (!GEP)
      return false;

    auto *BaseV = it->second;
    if (GEP->getOperand(0) != BaseV)
      return false;

    Cmps->push_back(CmpDesc(RecurrencePhiBoundCmp, Pred, U, Op1));
    return true;
  }

  // Since InstCombine or InstSimplify should have canonicalized a comparison
  // with `null` to have the `null` in the second operand, we don't need to
  // handle the case where Op0 is `null` like we did with Op1 above.
  if (NonNullOrPoisonValues.count(Op1)) {
    auto it = InBoundsRecurrenceBaseMap.find(Op0);
    if (it == InBoundsRecurrenceBaseMap.end())
      return false;

    auto *GEP = castToInBoundsGEP(Op1);
    if (!GEP)
      return false;

    auto *BaseV = it->second;
    if (GEP->getOperand(0) != BaseV)
      return false;

    Cmps->push_back(CmpDesc(RecurrencePhiBoundCmp, Pred, U, Op0));
    return true;
  }

  if (InBoundsRecurrenceBaseMap.count(Op0)) {
    if (isZeroConstant(Op1)) {
      Cmps->push_back(CmpDesc(NullCheckRecurrenceCmp, Pred, U, Op0));
      return true;
    }
  }

  return false;
}

/// Classifies the comparisons that are relevant to null check elimination,
/// starting from a Use. The CmpDescs of the comparisons are collected in Cmps.
bool NullCheckElimination::findRelevantCmps(CmpDescVec *Cmps, Use *U) {
  auto *I = dyn_cast<Instruction>(U->get());
  if (!I)
    return false;

  if (isa<ICmpInst>(I))
    return classifyCmp(Cmps, U);

  unsigned Opcode = I->getOpcode();
  if (Opcode == Instruction::Or || Opcode == Instruction::And) {
    bool FoundCmps = findRelevantCmps(Cmps, &I->getOperandUse(0));
    FoundCmps |= findRelevantCmps(Cmps, &I->getOperandUse(1));
    return FoundCmps;
  }

  return false;
}

/// Determines whether `BB` contains a load from `PtrV`, or any inbounds GEP
/// derived from `PtrV`.
bool
NullCheckElimination::blockContainsLoadDerivedFrom(BasicBlock *BB,
                                                   Value *PtrV) {
  for (auto &I : *BB) {
    auto *LI = dyn_cast<LoadInst>(&I);
    if (!LI)
      continue;

    Value *V = LI->getPointerOperand();
    while (1) {
      if (V == PtrV)
        return true;

      auto *GEP = castToInBoundsGEP(V);
      if (!GEP)
        break;

      V = GEP->getOperand(0);
    }
  }

  return false;
}

