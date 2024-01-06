//===-- RISCVZenoExpandPseudo.cpp - Expand pseudo instructions -----------===//
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
#include "llvm/CodeGen/MachineInstrBuilder.h"

#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-zeno-expand-pseudo"

#define RISCV_ZENO_EXPAND_PSEUDO_NAME                                          \
  "RISCV ZENO pseudo instruction expansion pass"

namespace {

class RISCVZenoExpandPseudo : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVZenoExpandPseudo() : MachineFunctionPass(ID) {
    initializeRISCVZenoExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return RISCV_ZENO_EXPAND_PSEUDO_NAME;
  }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandExtendedLoadStore(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator MBBI);
  bool expandExtendedMov(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);

  bool isExtendedOpcode(unsigned Opcode);

protected:
};

char RISCVZenoExpandPseudo::ID = 0;

bool RISCVZenoExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool RISCVZenoExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool RISCVZenoExpandPseudo::isExtendedOpcode(unsigned Opcode) {
  switch (Opcode) {
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

  case RISCV::PseudoEADDIE:
    return true;
  }
  return false;
}

bool RISCVZenoExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator MBBI,
                                     MachineBasicBlock::iterator &NextMBBI) {
  // RISCVInstrInfo::getInstSizeInBytes hard-codes the number of expanded
  // instructions for each pseudo, and must be updated when adding new pseudos
  // or changing existing ones.

  switch (MBBI->getOpcode()) {
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
    return expandExtendedLoadStore(MBB, MBBI);
  case RISCV::PseudoEADDIE:
    return expandExtendedMov(MBB, MBBI);
  }

  // for(auto i = 0; i < MBBI->getNumOperands(); i++) {
  //
  //   if(Op.isReg() && zeno::isPseudoReg(Op.getReg())) {

  //   }
  // }

  return false;
}

bool RISCVZenoExpandPseudo::expandExtendedLoadStore(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();

  if (!isExtendedOpcode(MBBI->getOpcode()))
    return false;

  LLVM_DEBUG(dbgs() << "EXPAND PSEUDO Extended: "; MBBI->dump());

  if (!(MBBI->getOperand(0).isReg() && MBBI->getOperand(1).isReg())) {
    LLVM_DEBUG(dbgs() << "Not all operands are regs yet, cannot expand yet\n";);
    return false;
  }
  if (MBBI->getNumOperands() == 4 && MBBI->getOperand(3).getImm() >= 0) {
    LLVM_DEBUG(dbgs() << "We have an unselected extended register, this cannot "
                         "be lowered "
                         "properly\n";);
    return false;
  }

  // at this point we should have selected registers
  assert((MBBI->getNumOperands() != 4 || MBBI->getOperand(3).getImm() < 0) &&
         "unselected extended register");

  // ext_regio
  // int64_t ext_regid = -1;
  // if(MBBI->getNumOperands() >= 4)
  //   ext_regid = MBBI->getOperand(3).getImm();

  Register DstGPR = MBBI->getOperand(0).getReg();
  Register SrcPseudo = MBBI->getOperand(1).getReg();
  Register SrcGPR = SrcPseudo;
  if (SrcPseudo.id() >= RISCV::PXE0 && SrcPseudo <= RISCV::PXE31) {
    // convert pseudo to gpr
    SrcGPR = RISCV::X0 + (SrcPseudo.id() - RISCV::PXE0);
  } /*else if (SrcPseudo.id() >= RISCV::X0 && SrcPseudo <= RISCV::X31) {
    // just pass gpr along
    SrcGPR = SrcPseudo;
  }*/

  if (!(SrcGPR.id() >= RISCV::X0 && SrcGPR <= RISCV::X31)) {
    LLVM_DEBUG(dbgs() << "bad machine instruction: "; MBBI->dump();
               dbgs() << "\n";);
    llvm_unreachable("Failed to convert Pseudo Extended Register to GPR");
  }

  bool isPtrLS = MBBI->getOpcode() == RISCV::PseudoESP ||
                 MBBI->getOpcode() == RISCV::PseudoELP ||
                 MBBI->getOpcode() == RISCV::ESP ||
                 MBBI->getOpcode() == RISCV::ELP;

  // if ele or ese, need to change DstGPR from pseudo to ext
  if (MBBI->getOpcode() == RISCV::PseudoELE ||
      MBBI->getOpcode() == RISCV::PseudoESE) {
    if (DstGPR.id() >= RISCV::PXE0 && DstGPR <= RISCV::PXE31) {
      // convert pseudo to ext
      DstGPR = RISCV::E0 + (DstGPR.id() - RISCV::PXE0);
    }
  }
  if (isPtrLS) {
    if (DstGPR.id() >= RISCV::PXE0 && DstGPR <= RISCV::PXE31) {
      // convert pseudo to gpr
      DstGPR = RISCV::X0 + (DstGPR.id() - RISCV::PXE0);
    }
  }
  int64_t Imm = MBBI->getOperand(2).getImm();

  unsigned Opcode = 0;
  bool isLoad = true;

  switch (MBBI->getOpcode()) {
  default:
    LLVM_DEBUG(MBBI->dump(); dbgs() << "\n";);
    llvm_unreachable("Unimplemented pseudo instruction!");
  case RISCV::PseudoELP:
    Opcode = RISCV::ELP;
    break;
  case RISCV::PseudoELD:
    Opcode = RISCV::ELD;
    break;
  case RISCV::PseudoELW:
    Opcode = RISCV::ELW;
    break;
  case RISCV::PseudoELH:
    Opcode = RISCV::ELH;
    break;
  case RISCV::PseudoELHU:
    Opcode = RISCV::ELHU;
    break;
  case RISCV::PseudoELB:
    Opcode = RISCV::ELB;
    break;
  case RISCV::PseudoELBU:
    Opcode = RISCV::ELBU;
    break;
  case RISCV::PseudoELE:
    Opcode = RISCV::ELE;
    break;

  case RISCV::PseudoESP:
    Opcode = RISCV::ESP;
    isLoad = false;
    break;
  case RISCV::PseudoESD:
    Opcode = RISCV::ESD;
    isLoad = false;
    break;
  case RISCV::PseudoESW:
    Opcode = RISCV::ESW;
    isLoad = false;
    break;
  case RISCV::PseudoESH:
    Opcode = RISCV::ESH;
    isLoad = false;
    break;
  case RISCV::PseudoESB:
    Opcode = RISCV::ESB;
    isLoad = false;
    break;
  case RISCV::PseudoESE:
    Opcode = RISCV::ESE;
    isLoad = false;
    break;
  }

  // bool hadToMoveExt = false;
  // int FI = -1;
  // // do we have an assigned reg id
  // if (ext_regid >= 0 && ext_regid <= 31) {
  //   // src reg and ext id do not match, need to moveee
  //   int64_t src_regid = (SrcGPR.id() - RISCV::X0);
  //   if (src_regid != ext_regid) {
  //     Register PrevExt = RISCV::E0 + src_regid;expandExtendedMov
  //     // Prevext = Newext
  //     BuildMI(MBB, MBBI, DL, TII->get(RISCV::EADDIE))
  //         .addReg(PrevExt)
  //         .addReg(NewExt)              // MN->getOperand(4).setImm(-2);

  // TODO(jacob-abraham): roll me back
  if (isLoad) {
    BuildMI(MBB, MBBI, DL, TII->get(Opcode), DstGPR)
        .addReg(SrcGPR /*, RegState::Kill*/)
        .addImm(Imm);
    if (isPtrLS) {
      BuildMI(MBB, MBBI, DL, TII->get(RISCV::ELE),
              RISCV::E0 + (DstGPR.id() - RISCV::X0))
          .addReg(SrcGPR /*, RegState::Kill*/)
          .addImm(Imm + 8);
    }
  } else {
    BuildMI(MBB, MBBI, DL, TII->get(Opcode))
        .addReg(DstGPR)
        .addReg(SrcGPR)
        .addImm(Imm);

    if (isPtrLS) {
      BuildMI(MBB, MBBI, DL, TII->get(RISCV::ESE))
          .addReg(RISCV::E0 + (DstGPR.id() - RISCV ::X0))
          .addReg(SrcGPR)
          .addImm(Imm + 8);
    }
  }

  // if (hadToMoveExt) {
  //   // load the original src_ext
  //   int64_t src_regid = (SrcGPR.id() - RISCV::X0);
  //   Register PrevExt = RISCV::E0 + src_regid;

  //   // fill PrevExt
  //   const MachineFunction *MF = MBB.getParent();
  //   TII->loadRegFromStackSlot(MBB, MBBI, PrevExt, FI, &RISCV::ER64RegClass,
  //                             MF->getSubtarget().getRegisterInfo());
  // }

  MBBI->eraseFromParent();

  return true;
}

bool RISCVZenoExpandPseudo::expandExtendedMov(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();

  Register DstExt = MBBI->getOperand(0).getReg();
  Register SrcGPR = MBBI->getOperand(1).getReg();
  int64_t SrcImm = MBBI->getOperand(2).getImm();

  LLVM_DEBUG(dbgs() << "EXPAND PSEUDO Extended: "; MBBI->dump(););

  unsigned Opcode = 0;
  switch (MBBI->getOpcode()) {
  default:
    LLVM_DEBUG(MBBI->dump(); dbgs() << "\n";);
    llvm_unreachable("Unimplemented pseudo instruction!");
  case RISCV::PseudoEADDIE:
    Opcode = RISCV::EADDIE;
    break;
  }

  BuildMI(MBB, MBBI, DL, TII->get(Opcode), DstExt)
      .addReg(SrcGPR)
      .addImm(SrcImm);

  MBBI->eraseFromParent();

  return true;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVZenoExpandPseudo, "riscv-zeno-expand-pseudo",
                RISCV_ZENO_EXPAND_PSEUDO_NAME, false, false)

namespace llvm {

FunctionPass *createRISCVZenoExpandPseudoPass() {
  return new RISCVZenoExpandPseudo();
}

} // end of namespace llvm
