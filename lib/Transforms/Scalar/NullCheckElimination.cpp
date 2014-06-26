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
    enum NullCheckResult {
	NotNullCheck,
	NullCheckEq,
	NullCheckNe,
    };

    bool isNonNullOrPoisonPhi(SmallPhiSet *VisitedPhis, PHINode*);

    NullCheckResult isCmpNullCheck(ICmpInst*);
    std::pair<Use*, NullCheckResult> findNullCheck(Use*);

    bool blockContainsLoadDerivedFrom(BasicBlock*, Value*);

    DenseSet<Value*> NonNullOrPoisonValues;
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

bool NullCheckElimination::runOnFunction(Function &F) {
  if (skipOptnoneFunction(F))
    return false;

  bool Changed = false;

  // Collect argumetns with the `nonnull` attribute.
  for (auto &Arg : F.args()) {
    if (Arg.hasNonNullAttr())
      NonNullOrPoisonValues.insert(&Arg);
  }

  // Collect instructions that definitely produce nonnull-or-poison values.
  // At the moment, this is restricted to inbounds GEPs. It would be slightly
  // more difficult to include uses of values dominated by a null check, since
  // then we would have to consider uses instead of mere values.
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
	if (GEP->isInBounds()) {
	  NonNullOrPoisonValues.insert(GEP);
	}
      }
    }
  }

  // Find phis that are derived entirely from nonnull-or-poison values,
  // including other phis that are themselves derived entirely from these
  // values.
  for (auto &BB : F) {
    for (auto &I : BB) {
      auto *PN = dyn_cast<PHINode>(&I);
      if (!PN)
	break;

      SmallPhiSet VisitedPHIs;
      if (isNonNullOrPoisonPhi(&VisitedPHIs, PN))
	NonNullOrPoisonValues.insert(PN);
    }
  }

  for (auto &BB : F) {
    // This could also be extended to handle SwitchInst, but using a SwitchInst
    // for a null check seems unlikely.
    auto *BI = dyn_cast<BranchInst>(BB.getTerminator());
    if (!BI || BI->isUnconditional())
      continue;

    // The first operand of a conditional branch is the condition.
    auto result = findNullCheck(&BI->getOperandUse(0));
    if (!result.first)
      continue;
    assert((result.second == NullCheckEq || result.second == NullCheckNe) &&
	   "valid null check kind expected if ICmpInst was found");

    BasicBlock *NonNullBB;
    if (result.second == NullCheckEq) {
      // If the comparison instruction is checking for equaliity with null,
      // then the pointer is nonnull on the `false` branch.
      NonNullBB = BI->getSuccessor(1);
    } else {
      // Otherwise, if the comparison instruction is checking for inequality
      // with null, the pointer is nonnull on the `true` branch.
      NonNullBB = BI->getSuccessor(0);
    }

    Use *U = result.first;
    ICmpInst *CI = cast<ICmpInst>(U->get());
    unsigned nonConstantIndex;
    if (isa<Constant>(CI->getOperand(0)))
      nonConstantIndex = 1;
    else
      nonConstantIndex = 0;

    // Due to the semantics of poison values in LLVM, we have to check that
    // there is actually some externally visible side effect that is dependent
    // on the poison value. Since poison values are otherwise treated as undef,
    // and a load of undef is undefined behavior (which is externally visible),
    // it suffices to look for a load of the nonnull-or-poison value.
    //
    // This could be extended to any block control-dependent on this branch of
    // the null check, it's unclear if that will actually catch more cases in
    // real code.
    Value *PtrV = CI->getOperand(nonConstantIndex);
    if (blockContainsLoadDerivedFrom(NonNullBB, PtrV)) {
      Type *BoolTy = CI->getType();
      Value *NewV = ConstantInt::get(BoolTy, result.second == NullCheckNe);
      U->set(NewV);
    }
  }

  NonNullOrPoisonValues.clear();

  return Changed;
}

/// Checks whether a phi is derived from known nonnnull-or-poison values,
/// including other phis that are derived from the same. May return `false`
/// conservatively in some cases, e.g. if exploring a large cycle of phis.
bool
NullCheckElimination::isNonNullOrPoisonPhi(SmallPhiSet *VisitedPhis,
					   PHINode *PN) {
  // If we've already seen this phi, return `true`, even though it may not be
  // nonnull, since some other operand in a cycle of phis may invalidate the
  // optimistic assumption that the entire cycle is nonnull, including this phi.
  if (!VisitedPhis->insert(PN).second)
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
    } else if (auto *SrcPN = dyn_cast<PHINode>(SrcValue)) {
      if (!isNonNullOrPoisonPhi(VisitedPhis, SrcPN))
	return false;
    } else {
      return false;
    }
  }

  return true;
}

/// Determines whether an ICmpInst is a null check of a known nonnull-or-poison
/// value.
NullCheckElimination::NullCheckResult
NullCheckElimination::isCmpNullCheck(ICmpInst *CI) {
  if (!CI->isEquality())
    return NotNullCheck;

  unsigned constantIndex;
  if (NonNullOrPoisonValues.count(CI->getOperand(0)))
    constantIndex = 1;
  else if (NonNullOrPoisonValues.count(CI->getOperand(1)))
    constantIndex = 0;
  else
    return NotNullCheck;

  auto *C = dyn_cast<Constant>(CI->getOperand(constantIndex));
  if (!C || !C->isZeroValue())
    return NotNullCheck;

  return
    CI->getPredicate() == llvm::CmpInst::ICMP_EQ ? NullCheckEq : NullCheckNe;
}

/// Finds the Use, if any, of an ICmpInst null check of a nonnull-or-poison
/// value.
std::pair<Use*, NullCheckElimination::NullCheckResult>
NullCheckElimination::findNullCheck(Use *U) {
  auto *I = dyn_cast<Instruction>(U->get());
  if (!I)
    return std::make_pair(nullptr, NotNullCheck);

  if (auto *CI = dyn_cast<ICmpInst>(I)) {
    NullCheckResult result = isCmpNullCheck(CI);
    if (result == NotNullCheck)
      return std::make_pair(nullptr, NotNullCheck);
    else
      return std::make_pair(U, result);
  }

  unsigned opcode = I->getOpcode();
  if (opcode == Instruction::Or || opcode == Instruction::And) {
    auto result = findNullCheck(&I->getOperandUse(0));
    if (result.second == NotNullCheck)
      return findNullCheck(&I->getOperandUse(1));
    else
      return result;
  }

  return std::make_pair(nullptr, NotNullCheck);
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
    while (NonNullOrPoisonValues.count(V)) {
      if (V == PtrV)
	return true;

      auto *GEP = dyn_cast<GetElementPtrInst>(V);
      if (!GEP)
	break;

      V = GEP->getOperand(0);
    }
  }

  return false;
}

