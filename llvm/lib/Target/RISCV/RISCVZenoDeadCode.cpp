//===-- RISCVZenoDeadCode.cpp -  -----------===//
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

#define DEBUG_TYPE "riscv-zeno-dce"

#define RISCV_ZENO_DCE "RISCV Zeno dead code elimination pass"

namespace {

class RISCVZenoDCE : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVZenoDCE() : MachineFunctionPass(ID) {
    initializeRISCVZenoDCEPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return RISCV_ZENO_DCE; }

private:
  bool runOnMBB(MachineBasicBlock &MBB);

protected:
};

char RISCVZenoDCE::ID = 0;

bool RISCVZenoDCE::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= runOnMBB(MBB);
  return Modified;
}

bool RISCVZenoDCE::runOnMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);

    // ext2ext movs right next to each other with same dest, keep the second one
    if (NMBBI != E && MBBI->getOpcode() == RISCV::EADDIX &&
        NMBBI->getOpcode() == RISCV::EADDIX && 
        MBBI->getOperand(0).isReg() &&
        NMBBI->getOperand(0).isReg() &&
        MBBI->getOperand(0).getReg() == NMBBI->getOperand(0).getReg()) {
      MBBI->removeFromParent();
    }
    // mv ext2ext with the same ext, nop so remove it
    else if (MBBI->getOpcode() == RISCV::EADDIX &&
             MBBI->getOperand(0).isReg() && MBBI->getOperand(1).isReg() &&
             MBBI->getOperand(0).getReg() == MBBI->getOperand(1).getReg() &&
             MBBI->getOperand(2).isImm() && MBBI->getOperand(2).getImm() == 0) {
      MBBI->removeFromParent();
    }
    // mv gpr2gpr with the same gpr, nop so remove it
    else if (MBBI->getOpcode() == RISCV::ADDI && 
             MBBI->getOperand(0).isReg() && MBBI->getOperand(1).isReg() &&
             MBBI->getOperand(0).getReg() == MBBI->getOperand(1).getReg() &&
             MBBI->getOperand(2).isImm() && MBBI->getOperand(2).getImm() == 0) {
      MBBI->removeFromParent();
    }

    MBBI = NMBBI;
  }

  return Modified;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVZenoDCE, "riscv-zeno-dce", RISCV_ZENO_DCE, false, false)

namespace llvm {

FunctionPass *createRISCVZenoDCEPass() { return new RISCVZenoDCE(); }

} // end of namespace llvm
