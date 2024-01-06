//===-- RISCVZenoHelper.h - Zeno helper functions -----------===//
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

#ifndef LLVM_LIB_TARGET_RISCV_RISCVZENOHELPER_H
#define LLVM_LIB_TARGET_RISCV_RISCVZENOHELPER_H

#include "llvm/CodeGen/MachineInstr.h"

namespace zeno {
__attribute__((unused)) static bool isPseudoReg(const llvm::Register &Reg) {
  return Reg.id() >= llvm::RISCV::PXE0 && Reg.id() <= llvm::RISCV::PXE31;
}
__attribute__((unused)) static bool isGPRReg(const llvm::Register &Reg) {
  return Reg.id() >= llvm::RISCV::X0 && Reg.id() <= llvm::RISCV::X31;
}
__attribute__((unused)) static bool isExtendedReg(const llvm::Register &Reg) {
  return Reg.id() >= llvm::RISCV::E0 && Reg.id() <= llvm::RISCV::E31;
}
__attribute__((unused)) static llvm::Register
getPseudoReg(const llvm::Register &Reg) {
  unsigned base;
  if (isPseudoReg(Reg))
    base = llvm::RISCV::PXE0;
  else if (isGPRReg(Reg))
    base = llvm::RISCV::X0;
  else if (isExtendedReg(Reg))
    base = llvm::RISCV::E0;
  else
    llvm_unreachable("Could not convert register");
  return llvm::RISCV::PXE0 + (Reg.id() - base);
}
__attribute__((unused)) static llvm::Register
getGPRReg(const llvm::Register &Reg) {
  unsigned base;
  if (isPseudoReg(Reg))
    base = llvm::RISCV::PXE0;
  else if (isGPRReg(Reg))
    base = llvm::RISCV::X0;
  else if (isExtendedReg(Reg))
    base = llvm::RISCV::E0;
  else
    llvm_unreachable("Could not convert register");
  return llvm::RISCV::X0 + (Reg.id() - base);
}
__attribute__((unused)) static llvm::Register
getExtendedReg(const llvm::Register &Reg) {
  unsigned base;
  if (isPseudoReg(Reg))
    base = llvm::RISCV::PXE0;
  else if (isGPRReg(Reg))
    base = llvm::RISCV::X0;
  else if (isExtendedReg(Reg))
    base = llvm::RISCV::E0;
  else
    llvm_unreachable("Could not convert register");
  return llvm::RISCV::E0 + (Reg.id() - base);
}
__attribute__((unused)) static bool
containsPseudoReg(const llvm::MachineInstr &MI) {
  for (const auto &MO : MI.operands()) {
    if (MO.isReg() && isPseudoReg(MO.getReg()))
      return true;
  }
  return false;
}

} // namespace zeno

#endif
