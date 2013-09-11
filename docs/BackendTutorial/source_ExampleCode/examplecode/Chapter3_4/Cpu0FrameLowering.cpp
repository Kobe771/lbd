//===-- Cpu0FrameLowering.cpp - Cpu0 Frame Information --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Cpu0 implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "Cpu0FrameLowering.h"
#include "Cpu0AnalyzeImmediate.h"
#include "Cpu0InstrInfo.h"
#include "Cpu0MachineFunction.h"
#include "llvm/IR/Function.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

//- emitPrologue() and emitEpilogue must exist for main(). 

//===----------------------------------------------------------------------===//
//
// Stack Frame Processing methods
// +----------------------------+
//
// The stack is allocated decrementing the stack pointer on
// the first instruction of a function prologue. Once decremented,
// all stack references are done thought a positive offset
// from the stack/frame pointer, so the stack is considering
// to grow up! Otherwise terrible hacks would have to be made
// to get this stack ABI compliant :)
//
//  The stack frame required by the ABI (after call):
//  Offset
//
//  0                 ----------
//  4                 Args to pass
//  .                 saved $GP  (used in PIC)
//  .                 Alloca allocations
//  .                 Local Area
//  .                 CPU "Callee Saved" Registers
//  .                 saved FP
//  .                 saved RA
//  .                 FPU "Callee Saved" Registers
//  StackSize         -----------
//
// Offset - offset from sp after stack allocation on function prologue
//
// The sp is the stack pointer subtracted/added from the stack size
// at the Prologue/Epilogue
//
// References to the previous stack (to obtain arguments) are done
// with offsets that exceeds the stack size: (stacksize+(4*(num_arg-1))
//
// Examples:
// - reference to the actual stack frame
//   for any local area var there is smt like : FI >= 0, StackOffset: 4
//     st REGX, 4(SP)
//
// - reference to previous stack frame
//   suppose there's a load to the 5th arguments : FI < 0, StackOffset: 16.
//   The emitted instruction will be something like:
//     ld REGX, 16+StackSize(SP)
//
// Since the total stack size is unknown on LowerFormalArguments, all
// stack references (ObjectOffset) created to reference the function
// arguments, are negative numbers. This way, on eliminateFrameIndex it's
// possible to detect those references and the offsets are adjusted to
// their real location.
//
//===----------------------------------------------------------------------===//

// Cpu0FrameLowering::hasFP()
//- Must have, hasFP() is pure virtual of parent
// hasFP - Return true if the specified function should have a dedicated frame
// pointer register.  This is true if the function has variable sized allocas or
// if frame pointer elimination is disabled.
bool Cpu0FrameLowering::hasFP(const MachineFunction &MF) const {
  const MachineFrameInfo *MFI = MF.getFrameInfo();
  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
      MFI->hasVarSizedObjects() || MFI->isFrameAddressTaken();
}

// expandLargeImm()
// Build an instruction sequence to load an immediate that is too large to fit
// in 16-bit and add the result to Reg.
static void expandLargeImm(unsigned Reg, int64_t Imm, 
                           const Cpu0InstrInfo &TII, MachineBasicBlock& MBB,
                           MachineBasicBlock::iterator II, DebugLoc DL) {
  unsigned LUi = Cpu0::LUi;
  unsigned ADDu = Cpu0::ADDu;
  unsigned ZEROReg = Cpu0::ZERO;
  unsigned ATReg = Cpu0::AT;
  Cpu0AnalyzeImmediate AnalyzeImm;
  const Cpu0AnalyzeImmediate::InstSeq &Seq =
    AnalyzeImm.Analyze(Imm, 32, false /* LastInstrIsADDiu */);
  Cpu0AnalyzeImmediate::InstSeq::const_iterator Inst = Seq.begin();

  // The first instruction can be a LUi, which is different from other
  // instructions (ADDiu, ORI and SLL) in that it does not have a register
  // operand.
  if (Inst->Opc == LUi)
    BuildMI(MBB, II, DL, TII.get(LUi), ATReg)
      .addImm(SignExtend64<16>(Inst->ImmOpnd));
  else
    BuildMI(MBB, II, DL, TII.get(Inst->Opc), ATReg).addReg(ZEROReg)
      .addImm(SignExtend64<16>(Inst->ImmOpnd));

  // Build the remaining instructions in Seq.
  for (++Inst; Inst != Seq.end(); ++Inst)
    BuildMI(MBB, II, DL, TII.get(Inst->Opc), ATReg).addReg(ATReg)
      .addImm(SignExtend64<16>(Inst->ImmOpnd));

  BuildMI(MBB, II, DL, TII.get(ADDu), Reg).addReg(Reg).addReg(ATReg);
}

// Cpu0FrameLowering::emitPrologue()
void Cpu0FrameLowering::emitPrologue(MachineFunction &MF) const {
  MachineBasicBlock &MBB   = MF.front();
  MachineFrameInfo *MFI    = MF.getFrameInfo();
  Cpu0FunctionInfo *Cpu0FI = MF.getInfo<Cpu0FunctionInfo>();
  const Cpu0InstrInfo &TII =
    *static_cast<const Cpu0InstrInfo*>(MF.getTarget().getInstrInfo());
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc dl = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  unsigned SP = Cpu0::SP;
  unsigned ADDiu = Cpu0::ADDiu;
  // First, compute final stack size.
  unsigned StackAlign = getStackAlignment();
  unsigned LocalVarAreaOffset = Cpu0FI->getMaxCallFrameSize();
  uint64_t StackSize =  RoundUpToAlignment(LocalVarAreaOffset, StackAlign) +
     RoundUpToAlignment(MFI->getStackSize(), StackAlign);
StackSize = 0x90008000;
   // Update stack size
  MFI->setStackSize(StackSize);

  // No need to allocate space on the stack.
  if (StackSize == 0 && !MFI->adjustsStack()) return;

  MachineModuleInfo &MMI = MF.getMMI();
  std::vector<MachineMove> &Moves = MMI.getFrameMoves();
  MachineLocation DstML, SrcML;

  // Adjust stack.
  if (isInt<16>(-StackSize)) // addiu sp, sp, (-stacksize)
    BuildMI(MBB, MBBI, dl, TII.get(ADDiu), SP).addReg(SP).addImm(-StackSize);
  else { // Expand immediate that doesn't fit in 16-bit.
    Cpu0FI->setEmitNOAT();
    expandLargeImm(SP, -StackSize, TII, MBB, MBBI, dl);
  }

  // emit ".cfi_def_cfa_offset StackSize"
  MCSymbol *AdjustSPLabel = MMI.getContext().CreateTempSymbol();
  BuildMI(MBB, MBBI, dl,
          TII.get(TargetOpcode::PROLOG_LABEL)).addSym(AdjustSPLabel);
  DstML = MachineLocation(MachineLocation::VirtualFP);
  SrcML = MachineLocation(MachineLocation::VirtualFP, -StackSize);
  Moves.push_back(MachineMove(AdjustSPLabel, DstML, SrcML));

  const std::vector<CalleeSavedInfo> &CSI = MFI->getCalleeSavedInfo();

  if (CSI.size()) {
    // Find the instruction past the last instruction that saves a callee-saved
    // register to the stack.
    for (unsigned i = 0; i < CSI.size(); ++i)
      ++MBBI;

    // Iterate over list of callee-saved registers and emit .cfi_offset
    // directives.
    MCSymbol *CSLabel = MMI.getContext().CreateTempSymbol();
    BuildMI(MBB, MBBI, dl,
            TII.get(TargetOpcode::PROLOG_LABEL)).addSym(CSLabel);

    for (std::vector<CalleeSavedInfo>::const_iterator I = CSI.begin(),
           E = CSI.end(); I != E; ++I) {
      int64_t Offset = MFI->getObjectOffset(I->getFrameIdx());
      unsigned Reg = I->getReg();
      {
        // Reg is either in CPURegs or FGR32.
        DstML = MachineLocation(MachineLocation::VirtualFP, Offset);
        SrcML = MachineLocation(Reg);
        Moves.push_back(MachineMove(CSLabel, DstML, SrcML));
      }
    }
  }
}

// Cpu0FrameLowering::emitEpilogue()
void Cpu0FrameLowering::emitEpilogue(MachineFunction &MF,
                                 MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  MachineFrameInfo *MFI            = MF.getFrameInfo();
  Cpu0FunctionInfo *Cpu0FI = MF.getInfo<Cpu0FunctionInfo>();
  const Cpu0InstrInfo &TII =
    *static_cast<const Cpu0InstrInfo*>(MF.getTarget().getInstrInfo());
  DebugLoc dl = MBBI->getDebugLoc();
  unsigned SP = Cpu0::SP;
  unsigned ADDiu = Cpu0::ADDiu;

  // Get the number of bytes from FrameInfo
  uint64_t StackSize = MFI->getStackSize();

StackSize = 0x90008000;
  if (!StackSize)
    return;

  // Adjust stack.
  if (isInt<16>(StackSize)) // addiu sp, sp, (stacksize)
    BuildMI(MBB, MBBI, dl, TII.get(ADDiu), SP).addReg(SP).addImm(StackSize);
  else { // Expand immediate that doesn't fit in 16-bit.
    Cpu0FI->setEmitNOAT();
    expandLargeImm(SP, StackSize, TII, MBB, MBBI, dl);
  }
}

// Cpu0FrameLowering::processFunctionBeforeCalleeSavedScan()
// This method is called immediately before PrologEpilogInserter scans the 
//  physical registers used to determine what callee saved registers should be 
//  spilled. This method is optional. 
// Without this will have following errors,
//  Target didn't implement TargetInstrInfo::storeRegToStackSlot!
//  UNREACHABLE executed at /usr/local/llvm/3.1.test/cpu0/1/src/include/llvm/
//  Target/TargetInstrInfo.h:390!
//  Stack dump:
//  0.	Program arguments: /usr/local/llvm/3.1.test/cpu0/1/cmake_debug_build/
//  bin/llc -march=cpu0 -relocation-model=pic -filetype=asm ch0.bc -o 
//  ch0.cpu0.s
//  1.	Running pass 'Function Pass Manager' on module 'ch0.bc'.
//  2.	Running pass 'Prologue/Epilogue Insertion & Frame Finalization' on 
//      function '@main'
//  Aborted (core dumped)

// Must exist
//	addiu	$sp, $sp, 8
//->	ret	$lr
//	.set	macro
//	.set	reorder
//	.end	main
void Cpu0FrameLowering::
processFunctionBeforeCalleeSavedScan(MachineFunction &MF,
                                     RegScavenger *RS) const {
  MachineRegisterInfo& MRI = MF.getRegInfo();

  // FIXME: remove this code if register allocator can correctly mark
  //        $fp and $ra used or unused.

  // The register allocator might determine $ra is used after seeing
  // instruction "jr $ra", but we do not want PrologEpilogInserter to insert
  // instructions to save/restore $ra unless there is a function call.
  // To correct this, $ra is explicitly marked unused if there is no
  // function call.
  if (MF.getFrameInfo()->hasCalls())
    MRI.setPhysRegUsed(Cpu0::LR);
  else {
    MRI.setPhysRegUnused(Cpu0::LR);
  }
}

