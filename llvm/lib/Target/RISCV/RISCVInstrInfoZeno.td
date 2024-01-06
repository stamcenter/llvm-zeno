

// def SDTLoad : SDTypeProfile<1, 1, [         // load
//   SDTCisPtrTy<1>
// ]>;

// def SDTStore : SDTypeProfile<0, 2, [        // store
//   SDTCisPtrTy<1>
// ]>;


// def SDTZenoStore : SDTypeProfile<0, 3, [
//   SDTCisPtrTy<1>, SDTCisInt<2>
// ]>;
// def SDTSetExt : SDTypeProfile<0, 1, [
//   SDTCisInt<0>
// ]>;
// def riscv_zeno_set_ext : SDNode<"RISCVISD::ZENO_SET_EXT", SDTSetExt, [SDNPHasChain]>;

// // def ld         : SDNode<"ISD::LOAD"       , SDTLoad,
// //                         [SDNPHasChain, SDNPMayLoad, SDNPMemOperand]>;
// def riscv_zeno_store : SDNode<"RISCVISD::ZENO_STORE", SDTZenoStore,
//                         [SDNPHasChain, SDNPMayStore, SDNPMemOperand]>;


  // def loadptr : PatFrag<(ops node:$ptr), (unindexedload node:$ptr)> {
  //   let IsLoad = true;
  //   let IsNonExtLoad = true;
  //   let MemoryVT = iPTR;
  // }
  //   def loadanyptr : PatFrag<(ops node:$ptr), (unindexedload node:$ptr)> {
  //   let IsLoad = true;
  //   let IsNonExtLoad = true;
  //   let MemoryVT = iPTRAny;
  // }

def loadptr : PatFrag<(ops node:$ptr), (unindexedload node:$ptr)> {
  let IsLoad = true;
  let IsNonExtLoad = true;
  let MemoryVT = i128;
}
def extloadptr  : PatFrag<(ops node:$ptr), (extload node:$ptr)> {
  let IsLoad = true;
  let MemoryVT = i64;
}
def sextloadptr  : PatFrag<(ops node:$ptr), (sextload node:$ptr)> {
  let IsLoad = true;
  let MemoryVT = i64;
}
def zextloadptr  : PatFrag<(ops node:$ptr), (zextload node:$ptr)> {
  let IsLoad = true;
  let MemoryVT = i64;
}


def storeptr : PatFrag<(ops node:$val, node:$ptr), (unindexedstore node:$val, node:$ptr)> {
  let IsStore = true;
  let IsTruncStore = false;
  let MemoryVT = i128;
}


def SDT_RISCVZenoGet : SDTypeProfile<1, 1, [SDTCisInt<0>, SDTCisInt<1>]>;
def SDT_RISCVZenoSet : SDTypeProfile<1, 2, [SDTCisInt<0>, SDTCisInt<1>, SDTCisInt<2>]>;
def SDT_RISCVZenoCombine : SDTypeProfile<1, 2, [SDTCisInt<0>, SDTCisInt<1>, SDTCisInt<2>]>;

def riscv_zeno_get_ptr : SDNode<"RISCVISD::ZENO_GET_PTR", 
                                  SDT_RISCVZenoGet, []>;
def riscv_zeno_set_ptr : SDNode<"RISCVISD::ZENO_SET_PTR", 
                                  SDT_RISCVZenoSet, []>;

def riscv_zeno_get_nsid : SDNode<"RISCVISD::ZENO_GET_NSID", 
                                  SDT_RISCVZenoGet, []>;
def riscv_zeno_set_nsid : SDNode<"RISCVISD::ZENO_SET_NSID", 
                                  SDT_RISCVZenoSet, []>;

def riscv_zeno_combine_ptr_nsid : SDNode<"RISCVISD::ZENO_COMBINE_PTR_NSID", 
                                            SDT_RISCVZenoCombine, []>;

// TODO(jacob-abraham) these could be much cleaner, we dont need the duplicate real instruction

multiclass RV64LoadExtended<bits<3> funct3, string opcodestr, string schedSize, string loadRegClass = "GPR", bit hasConstraint = 0> {

  let hasSideEffects = 0, mayLoad = 1, mayStore = 0 in {

    def ""#NAME
      : RVInstI<funct3, OPC_ELOAD, 
                (outs !cast<RegisterClass>(""#loadRegClass):$rd), 
                (ins GPR:$rs1, simm12:$imm12),
                opcodestr, 
                "$rd, ${imm12}(${rs1})">, 
                  Sched<[!cast<SchedWrite>("WriteLD"#schedSize), ReadMemBase]> {
                    // let Constraints = !if(!eq(hasConstraint, 1), "@earlyclobber $rd", "");
                    // let Constraints = "@earlyclobber $rd";
                  }

    let hasPostISelHook = 1, isAsmParserOnly = 0, isCodeGenOnly = 0, isPseudo = 1 in {
    def Pseudo#NAME 
      : Pseudo<(outs !cast<RegisterClass>(""#loadRegClass):$rd), 
                (ins PXER:$rs1, simm12:$imm12, simm12:$regID), 
                [], 
                opcodestr#"_p", 
                "$rd, ${imm12}(${rs1}), $regID"> {
                  let Constraints = !if(hasConstraint, "@earlyclobber $rd", "");
                  // let Constraints = "@earlyclobber $rd";
                  // let Constraints = !if(!eq(hasConstraint, 1), "$rd != $rs1", "");
                }
    }
  }

  def : InstAlias<opcodestr#" $rd, (${rs1})", 
        (!cast<RVInstI>(""#NAME) 
          !cast<RegisterClass>(""#loadRegClass):$rd, 
          GPR:$rs1, 
          0),
        0>;

}

multiclass RV64StoreExtended<bits<3> funct3, string opcodestr, string schedSize, string storeRegClass = "GPR"> {

  let hasSideEffects = 0, mayLoad = 0, mayStore = 1 in {

    def ""#NAME
      : RVInstS<funct3, OPC_ESTORE, 
                (outs),
                (ins !cast<RegisterClass>(""#storeRegClass):$rs2, GPR:$rs1, simm12:$imm12),
                opcodestr, 
                "$rs2, ${imm12}(${rs1})">, 
                  Sched<[!cast<SchedWrite>("WriteST"#schedSize), ReadStoreData]>;

    let hasPostISelHook = 1, isAsmParserOnly = 0, isCodeGenOnly = 0, isPseudo = 1 in {
    def Pseudo#NAME 
      : Pseudo<(outs), 
                (ins !cast<RegisterClass>(""#storeRegClass):$rs2, PXER:$rs1, simm12:$imm12, simm12:$regID), 
                [], 
                opcodestr#"_p", 
                "$rs2, ${imm12}(${rs1}), $regID">;
    }
  }

  def : InstAlias<opcodestr#" $rs2, (${rs1})", 
        (!cast<RVInstS>(""#NAME) 
          !cast<RegisterClass>(""#storeRegClass):$rs2, 
          GPR:$rs1, 
          0), 
        0>;

}



multiclass ZenoAMI<bits<3> funct3, RISCVOpcode opcode, string opcodestr, string shortopcodestr, string dstRegClass, string srcRegClass> {

  let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {

    def ""#NAME
      : RVInstI<funct3, opcode, 
                (outs !cast<RegisterClass>(""#dstRegClass):$rd),
                (ins !cast<RegisterClass>(""#srcRegClass):$rs1, simm12:$imm12),
                opcodestr, "$rd, $rs1, $imm12">, 
              Sched<[]>;

    let hasSideEffects = 1, hasPostISelHook = 0, isAsmParserOnly = 0, isCodeGenOnly = 0, isPseudo = 1 in {
      // takes dst as input so we can set it
    def Pseudo#NAME 
      : Pseudo<(outs), 
                (ins 
                  !cast<RegisterClass>(""#dstRegClass):$rd, 
                  !cast<RegisterClass>(""#srcRegClass):$rs1, 
                  simm12:$imm12
                ), 
                [], 
                opcodestr#"_p", "$rd, $rs1, $imm12">;
    }
  }

  def : InstAlias<shortopcodestr#" $rd, $rs1", 
        (!cast<RVInstI>(""#NAME)  
          !cast<RegisterClass>(""#dstRegClass):$rd, 
          !cast<RegisterClass>(""#srcRegClass):$rs1, 
          0),
        1>;

}


// let hasSideEffects = 0, mayLoad = 1, mayStore = 0 in
// class RV64LoadExtendedRaw<bits<3> funct3, string opcodestr>
//     : RVInstR<0b1010101, funct3, OPC_OP, (outs GPR:$rd), 
//               (ins GPR:$rs1, ER64:$rs2/*ext2*/),
//               opcodestr, "$rd, $rs1, $rs2">;

// let hasSideEffects = 0, mayLoad = 0, mayStore = 1 in
// class RV64StoreExtendedRaw<bits<3> funct3, string opcodestr>
//     : RVInstR<0b0100010, funct3, OPC_OP, (outs), 
//               (ins GPR:$rd/*rs1*/, GPR:$rs1/*rs2*/, ER64:$rs2/*ext3*/),
//               opcodestr, "$rd, $rs1, $rs2">;


let Predicates = [HasStdExtZZeno] in {
  // define base loads
  defm ELD  : RV64LoadExtended<0b011, "eld",  "D">;
  defm ELP  : RV64LoadExtended<0b011, "eld",  "D", "PXER", 1>; // TODO(jacob-abraham) add 32 bit version

  defm ELW  : RV64LoadExtended<0b010, "elw",  "W">;
  defm ELH  : RV64LoadExtended<0b001, "elh",  "H">;
  defm ELHU : RV64LoadExtended<0b101, "elhu", "H">;
  defm ELB  : RV64LoadExtended<0b000, "elb",  "B">;
  defm ELBU : RV64LoadExtended<0b100, "elbu", "B">;
  defm ELE  : RV64LoadExtended<0b111, "ele",  "D", "ER64">;

  // define base stores
  defm ESD : RV64StoreExtended<0b011, "esd", "D">;
  defm ESP : RV64StoreExtended<0b011, "esd", "D", "PXER">;

  defm ESW : RV64StoreExtended<0b010, "esw", "W">;
  defm ESH : RV64StoreExtended<0b001, "esh", "H">;
  defm ESB : RV64StoreExtended<0b000, "esb", "B">;
  defm ESE : RV64StoreExtended<0b101, "ese", "D", "ER64">;


  defm EADDI : ZenoAMI<0b110, OPC_ESTORE, "eaddi", "movebe", "GPR", "ER64">;
  defm EADDIE : ZenoAMI<0b111, OPC_ESTORE, "eaddie", "moveeb", "ER64", "GPR">;
  defm EADDIX : ZenoAMI<0b111, OPC_LOAD, "eaddix", "moveee", "ER64", "ER64">;


  // raw load
  // def ERLD  : RV64LoadExtendedRaw<0b011, "erld" >, Sched<[WriteLDD, ReadMemBase]>;
  // def ERLW  : RV64LoadExtendedRaw<0b010, "erlw" >, Sched<[WriteLDW, ReadMemBase]>;
  // def ERLH  : RV64LoadExtendedRaw<0b001, "erlh" >, Sched<[WriteLDH, ReadMemBase]>;
  // def ERLHU : RV64LoadExtendedRaw<0b101, "erlhu">, Sched<[WriteLDH, ReadMemBase]>;
  // def ERLB  : RV64LoadExtendedRaw<0b000, "erlb" >, Sched<[WriteLDB, ReadMemBase]>;
  // def ERLBU : RV64LoadExtendedRaw<0b100, "erlbu">, Sched<[WriteLDB, ReadMemBase]>;


  // // raw store
  // def ERSD : RV64StoreExtendedRaw<0b011, "ersd">, Sched<[WriteSTD, ReadStoreData, ReadMemBase]>;
  // def ERSW : RV64StoreExtendedRaw<0b010, "ersw">, Sched<[WriteSTW, ReadStoreData, ReadMemBase]>;
  // def ERSH : RV64StoreExtendedRaw<0b001, "ersh">, Sched<[WriteSTH, ReadStoreData, ReadMemBase]>;
  // def ERSB : RV64StoreExtendedRaw<0b000, "ersb">, Sched<[WriteSTB, ReadStoreData, ReadMemBase]>;


  // TODO(jacob-abraham) make into multiclass
  // def PseudoELP 
  //   : Pseudo<(outs GPR:$rd), 
  //             (ins PXER:$rs1, simm12:$imm12, simm12:$regID), 
  //             [], 
  //             "elp_p", 
  //             "$rd, ${imm12} (${rs1}), $regID"> {
  //               let Constraints = "@earlyclobber $rd"; 

  //               let hasSideEffects = 0;
  //               let mayLoad = 1;
  //               let mayStore = 0;
  //               let hasPostISelHook = 1; 
  //               let isAsmParserOnly = 0;
  //               let isCodeGenOnly = 0;
  //               let isPseudo = 1;
  //             }
  
  // def PseudoESP 
  //   : Pseudo<(outs), 
  //             (ins GPR:$rs2, PXER:$rs1, simm12:$imm12, simm12:$regID), 
  //             [], 
  //             "esp_p", 
  //             "$rs2, ${imm12}(${rs1}), $regID"> {
  //               let hasSideEffects = 0;
  //               let mayLoad = 0;
  //               let mayStore = 1;
  //               let hasPostISelHook = 1; 
  //               let isAsmParserOnly = 0;
  //               let isCodeGenOnly = 0;
  //               let isPseudo = 1;
  //             }
  

  // // address managment instructions
  // let hasSideEffects = /*1*/0, mayLoad = 0, mayStore = 0 in {
  //   def EADDI : 
  //     RVInstI<0b110, 
  //             OPC_ESTORE, 
  //             (outs GPR:$rd), 
  //             (ins ER64:$rs1/*ext1*/, simm12:$imm12), 
  //             "eaddi", "$rd, $rs1, $imm12">, 
  //           Sched<[]>;

  //   def EADDIE : 
  //     RVInstI<0b111, 
  //             OPC_ESTORE, 
  //             (outs ER64:$rd/*extd*/), 
  //             (ins GPR:$rs1, simm12:$imm12), 
  //             "eaddie", "$rd, $rs1, $imm12">,
  //           Sched<[]>;

  //   def EADDIX : 
  //     RVInstI<0b111, 
  //             OPC_LOAD, 
  //             (outs ER64:$rd/*extd*/), 
  //             (ins ER64:$rs1/*ext1*/, simm12:$imm12), 
  //             "eaddix", "$rd, $rs1, $imm12">,
  //           Sched<[]>;
  // }


  // // shorthands for moving between regular and extended registers
  // def MOVEBE : InstAlias<"movebe $rd, $ext1", (EADDI GPR:$rd, ER64:$ext1, 0)>;
  // def MOVEEB : InstAlias<"moveeb $extd, $rs1", (EADDIE ER64:$extd, GPR:$rs1, 0)>;
  // def MOVEEE : InstAlias<"moveee $extd, $ext1", (EADDIX ER64:$extd, ER64:$ext1, 0)>;


  // let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
  //   // takes er as input so we can set it
  //   def PseudoEADDIE : 
  //     Pseudo<
  //             (outs), 
  //             (ins ER64:$rd/*extd*/, GPR:$rs1, simm12:$imm12),
  //             [],
  //             "eaddie_p", "$rd, $rs1, $imm12">;
  // }


  // defines base pattern from 128->64
  def PseudoZenoGetPtr: Pseudo<(outs GPR:$rd), (ins PXER:$val),
            [(set (i64 GPR:$rd), (riscv_zeno_get_ptr (i128 PXER:$val)))]> {
              let isExtractSubreg = true;
            }
  def PseudoZenoSetPtr: Pseudo<(outs PXER:$rd), (ins PXER:$val, GPR:$ptr),
            [(set (i128 PXER:$rd), (riscv_zeno_set_ptr (i128 PXER:$val), (i64 GPR:$ptr)))]> {
              let isInsertSubreg = true;
              // let Constraints = "$rd = $val";
            }

  def PseudoZenoGetNSID: Pseudo<(outs ER64:$rd), (ins PXER:$val),
            [(set (i64 ER64:$rd), (riscv_zeno_get_nsid (i128 PXER:$val)))]> {
              let isExtractSubreg = true;
            }
  def PseudoZenoSetNSID: Pseudo<(outs PXER:$rd), (ins PXER:$val, ER64:$nsid),
            [(set (i64 PXER:$rd), (riscv_zeno_set_nsid (i128 PXER:$val), (i64 ER64:$nsid)))]> {
              let isInsertSubreg = true;
              // let Constraints = "$rd = $val";
            }

  // defines extra pattern from 64->64
  def : Pat<(riscv_zeno_get_ptr (i64 GPR:$val)), (PseudoZenoGetPtr (i64 GPR:$val))>;
  def : Pat<(riscv_zeno_get_nsid (i64 GPR:$val)), (PseudoZenoGetNSID (i64 GPR:$val))>;

  def PseudoZenoCombinePtrNSID: Pseudo<(outs PXER:$rd), (ins GPR:$ptr, ER64:$nsid),
              [(set (i128 PXER:$rd), (riscv_zeno_combine_ptr_nsid (i64 GPR:$ptr), (i64 ER64:$nsid)))]> {
                let isRegSequence = true;
                // this constraint means we do less gpr copying and more nsid copying
                // let Constraints = "$rd = $ptr";
              }


  //need to set addedcomplexity so we override existing load/store patterns
  let AddedComplexity = 1 in {
  multiclass ZenoLdPat<PatFrag LoadOp, RVInst Inst, /*RVInst RawInst,*/ ValueType vt = XLenVT> {
    // def : Pat<(vt (LoadOp BaseAddr:$rs1)), 
    //           (RawInst BaseAddr:$rs1, (EADDIE BaseAddr:$rs1, 0))>;
    def : Pat<(vt (LoadOp BaseAddr:$rs1)), 
              (Inst BaseAddr:$rs1, (i64 0), (i64 -1))>;

    def : Pat<(vt (LoadOp (add BaseAddr:$rs1, simm12:$imm12))),
              (Inst BaseAddr:$rs1, simm12:$imm12, (i64 -1))>;
    
    def : Pat<(vt (LoadOp 
                    (riscv_zeno_combine_ptr_nsid 
                      (i64 (add BaseAddr:$rs1, simm12:$imm12)), 
                      (i64 (BaseAddr:$ext1))))),
              (Inst (PseudoZenoCombinePtrNSID BaseAddr:$rs1, BaseAddr:$ext1), simm12:$imm12, (i64 -1))>;

    def : Pat<(vt (LoadOp (IsOrAdd AddrFI:$rs1, simm12:$imm12))),
              (Inst AddrFI:$rs1, simm12:$imm12, (i64 -1))>;
    
    def : Pat<(vt (LoadOp 
                    (riscv_zeno_combine_ptr_nsid 
                      (i64 (IsOrAdd AddrFI:$rs1, simm12:$imm12)), 
                      (i64 (AddrFI:$ext1))))),
              (Inst (PseudoZenoCombinePtrNSID AddrFI:$rs1, AddrFI:$ext1), simm12:$imm12, (i64 -1))>;
  }

  multiclass ZenoStPat<PatFrag StoreOp, RVInst Inst, /*RVInst RawInst,*/ RegisterClass StTy = GPR, ValueType vt = XLenVT> {
    // def : Pat<(StoreOp (vt StTy:$rs2), BaseAddr:$rs1),
    //           (RawInst StTy:$rs2, BaseAddr:$rs1, (EADDIE BaseAddr:$rs1, 0))>;
    def : Pat<(StoreOp (vt StTy:$rs2), BaseAddr:$rs1),
              (Inst StTy:$rs2, BaseAddr:$rs1, (i64 0), (i64 -1))>;

    def : Pat<(StoreOp (vt StTy:$rs2), (add BaseAddr:$rs1, simm12:$imm12)),
              (Inst StTy:$rs2, BaseAddr:$rs1, simm12:$imm12, (i64 -1))>;

    def : Pat<(StoreOp (vt StTy:$rs2),     
                       (riscv_zeno_combine_ptr_nsid 
                          (i64 (add BaseAddr:$rs1, simm12:$imm12)), 
                          (i64 (BaseAddr:$ext1)))),
              (Inst StTy:$rs2, 
                  (PseudoZenoCombinePtrNSID BaseAddr:$rs1, BaseAddr:$ext1),
                  simm12:$imm12, 
                  (i64 -1))>;

    def : Pat<(StoreOp (vt StTy:$rs2), (IsOrAdd AddrFI:$rs1, simm12:$imm12)),
              (Inst StTy:$rs2, AddrFI:$rs1, simm12:$imm12, (i64 -1))>;
    
    def : Pat<(StoreOp (vt StTy:$rs2),     
                       (riscv_zeno_combine_ptr_nsid 
                          (i64 (IsOrAdd AddrFI:$rs1, simm12:$imm12)), 
                          (i64 (AddrFI:$ext1)))),
              (Inst StTy:$rs2, 
                  (PseudoZenoCombinePtrNSID AddrFI:$rs1, AddrFI:$ext1),
                  simm12:$imm12, 
                  (i64 -1))>;

  }
  }


  

  //arly-clobber renamable $e12 = PseudoZenoGetNSID killed renamable $pxe10                                              
  //early-clobber renamable $x10 = PseudoZenoGetPtr killed renamable $pxe11               
  //renamable $pxe10 = PseudoZenoCombinePtrNSID killed renamable $x10, killed renamable $e12
// def  : SDNodeXForm<imm, [{
//   return CurDAG->getTargetConstant(N->getSExtValue() / 2, SDLoc(N),
//                                    N->getValueType(0));
// }]>;

// def : Pat<(add GPR:$rs1, (AddiPair:$rs2)),
//           (ADDI (ADDI GPR:$rs1, (AddiPairImmB AddiPair:$rs2)),
//                 (AddiPairImmA GPR:$rs2))>;

//   // define pattern to combine Ptr from one val and NSID from another val
//   def : Pat<
//     (riscv_zeno_combine_ptr_nsid 
//       (riscv_zeno_get_ptr (i128 PXER:$val1)), 
//       ((riscv_zeno_get_nsid (i128 PXER:$val2)))
//       ), 
    
//       >;
//   // def : ComplexPattern<(PXER, 2, riscv_zeno_combine_ptr_nsid (i64 GPR:$ptr), (i64 ER64:$nsid))>


  defm : ZenoLdPat<load,        PseudoELD/*,  ERLD*/>;
  defm : ZenoLdPat<sextloadi32, PseudoELW/*,  ERLW*/>;
  defm : ZenoLdPat<extloadi32,  PseudoELW/*,  ERLW*/>;
  defm : ZenoLdPat<zextloadi32, PseudoELW/*,  ERLW*/>; //not the same as LWU

  defm : ZenoLdPat<extloadi16,  PseudoELH/*,  ERLH*/>;
  defm : ZenoLdPat<sextloadi16, PseudoELH/*,  ERLH*/>;
  defm : ZenoLdPat<zextloadi16, PseudoELHU/*, ERLHU*/>;

  defm : ZenoLdPat<extloadi8,   PseudoELB/*,  ERLB */>;
  defm : ZenoLdPat<sextloadi8,  PseudoELB/*,  ERLB */>;
  defm : ZenoLdPat<zextloadi8,  PseudoELBU/*, ERLBU*/>;

  defm : ZenoStPat<store         , PseudoESD/*, ERSD*/, GPR>;
  defm : ZenoStPat<truncstorei32 , PseudoESW/*, ERSW*/, GPR>;
  defm : ZenoStPat<truncstorei16 , PseudoESH/*, ERSH*/, GPR>;
  defm : ZenoStPat<truncstorei8  , PseudoESB/*, ERSB*/, GPR>;


  defm : ZenoLdPat<loadptr, PseudoELP, i128>;
  defm : ZenoLdPat<extloadptr, PseudoELP, i128>;
  defm : ZenoLdPat<sextloadptr, PseudoELP, i128>;
  defm : ZenoLdPat<zextloadptr, PseudoELP, i128>;

  defm : ZenoStPat<storeptr, PseudoESP, PXER, i128>;


  // load address immediate, translates to "(addi dst, (lui hi), lo)"
  // commented out asm parser, need to change these if we readd it
  let hasSideEffects = 0, mayLoad = 1, mayStore = 0, isCodeGenOnly = 1,
    isAsmParserOnly = 0 in
  def PseudoLAI : Pseudo<(outs GPR:$dst), (ins bare_symbol:$hi, bare_symbol:$lo), [], "lai", "$dst, ($hi,$lo)">;

}
