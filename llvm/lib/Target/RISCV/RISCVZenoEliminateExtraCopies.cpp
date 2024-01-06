//===-- RISCVZenoEliminateExtraCopies.cpp - Debug Zeno instructions -----------===//
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

#define DEBUG_TYPE "riscv-zeno-elim-copies"

#define _PASS_NAME "RISCV Zeno Eliminate Extra Copies pass"

namespace {

class RISCVZenoEliminateExtraCopies : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  const MachineRegisterInfo* MRI;
  static char ID;

  RISCVZenoEliminateExtraCopies() : MachineFunctionPass(ID) {
    initializeRISCVZenoEliminateExtraCopiesPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
  bool runOnMBB(MachineBasicBlock &MBB);
  bool runOnMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
               MachineBasicBlock::iterator &NextMBBI);

  bool runOnRegisters(MachineFunction &MF);

  StringRef getPassName() const override { return _PASS_NAME; }
};

char RISCVZenoEliminateExtraCopies::ID = 0;

bool RISCVZenoEliminateExtraCopies::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  MRI = &MF.getRegInfo();
  assert(MRI->isSSA() && "Function must be SSA form for pass to work");

  bool Modified = false;

  Modified |= runOnRegisters(MF);

  for (auto &MBB : MF)
    Modified |= runOnMBB(MBB);
  return Modified;
}

bool RISCVZenoEliminateExtraCopies::runOnMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= runOnMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool RISCVZenoEliminateExtraCopies::runOnMI(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MBBI,
                                MachineBasicBlock::iterator &NextMBBI) {
  bool Modified = false;

  MachineInstr& MI = *MBBI;

  return Modified;
}


// returns the machine instruction that defines the register, or nullptr if the opcode didnt match
static MachineInstr* isDefinedByOpcode(const Register& Reg, const MachineRegisterInfo& MRI, unsigned int Opcode) {
    // should be only one DefMI or zero DefMI
    auto DefMIs = MRI.def_instructions(Reg);
    if(DefMIs.empty()) return nullptr;
    assert(std::next(DefMIs.begin()) == DefMIs.end() && "Should be exactly one def of register in SSA form");
    auto& DefMI = *DefMIs.begin();
    return DefMI.getOpcode() == Opcode ? &(*DefMIs.begin()) : nullptr;
}

static MachineInstr* isDefinedByOpcode(const MachineOperand& Operand, const MachineRegisterInfo& MRI, unsigned int Opcode) {
  assert(Operand.isReg() && "Operand is not a register");
  return isDefinedByOpcode(Operand.getReg(), MRI, Opcode);
}

bool RISCVZenoEliminateExtraCopies::runOnRegisters(MachineFunction &MF) {

  bool Modified = false;


  // %2:gpr = PseudoZenoGetPtr %0:pxer
  //   %4:pxer = COPY %2:gpr
  // if virt reg is defined by PseudoZenoGetPtr and its used by a COPY to a pxer, remove the copy and replace all uses of the copied value with the virt reg
  for (unsigned Idx = 0; Idx < MRI->getNumVirtRegs(); ++Idx) {
    Register Reg = Register::index2VirtReg(Idx);

    // should be only one DefMI, but loop through all anyways
    for(MachineInstr &DefMI : MRI->def_instructions(Reg)) {
      if(DefMI.getOpcode() == RISCV::PseudoZenoGetPtr) {
        Register OriginalPtrReg = DefMI.getOperand(1).getReg();
        // Reg is defined by a PseudoZenoGetPtr, lets find the uses that are COPY
        for (MachineInstr &UseMI : MRI->use_instructions(Reg)) {
          if(UseMI.getOpcode() == RISCV::COPY) {
            //now need to find all uses of the copy def and replace them with the original reg
            MachineOperand &CopyDef = UseMI.getOperand(0);
            for (MachineOperand &UseOfCopyDef : MRI->use_operands(CopyDef.getReg())) {
              // we can only replace a virtual reg
              if(UseOfCopyDef.getReg().isVirtual()) {
                UseOfCopyDef.setReg(OriginalPtrReg);
                Modified |= true;
              }
            }
          }
        }
      }
    }
  }

  // now we will do this again, checking for the following pattern
  // %1:er64 = PseudoZenoGetNSID %0:pxer
  // %2:gpr = PseudoZenoGetPtr %0:pxer
  // %3:pxer = PseudoZenoCombinePtrNSID killed %2:gpr, killed %1:er64
  // if %3 is defined by PseudoZenoCombinePtrNSID and its two operands are defined by PseudoZenoGetNSID and PseudoZenoGetPtr
  for (unsigned Idx = 0; Idx < MRI->getNumVirtRegs(); ++Idx) {
    Register Reg = Register::index2VirtReg(Idx);


    auto* DefMI = isDefinedByOpcode(Reg, *MRI, RISCV::PseudoZenoCombinePtrNSID);
    if(!DefMI) continue;

    auto& GPROp = DefMI->getOperand(1);
    auto& EROp = DefMI->getOperand(2);

    // if gpr is defined by PseudoZenoGetPtr and has one use
    auto GPRDefMI = isDefinedByOpcode(GPROp, *MRI, RISCV::PseudoZenoGetPtr);
    if(!(GPROp.isReg() && GPRDefMI && std::next(MRI->use_begin(GPROp.getReg())) == MRI->use_end())) continue;

    // if er is defined by PseudoZenoGetNSID and has one use
    auto ERDefMI = isDefinedByOpcode(EROp, *MRI, RISCV::PseudoZenoGetNSID);
    if(!(EROp.isReg() && ERDefMI && std::next(MRI->use_begin(EROp.getReg())) == MRI->use_end())) continue;

    auto& OrigFromGPR = GPRDefMI->getOperand(1);
    auto& OrigFromER = ERDefMI->getOperand(1);

    if(!(OrigFromGPR.isReg() && OrigFromER.isReg() && OrigFromGPR.getReg() == OrigFromER.getReg())) continue;

    Register Orig = OrigFromGPR.getReg();
    // since all of the above are true, we can set all Uses of Reg to be the original pointer
    for(auto& UseMO: MRI->use_operands(Reg)) {
      UseMO.setReg(Orig);
      Modified |= true;
    }
    
  }

  // check for double copies and remove them
  for (unsigned Idx = 0; Idx < MRI->getNumVirtRegs(); ++Idx) {
    Register Reg = Register::index2VirtReg(Idx);

    // find the instruction that defines it
    auto* DefMI = isDefinedByOpcode(Reg, *MRI, RISCV::COPY);
    if(!DefMI) continue;
    if(DefMI->getNumOperands() < 2) continue;
    if(!DefMI->getOperand(1).isReg()) continue;
    Register SrcReg = DefMI->getOperand(1).getReg();
    
    // for all uses of the original DefMI, replace
    for(auto& Use: MRI->use_operands(Reg)) {
      MachineInstr& UseMI = *Use.getParent();
      if (Use.getReg().isVirtual() && SrcReg.isVirtual() && !RISCV::PXERRegClass.contains(Use.getReg(), SrcReg) && UseMI.getOpcode() != RISCV::COPY) {
        Use.setReg(SrcReg);
        Modified |= true;
      }
    }
  }


  return Modified;
}

// liveins: $pxe10, $x11
// %1:gpr = COPY $x11
// %0:pxer = COPY $pxe10
// %6:gpr = COPY %0.sub_ext_64:pxer
// %4:er64 = COPY %1:gpr
// %3:pxer = PseudoZenoCombinePtrNSID %6:gpr, %4:er64
// %5:gpr = PseudoELBU killed %3:pxer, 0, -1 :: (load (s8) from %ir.x)
// $x10 = COPY %5:gpr
// PseudoRET implicit $x10

} // end of anonymous namespace

INITIALIZE_PASS(RISCVZenoEliminateExtraCopies, DEBUG_TYPE, _PASS_NAME, false, false)

namespace llvm {

FunctionPass *createRISCVZenoEliminateExtraCopiesPass() { return new RISCVZenoEliminateExtraCopies(); }

} // end of namespace llvm

#undef _PASS_NAME
