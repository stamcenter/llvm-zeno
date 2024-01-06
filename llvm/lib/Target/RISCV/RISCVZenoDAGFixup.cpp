//===-- RISCVZenoDAGFixup.cpp - Debug Zeno instructions -----------===//
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

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-zeno-dag-fixup"

#define _PASS_NAME "RISCV ZENO DAG fixup pass"

namespace {

class RISCVZenoDAGFixup : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVZenoDAGFixup() : MachineFunctionPass(ID) {
    initializeRISCVZenoDAGFixupPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
  bool runOnMBB(MachineBasicBlock &MBB);
  bool runOnMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
               MachineBasicBlock::iterator &NextMBBI);

  bool fixupPseudoPointerInst(MachineInstr &MI);

  StringRef getPassName() const override { return _PASS_NAME; }
};

char RISCVZenoDAGFixup::ID = 0;

bool RISCVZenoDAGFixup::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  MachineFrameInfo &MFI = MF.getFrameInfo();


  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= runOnMBB(MBB);
  return Modified;
}

bool RISCVZenoDAGFixup::runOnMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= runOnMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool RISCVZenoDAGFixup::runOnMI(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MBBI,
                                MachineBasicBlock::iterator &NextMBBI) {

  unsigned Opcode = MBBI->getOpcode();

  bool Modified = false;

  if (Opcode == RISCV::PseudoELP || Opcode == RISCV::PseudoESP) {
    Modified |= fixupPseudoPointerInst(*MBBI);
  }

  return false;
}

bool RISCVZenoDAGFixup::fixupPseudoPointerInst(MachineInstr &MI) {
  assert(MI.getOpcode() == RISCV::PseudoELP ||
         MI.getOpcode() == RISCV::PseudoESP);

  if(!MI.getOperand(0).isReg()) return false;

  Register Reg = MI.getOperand(0).getReg();
  if (zeno::isPseudoReg(Reg)) {
    return false;
  }

  MI.getOperand(0).setReg(zeno::getPseudoReg(Reg));

  return true;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVZenoDAGFixup, DEBUG_TYPE, _PASS_NAME, false, false)

namespace llvm {

FunctionPass *createRISCVZenoDAGFixupPass() { return new RISCVZenoDAGFixup(); }

} // end of namespace llvm

#undef _PASS_NAME
