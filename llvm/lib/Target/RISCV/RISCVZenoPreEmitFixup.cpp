//===-- RISCVZenoPreEmitFixup.cpp - Debug Zeno instructions -----------===//
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

#define DEBUG_TYPE "riscv-zeno-pre-emit-fixup"

#define _PASS_NAME "RISCV ZENO Pre Emit fixup pass"

namespace {

class RISCVZenoPreEmitFixup : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVZenoPreEmitFixup() : MachineFunctionPass(ID) {
    initializeRISCVZenoPreEmitFixupPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
  bool runOnMBB(MachineBasicBlock &MBB);
  bool runOnMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
               MachineBasicBlock::iterator &NextMBBI);
  bool fixupFrame(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool fixupPseudoRegs(MachineBasicBlock &MBB,
                       MachineBasicBlock::iterator MBBI);
  bool fixupPseudoNSManagement(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator &MBBI,MachineBasicBlock::iterator &PMBBI);

  StringRef getPassName() const override { return _PASS_NAME; }
};

char RISCVZenoPreEmitFixup::ID = 0;

bool RISCVZenoPreEmitFixup::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());

  if (!MF.getSubtarget<RISCVSubtarget>().hasStdExtZZeno())
    return false;

  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= runOnMBB(MBB);
  return Modified;
}

bool RISCVZenoPreEmitFixup::runOnMBB(MachineBasicBlock &MBB) {
  bool Modified = false;


  // MUST run each pass individually

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    if (MBBI->getFlag(MachineInstr::FrameSetup) ||
      MBBI->getFlag(MachineInstr::FrameDestroy)) {
      Modified |= fixupFrame(MBB, MBBI);
    }
    MBBI = NMBBI;
  }

  MBBI = MBB.begin(); E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    MachineBasicBlock::iterator PMBBI = std::prev(MBBI);
    Modified |= fixupPseudoNSManagement(MBB, MBBI, PMBBI);
    MBBI = NMBBI;
  }

  MBBI = MBB.begin(); E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= fixupPseudoRegs(MBB, MBBI);
    MBBI = NMBBI;
  }


  return Modified;
}

bool RISCVZenoPreEmitFixup::fixupFrame(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {
  assert(MBBI->getFlag(MachineInstr::FrameSetup) ||
         MBBI->getFlag(MachineInstr::FrameDestroy));
  DebugLoc DL = MBBI->getDebugLoc();

  unsigned Opcode = MBBI->getOpcode();
  if (Opcode == RISCV::ADDI) {
    Register Dst = MBBI->getOperand(0).getReg();
    Register Src = MBBI->getOperand(1).getReg();
    if (Dst == RISCV::X8 && Src == RISCV::X2) {
      // insert after current MI
      BuildMI(MBB, std::next(MBBI), DL, TII->get(RISCV::EADDIX),
              zeno::getExtendedReg(Dst))
          .addReg(zeno::getExtendedReg(Src))
          .addImm(0)
          .setMIFlag(MachineInstr::FrameSetup);
      return true;
    }
  }

  return false;
}

bool RISCVZenoPreEmitFixup::fixupPseudoRegs(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator MBBI) {
  if(MBBI->isDebugInstr()) return false;

  bool Modified = false;
  // if any operand is a pseudo register, need to do some extra reg copies
  if (MBBI->getNumOperands() >= 2) {
    auto &Op0 = MBBI->getOperand(0);
    auto &Op1 = MBBI->getOperand(1);

    auto PseudoDst = Op0.isReg() && zeno::isPseudoReg(Op0.getReg());
    auto PseudoSrc = Op1.isReg() && zeno::isPseudoReg(Op1.getReg());

    // MUST OCCUR BEFORE OP CHANGES
    // if going to insert a new MI, need to do it before changing the operands
    if ((PseudoDst || PseudoSrc) && Op0.isReg() && Op1.isReg()) {
      // std::prev(MBBI)->dump();
      // MBBI->dump();
      // dbgs() << "mbbi " << MBBI->getOpcode() << "\n";
      // std::prev(MBBI)->dump();
      // std::next(MBBI)->dump();
      // MBBI->getMF()->dump();
      DebugLoc DL = MBBI->getDebugLoc();
      // insert before old instruction so we dont mess up branches
      BuildMI(MBB, MBBI, DL, TII->get(RISCV::EADDIX),
              zeno::getExtendedReg(Op0.getReg()))
          .addReg(zeno::getExtendedReg(Op1.getReg()))
          .addImm(0);
      Modified |= true;
    }
    if (PseudoDst) {
      Op0.setReg(zeno::getGPRReg(Op0.getReg()));
      Modified |= true;
    }
    if (PseudoSrc) {
      Op1.setReg(zeno::getGPRReg(Op1.getReg()));
      Modified |= true;
    }
  }
  // for all instructions, if there are any remaining pseudo regs, we assume it needs to be lowered to GPR
  for (auto &MO : MBBI->operands()) {
    if (MO.isReg() && zeno::isPseudoReg(MO.getReg())) {
      // for ret and call this is not a warning
      if(!MBBI->isCall() && !MBBI->isReturn()) {
        errs() << "Warning: Potentially invalid pseudo register usage '";
        MBBI->print(errs(), true /*IsStandalone*/, false /*SkipOpers*/,
                    false /*SkipDebugLoc*/, false /*AddNewLine*/, TII);
        errs() << "', lowering to GPR but may lose NS information\n";
      }
      MO.setReg(zeno::getGPRReg(MO.getReg()));
      Modified |= true;
    }
  }
  return Modified;
}

bool RISCVZenoPreEmitFixup::fixupPseudoNSManagement(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator &MBBI, MachineBasicBlock::iterator &PMBBI) {

  DebugLoc DL = MBBI->getDebugLoc();
  if (MBBI->getOpcode() == RISCV::PseudoZenoGetPtr) {
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI),
            zeno::getGPRReg(MBBI->getOperand(0).getReg()))
        .addReg(zeno::getGPRReg(MBBI->getOperand(1).getReg()))
        .addImm(0);
    MBBI->eraseFromParent();
    return true;
  }
  if (MBBI->getOpcode() == RISCV::PseudoZenoSetPtr) {
    assert(MBBI->getOperand(0).getReg() == MBBI->getOperand(1).getReg() &&
           "Src and Dst must be the same for ZenoSetPtr");

    auto NewPtrReg = zeno::getGPRReg(MBBI->getOperand(2).getReg());

    // if we are setting the ptr, and the previous instruction has the same
    // register dest as we have of operand, we can just rewrite
    // a1 = addi a0, 2
    // a0 = PseudoZenoSetPtr pxe10, a1
    // BECOMES
    // a0 = addi a0, 2
    if (PMBBI->getOperand(0).isDef() && zeno::getGPRReg(PMBBI->getOperand(0).getReg()) == NewPtrReg) {
      // rewrite the previous def
      PMBBI->getOperand(0).setReg(zeno::getGPRReg(MBBI->getOperand(0).getReg()));
      // remove PseudoZenoSetPtr
      MBBI->eraseFromParent();
    } else {
      TII->copyPhysReg(MBB, MBBI, DL, MBBI->getOperand(0).getReg(), NewPtrReg, false);
      // BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI),
      //         zeno::getGPRReg(MBBI->getOperand(0).getReg()))
      //     .addReg(NewPtrReg)
      //     .addImm(0);
      MBBI->eraseFromParent();
    }
    return true;
  }

  if (MBBI->getOpcode() == RISCV::PseudoZenoGetNSID) {
    TII->copyPhysReg(MBB, MBBI, DL, MBBI->getOperand(0).getReg(), MBBI->getOperand(1).getReg(), false);
    // BuildMI(MBB, MBBI, DL, TII->get(RISCV::EADDIX),
    //         zeno::getExtendedReg(MBBI->getOperand(0).getReg()))
    //     .addReg(zeno::getExtendedReg(MBBI->getOperand(1).getReg()))
    //     .addImm(0);
    MBBI->eraseFromParent();
    return true;
  }
  if (MBBI->getOpcode() == RISCV::PseudoZenoSetNSID) {
    assert(MBBI->getOperand(0).getReg() == MBBI->getOperand(1).getReg() && "Src and Dst must be the same for ZenoSetNSID");
      TII->copyPhysReg(MBB, MBBI, DL, MBBI->getOperand(0).getReg(), MBBI->getOperand(2).getReg(), false);
    // BuildMI(MBB, MBBI, DL, TII->get(RISCV::EADDIX),
    //         zeno::getExtendedReg(MBBI->getOperand(0).getReg()))
    //     .addReg(zeno::getExtendedReg(MBBI->getOperand(2).getReg()))
    //     .addImm(0);
    MBBI->eraseFromParent();
    return true;
  }
  if (MBBI->getOpcode() == RISCV::PseudoZenoCombinePtrNSID) {
    auto DestGPR = zeno::getGPRReg(MBBI->getOperand(0).getReg());
    auto DestEXT = zeno::getExtendedReg(MBBI->getOperand(0).getReg());

    auto SrcGPR = MBBI->getOperand(1).getReg();
    auto SrcEXT = MBBI->getOperand(2).getReg();
    // if its pseudo, just get gpr
    // if its ext, leave it
    if(zeno::isPseudoReg(SrcGPR)) {
      SrcGPR = zeno::getGPRReg(SrcGPR);
    }
    // if its pseudo, just get ext
    // if its gpr, leave it
    if(zeno::isPseudoReg(SrcEXT)) {
      SrcEXT = zeno::getExtendedReg(SrcEXT);
    }
    TII->copyPhysReg(MBB, MBBI, DL, DestGPR, SrcGPR, false);
    TII->copyPhysReg(MBB, MBBI, DL, DestEXT, SrcEXT, false);

    // BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI), DestGPR)
    //     .addReg(zeno::getGPRReg(MBBI->getOperand(1).getReg()))
    //     .addImm(0);

    // BuildMI(MBB, MBBI, DL, TII->get(RISCV::EADDIX), DestEXT)
    //     .addReg(zeno::getExtendedReg(MBBI->getOperand(2).getReg()))
    //     .addImm(0);
    MBBI->eraseFromParent();
    return true;
  }

  return false;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVZenoPreEmitFixup, DEBUG_TYPE, _PASS_NAME, false, false)

namespace llvm {

FunctionPass *createRISCVZenoPreEmitFixupPass() {
  return new RISCVZenoPreEmitFixup();
}

} // end of namespace llvm

#undef _PASS_NAME
