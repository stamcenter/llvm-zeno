//===-- RISCVZenoFoldPseudo.cpp - Debug Zeno instructions -----------===//
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
#include "RISCVZenoHelper.h"
#include "RISCVMachineFunctionInfo.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-zeno-fold-pseduo"

#define _PASS_NAME "RISCV Zeno Fold Pseudo pass"

namespace {

class RISCVZenoFoldPseudo : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  const MachineRegisterInfo* MRI;
  static char ID;

  RISCVZenoFoldPseudo() : MachineFunctionPass(ID) {
    initializeRISCVZenoFoldPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
  bool runOnMBB(MachineBasicBlock &MBB);
  bool runOnMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
               MachineBasicBlock::iterator &NextMBBI);


  StringRef getPassName() const override { return _PASS_NAME; }
};

char RISCVZenoFoldPseudo::ID = 0;

bool RISCVZenoFoldPseudo::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  MRI = &MF.getRegInfo();

  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= runOnMBB(MBB);
  return Modified;
}

bool RISCVZenoFoldPseudo::runOnMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= runOnMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool RISCVZenoFoldPseudo::runOnMI(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MBBI,
                                MachineBasicBlock::iterator &NextMBBI) {
  bool Modified = false;

  // if we are setting the ptr, and the previous instruction has the same register dest as we have of operand, we can just rewrite
  // a1 = addi a0, 2
  // a0 = PseudoZenoSetPtr a1
  // BECOMES
  // a0 = addi a0, 2
  // if(NextMBBI->getOpcode() == RISCV::PseudoZenoSetPtr && NextMBBI->getOperand(1)) {

  // }

  return Modified;
}


} // end of anonymous namespace

INITIALIZE_PASS(RISCVZenoFoldPseudo, DEBUG_TYPE, _PASS_NAME, false, false)

namespace llvm {

FunctionPass *createRISCVZenoFoldPseudoPass() { return new RISCVZenoFoldPseudo(); }

} // end of namespace llvm

#undef _PASS_NAME
