//===-- RISCVZenoSelectExtReg.cpp -  -----------===//
//  Author : Secure, Trusted, and Assured Microelectronics (STAM) Center

//  Copyright (c) 2023 STAM Center (STAM/SCAI/ASU)
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.

//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVTargetMachine.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-zeno-select-ext-reg"

#define RISCV_ZENO_SELECT_EXT_REG "RISCV Zeno select extended register pass"

namespace {

class RISCVZenoSelectExtReg : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVZenoSelectExtReg() : MachineFunctionPass(ID) {
    initializeRISCVZenoSelectExtRegPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return RISCV_ZENO_SELECT_EXT_REG; }

private:
  bool runOnMBB(MachineBasicBlock &MBB);

protected:
};

char RISCVZenoSelectExtReg::ID = 0;

bool RISCVZenoSelectExtReg::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= runOnMBB(MBB);
  return Modified;
}

static bool isPseudoExtInst(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case RISCV::PseudoELP:
  case RISCV::PseudoELD:
  case RISCV::PseudoELW:
  case RISCV::PseudoELH:
  case RISCV::PseudoELHU:
  case RISCV::PseudoELB:
  case RISCV::PseudoELBU:
  case RISCV::PseudoELE:
  case RISCV::PseudoESP:
  case RISCV::PseudoESD:
  case RISCV::PseudoESW:
  case RISCV::PseudoESH:
  case RISCV::PseudoESB:
  case RISCV::PseudoESE:
    return true;
  }
  return false;
}

bool RISCVZenoSelectExtReg::runOnMBB(MachineBasicBlock &MBB) {
  bool Modified = false;
  auto MF = MBB.getParent();

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    DebugLoc DL = MBBI->getDebugLoc();
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);

    if (isPseudoExtInst(*MBBI) &&
        MBBI->getNumOperands() == 4 && MBBI->getOperand(3).getImm() != -1) {

      Register SrcPseudo = MBBI->getOperand(1).getReg();
      Register SrcGPR = SrcPseudo;
      if (SrcPseudo.id() >= RISCV::PXE0 && SrcPseudo <= RISCV::PXE31) {
        // convert pseudo to gpr
        SrcGPR = RISCV::X0 + (SrcPseudo.id() - RISCV::PXE0);
      }

      // need to insert spill and fill around this
      // bool hadToMoveExt = false;
      int FI = -1;
      int64_t ext_regid = MBBI->getOperand(3).getImm();
      // do we have an assigned reg id
      if (ext_regid >= 0 && ext_regid <= 31) {
        // src reg and ext id do not match, need to moveee
        int64_t src_regid = (SrcGPR.id() - RISCV::X0);
        if (src_regid != ext_regid) {
          Register PrevExt = RISCV::E0 + src_regid;
          Register NewExt = RISCV::E0 + ext_regid;

          // currently not using spill/fill, likely not nescacry cuz this is
          // done pre reg allocation
          // further, if u are using intrinsics, you "know more" than the
          // compiler and except that risc (pun intended)

          // spill PrevExt
          // FI = MF->getFrameInfo().CreateStackObject(
          //     8, Align(8),
          //     false); /*should make this reuse existing stack slot, so we
          //     dont
          //                have a new stack slot every time*/
          // TII->storeRegToStackSlot(MBB, MBBI, PrevExt, false, FI,
          //                          &RISCV::ER64RegClass,
          //                          MF->getSubtarget().getRegisterInfo());

          // Prevext = Newext
          BuildMI(MBB, MBBI, DL, TII->get(RISCV::EADDIX))
              .addReg(PrevExt)
              .addReg(NewExt)
              .addImm(0);

          // we have added instruction for pseudo reg, we can mark it as such
          MBBI->getOperand(3).setImm(-1);

          // insert after MBBI, so NMBBI goes in here
          // TII->loadRegFromStackSlot(MBB, NMBBI, PrevExt, FI,
          //                           &RISCV::ER64RegClass,
          //                           MF->getSubtarget().getRegisterInfo());
        }
      }
    }

    MBBI = NMBBI;
  }

  return Modified;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVZenoSelectExtReg, "riscv-zeno-select-ext-reg",
                RISCV_ZENO_SELECT_EXT_REG, false, false)

namespace llvm {

FunctionPass *createRISCVZenoSelectExtRegPass() {
  return new RISCVZenoSelectExtReg();
}

} // end of namespace llvm
