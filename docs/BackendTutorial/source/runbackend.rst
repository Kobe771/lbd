.. _sec-runbackend:

Run backend
===========

This chapter will add LLVM AsmParser support first. 
With AsmParser support, we can hand code the assembly language in C/C++ file 
and translate it into obj (elf format). 
We can write a C++ main 
function as well as the boot code by assembly hand code, and translate this 
main()+bootcode() into obj file.
Combined with llvm-objdump support in last chapter, 
this main()+bootcode() elf can be translated into hex file format which 
include the disassemble code as comment. 
Furthermore, we can design the Cpu0 with Verilog language tool and run the Cpu0 
backend on PC by feed the hex file and see the Cpu0 instructions execution 
result.


AsmParser support
------------------

Run Chapter9_1/ with ch10_1.cpp will get the following error message.

.. rubric:: LLVMBackendTutorialExampleCode/InputFiles/ch10_1.cpp
.. literalinclude:: ../LLVMBackendTutorialExampleCode/InputFiles/ch10_1.cpp
    :lines: 4-
    :linenos:

.. code-block:: bash

  JonathantekiiMac:InputFiles Jonathan$ clang -c ch10_1.cpp -emit-llvm -o 
  ch10_1.bc
  JonathantekiiMac:InputFiles Jonathan$ /Users/Jonathan/llvm/test/cmake_debug_
  build/bin/Debug/llc -march=cpu0 -relocation-model=pic -filetype=obj ch10_1.bc 
  -o ch10_1.cpu0.o
  LLVM ERROR: Inline asm not supported by this streamer because we don't have 
  an asm parser for this target
  
Since we didn't implement cpu0 assembly, it has the error message as above. 
The cpu0 can translate LLVM IR into assembly and obj directly, but it cannot 
translate hand code assembly into obj. 
Directory AsmParser handle the assembly to obj translation.
The Chapter10_1/ include AsmParser implementation as follows,

.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/AsmParser/Cpu0AsmParser.cpp
.. literalinclude:: ../LLVMBackendTutorialExampleCode/Chapter10_1/AsmParser/Cpu0AsmParser.cpp
    :linenos:

.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/AsmParser/CMakeLists.txt
.. literalinclude:: ../LLVMBackendTutorialExampleCode/Chapter10_1/AsmParser/CMakeLists.txt
    :linenos:

.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/AsmParser/LLVMBuild.txt
.. literalinclude:: ../LLVMBackendTutorialExampleCode/Chapter10_1/AsmParser/LLVMBuild.txt
    :linenos:


The Cpu0AsmParser.cpp contains one thousand of code which do the assembly 
language parsing. You can understand it with a little patient only.
To let directory AsmParser be built, modify CMakeLists.txt and LLVMBuild.txt as 
follows,

.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/CMakeLists.txt
.. code-block:: c++

  tablegen(LLVM Cpu0GenAsmMatcher.inc -gen-asm-matcher)
  ...
  add_subdirectory(AsmParser)
  
.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/LLVMBuild.txt
.. code-block:: c++

  subdirectories = AsmParser ...
  ...
  has_asmparser = 1
  
  
The other files change as follows,

.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/MCTargetDesc/Cpu0MCCodeEmitter.cpp
.. code-block:: c++

  unsigned Cpu0MCCodeEmitter::
  getBranchTargetOpValue(const MCInst &MI, unsigned OpNo,
               SmallVectorImpl<MCFixup> &Fixups) const {
    ...
    // If the destination is an immediate, we have nothing to do.
    if (MO.isImm()) return MO.getImm();
    ...
  }
  
  /// getJumpAbsoluteTargetOpValue - Return binary encoding of the jump
  /// target operand. Such as SWI.
  unsigned Cpu0MCCodeEmitter::
  getJumpAbsoluteTargetOpValue(const MCInst &MI, unsigned OpNo,
             SmallVectorImpl<MCFixup> &Fixups) const {
    ...
    // If the destination is an immediate, we have nothing to do.
    if (MO.isImm()) return MO.getImm();
    ...
  }
  
.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/Cpu0.td
.. code-block:: c++

  def Cpu0AsmParser : AsmParser {
    let ShouldEmitMatchRegisterName = 0;
  }
  
  def Cpu0AsmParserVariant : AsmParserVariant {
    int Variant = 0;
  
    // Recognize hard coded registers.
    string RegisterPrefix = "$";
  }
  
  def Cpu0 : Target {
    ...
    let AssemblyParsers = [Cpu0AsmParser];
    ...
    let AssemblyParserVariants = [Cpu0AsmParserVariant];
  }
  
.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/Cpu0InstrFormats.td
.. code-block:: c++

  // Pseudo-instructions for alternate assembly syntax (never used by codegen).
  // These are aliases that require C++ handling to convert to the target
  // instruction, while InstAliases can be handled directly by tblgen.
  class Cpu0AsmPseudoInst<dag outs, dag ins, string asmstr>:
    Cpu0Inst<outs, ins, asmstr, [], IIPseudo, Pseudo> {
    let isPseudo = 1;
    let Pattern = [];
  }
  
.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/Cpu0InstrInfo.td
.. code-block:: c++

  // Cpu0InstrInfo.td
  def Cpu0MemAsmOperand : AsmOperandClass {
    let Name = "Mem";
    let ParserMethod = "parseMemOperand";
  }
  
  // Address operand
  def mem : Operand<i32> {
    ...
    let ParserMatchClass = Cpu0MemAsmOperand;
  }
  ...
  class CmpInstr<...
     !strconcat(instr_asm, "\t$rc, $ra, $rb"), [], itin> {
    ...
  }
  ...
  class CBranch<...
         !strconcat(instr_asm, "\t$ra, $addr"), ...> {
    ...
  }
  ...
  //===----------------------------------------------------------------------===//
  // Pseudo Instruction definition
  //===----------------------------------------------------------------------===//
  
  class LoadImm32< string instr_asm, Operand Od, RegisterClass RC> :
    Cpu0AsmPseudoInst<(outs RC:$ra), (ins Od:$imm32),
             !strconcat(instr_asm, "\t$ra, $imm32")> ;
  def LoadImm32Reg : LoadImm32<"li", shamt, CPURegs>;
  
  class LoadAddress<string instr_asm, Operand MemOpnd, RegisterClass RC> :
    Cpu0AsmPseudoInst<(outs RC:$ra), (ins MemOpnd:$addr),
             !strconcat(instr_asm, "\t$ra, $addr")> ;
  def LoadAddr32Reg : LoadAddress<"la", mem, CPURegs>;
  
  class LoadAddressImm<string instr_asm, Operand Od, RegisterClass RC> :
    Cpu0AsmPseudoInst<(outs RC:$ra), (ins Od:$imm32),
             !strconcat(instr_asm, "\t$ra, $imm32")> ;
  def LoadAddr32Imm : LoadAddressImm<"la", shamt, CPURegs>;


Above define the **ParserMethod = "parseMemOperand"** and implement the 
parseMemOperand() in Cpu0AsmParser.cpp to handle the **"mem"** operand which 
used in ld and st. For example, ld $2, 4($sp), the **mem** operand is 4($sp). 
Accompany with **"let ParserMatchClass = Cpu0MemAsmOperand;"**, 
LLVM will call parseMemOperand() of Cpu0AsmParser.cpp when it meets the assembly 
**mem** operand 4($sp). With above **"let"** assignment, TableGen will generate 
the following structure and functions in Cpu0GenAsmMatcher.inc.

.. rubric:: cmake_debug_build/lib/Target/Cpu0/Cpu0GenAsmMatcher.inc
.. code-block:: c++
  
    enum OperandMatchResultTy {
      MatchOperand_Success,    // operand matched successfully
      MatchOperand_NoMatch,    // operand did not match
      MatchOperand_ParseFail   // operand matched but had errors
    };
    OperandMatchResultTy MatchOperandParserImpl(
      SmallVectorImpl<MCParsedAsmOperand*> &Operands,
      StringRef Mnemonic);
    OperandMatchResultTy tryCustomParseOperand(
      SmallVectorImpl<MCParsedAsmOperand*> &Operands,
      unsigned MCK);
  
  Cpu0AsmParser::OperandMatchResultTy Cpu0AsmParser::
  tryCustomParseOperand(SmallVectorImpl<MCParsedAsmOperand*> &Operands,
              unsigned MCK) {
  
    switch(MCK) {
    case MCK_Mem:
      return parseMemOperand(Operands);
    default:
      return MatchOperand_NoMatch;
    }
    return MatchOperand_NoMatch;
  }
  
  Cpu0AsmParser::OperandMatchResultTy Cpu0AsmParser::
  MatchOperandParserImpl(SmallVectorImpl<MCParsedAsmOperand*> &Operands,
               StringRef Mnemonic) {
    ...
  }
  
  /// MatchClassKind - The kinds of classes which participate in
  /// instruction matching.
  enum MatchClassKind {
    ...
    MCK_Mem, // user defined class 'Cpu0MemAsmOperand'
    ...
  };


Above 3 Pseudo Instruction definitions in Cpu0InstrInfo.td such as 
LoadImm32Reg are handled by Cpu0AsmParser.cpp as follows,

.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/AsmParser/Cpu0AsmParser.cpp
.. code-block:: c++
  
  bool Cpu0AsmParser::needsExpansion(MCInst &Inst) {
  
    switch(Inst.getOpcode()) {
    case Cpu0::LoadImm32Reg:
    case Cpu0::LoadAddr32Imm:
    case Cpu0::LoadAddr32Reg:
      return true;
    default:
      return false;
    }
  }
  
  void Cpu0AsmParser::expandInstruction(MCInst &Inst, SMLoc IDLoc,
              SmallVectorImpl<MCInst> &Instructions){
    switch(Inst.getOpcode()) {
    case Cpu0::LoadImm32Reg:
      return expandLoadImm(Inst, IDLoc, Instructions);
    case Cpu0::LoadAddr32Imm:
      return expandLoadAddressImm(Inst,IDLoc,Instructions);
    case Cpu0::LoadAddr32Reg:
      return expandLoadAddressReg(Inst,IDLoc,Instructions);
    }
  }
  
  bool Cpu0AsmParser::
  MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
              SmallVectorImpl<MCParsedAsmOperand*> &Operands,
              MCStreamer &Out, unsigned &ErrorInfo,
              bool MatchingInlineAsm) {
    MCInst Inst;
    unsigned MatchResult = MatchInstructionImpl(Operands, Inst, ErrorInfo,
                          MatchingInlineAsm);
  
    switch (MatchResult) {
    default: break;
    case Match_Success: {
    if (needsExpansion(Inst)) {
      SmallVector<MCInst, 4> Instructions;
      expandInstruction(Inst, IDLoc, Instructions);
      ...
    }
    ...
  }


Finally, remind the CPURegs as below must 
follow the order of register number because AsmParser use this when do register 
number encode.

.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_1/Cpu0RegisterInfo.td
.. literalinclude:: ../LLVMBackendTutorialExampleCode/Chapter10_1/Cpu0RegisterInfo.td
    :start-after: //  Registers
    :end-before: // Hi/Lo Registers
    :linenos:


Run Chapter10_1/ with ch10_1.cpp to get the correct result as follows,

.. code-block:: bash

  JonathantekiiMac:InputFiles Jonathan$ /Users/Jonathan/llvm/test/cmake_debug_
  build/bin/Debug/llc -march=cpu0 -relocation-model=pic -filetype=obj ch10_1.bc -o 
  ch10_1.cpu0.o
  JonathantekiiMac:InputFiles Jonathan$ /Users/Jonathan/llvm/test/cmake_debug_
  build/bin/Debug/llvm-objdump -d ch10_1.cpu0.o
  
  ch10_1.cpu0.o:  file format ELF32-unknown
  
  Disassembly of section .text:
  .text:
       0: 00 2d 00 08                                   ld  $2, 8($sp)
       4: 01 0d 00 04                                   st  $zero, 4($sp)
       8: 09 30 00 00                                   addiu $3, $zero, 0
       c: 13 31 20 00                                   add $3, $at, $2
      10: 14 32 30 00                                   sub $3, $2, $3
      14: 15 21 30 00                                   mul $2, $at, $3
      18: 16 32 00 00                                   div $3, $2
      1c: 17 23 00 00                                   divu  $2, $3
      20: 18 21 30 00                                   and $2, $at, $3
      24: 19 31 20 00                                   or  $3, $at, $2
      28: 1a 12 30 00                                   xor $at, $2, $3
      2c: 50 43 00 00                                   mult  $4, $3
      30: 51 32 00 00                                   multu $3, $2
      34: 40 30 00 00                                   mfhi  $3
      38: 41 20 00 00                                   mflo  $2
      3c: 42 20 00 00                                   mthi  $2
      40: 43 20 00 00                                   mtlo  $2
      44: 1b 22 00 02                                   sra $2, $2, 2
      48: 1c 21 10 03                                   rol $2, $at, 3
      4c: 1d 33 10 04                                   ror $3, $3, 4
      50: 1e 22 00 02                                   shl $2, $2, 2
      54: 1f 23 00 05                                   shr $2, $3, 5
      58: 10 23 00 00                                   cmp $zero, $2, $3
      5c: 20 00 00 14                                   jeq $zero, 20
      60: 21 00 00 10                                   jne $zero, 16
      64: 22 ff ff ec                                   jlt $zero, -20
      68: 24 ff ff f0                                   jle $zero, -16
      6c: 23 ff ff fc                                   jgt $zero, -4
      70: 25 ff ff f4                                   jge $zero, -12
      74: 2a 00 04 00                                   swi 1024
      78: 2b 01 00 00                                   jsub  65536
      7c: 2c e0 00 00                                   ret $lr
      80: 2d e6 00 00                                   jalr  $6
      84: 09 30 00 70                                   addiu $3, $zero, 112
      88: 1e 33 00 10                                   shl $3, $3, 16
      8c: 09 10 00 00                                   addiu $at, $zero, 0
      90: 19 33 10 00                                   or  $3, $3, $at
      94: 09 30 00 80                                   addiu $3, $zero, 128
      98: 1e 36 00 10                                   shl $3, $6, 16
      9c: 09 10 00 00                                   addiu $at, $zero, 0
      a0: 19 36 10 00                                   or  $3, $6, $at
      a4: 13 33 60 00                                   add $3, $3, $6
      a8: 09 30 00 90                                   addiu $3, $zero, 144
      ac: 1e 33 00 10                                   shl $3, $3, 16
      b0: 09 10 00 00                                   addiu $at, $zero, 0
      b4: 19 33 10 00                                   or  $3, $3, $at


We replace cmp and jeg with explicit $sw in assembly and $zero in disassembly for 
AsmParser support. It's OK with just a little bad in readability and in assembly 
programing than implicit representation.


Verilog of CPU0
------------------

Verilog language is an IEEE standard in IC design. There are a lot of book and 
documents for this language. Web site [#]_ has a pdf [#]_ in this. 
Example code LLVMBackendTutorialExampleCode/cpu0s_verilog/raw/cpu0s.v is the 
cpu0 design in Verilog. In Appendix A, we have downloaded and installed Icarus 
Verilog tool both on iMac and Linux. The cpu0s.v is a simple design with only 
280 lines of code. Alough it has not the pipeline features, we can assume the 
cpu0 backend code run on the pipeline machine because the pipeline version  
use the same machine instructions. Verilog is C like language in syntex and 
this book is a compiler book, so we list the cpu0s.v as well as the building 
command directly as below. We expect 
readers can understand the Verilog code just with a little patient and no need 
further explanation. There are two type of I/O. One is memory mapped I/O, the 
other is instruction I/O. CPU0 use memory mapped I/O, we set the memory address 
0x7000 as the output port. When meet the instruction **"st $ra, cx($rb)"**, where 
cx($rb) is 0x7000 (28672), CPU0 display the content as follows,

.. code-block:: c++

        ST :
          if (R[b]+c16 == 28672)
            $display("%4dns %8x : %8x OUTPUT=%-d", $stime, pc0, ir, R[a]);


.. rubric:: LLVMBackendTutorialExampleCode/cpu0_verilog/raw/cpu0s.v
.. literalinclude:: ../LLVMBackendTutorialExampleCode/cpu0_verilog/raw/cpu0s.v
    :linenos:


.. code-block:: bash

  JonathantekiiMac:raw Jonathan$ pwd
  /Users/Jonathan/test/2/lbd/LLVMBackendTutorialExampleCode/cpu0_verilog/raw
  JonathantekiiMac:raw Jonathan$ iverilog -o cpu0s cpu0s.v 


Run program on CPU0 machine
---------------------------

Now let's compile ch_run_backend.cpp as below. Since code size grows up from low to high 
address and stack grows up from high to low address. We set $sp at 0x6ffc because 
cpu0s.v use 0x7000 bytes of memory.

.. rubric:: LLVMBackendTutorialExampleCode/InputFiles/InitRegs.cpp
.. literalinclude:: ../LLVMBackendTutorialExampleCode/InputFiles/InitRegs.cpp
    :linenos:

.. rubric:: LLVMBackendTutorialExampleCode/InputFiles/print.h
.. literalinclude:: ../LLVMBackendTutorialExampleCode/InputFiles/print.h
    :linenos:

.. rubric:: LLVMBackendTutorialExampleCode/InputFiles/print.cpp
.. literalinclude:: ../LLVMBackendTutorialExampleCode/InputFiles/print.cpp
    :linenos:

.. rubric:: LLVMBackendTutorialExampleCode/InputFiles/ch_run_backend.cpp
.. literalinclude:: ../LLVMBackendTutorialExampleCode/InputFiles/ch_run_backend.cpp
    :lines: 7-
    :linenos:

.. code-block:: bash

  JonathantekiiMac:InputFiles Jonathan$ pwd
  /Users/Jonathan/test/2/lbd/LLVMBackendTutorialExampleCode/InputFiles
  JonathantekiiMac:InputFiles Jonathan$ clang -c ch_run_backend.cpp -emit-llvm -o 
  ch_run_backend.bc
  JonathantekiiMac:InputFiles Jonathan$ /Users/Jonathan/llvm/test/cmake_debug_
  build/bin/Debug/llc -march=cpu0 -relocation-model=static -filetype=obj 
  ch_run_backend.bc -o ch_run_backend.cpu0.o
  JonathantekiiMac:InputFiles Jonathan$ /Users/Jonathan/llvm/test/cmake_debug_
  build/bin/Debug/llvm-objdump -d ch_run_backend.cpu0.o | tail -n +6| awk '{print "/* " 
  $1 " */\t" $2 " " $3 " " $4 " " $5 "\t/* " $6"\t" $7" " $8" " $9" " $10 "\t*/"}'
   > ../cpu0_verilog/raw/cpu0s.hex
  
  118-165-81-39:raw Jonathan$ cat cpu0s.hex 
  ...
  /* 4c: */ 2b 00 00 20 /* jsub 0    */
  /* 50: */ 01 2d 00 04 /* st $2, 4($sp)    */
  /* 54: */ 2b 00 01 44 /* jsub 0    */
  
  
As above code the subroutine address for **"jsub #offset"** are 0. 
This is correct since C language support separate compile and the subroutine 
address is decided at link time for static address mode or at 
load time for PIC address mode.
Since our backend didn't implement the linker and loader, we change the  
**"jsub #offset"** encode in Chapter10_2/ as follow,

.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_2/MCTargetDesc/Cpu0MCCodeEmitter.cpp
.. code-block:: c++

  unsigned Cpu0MCCodeEmitter::
  getJumpTargetOpValue(const MCInst &MI, unsigned OpNo,
             SmallVectorImpl<MCFixup> &Fixups) const {
  
    unsigned Opcode = MI.getOpcode();
    ...
    if (Opcode == Cpu0::JSUB)
      Fixups.push_back(MCFixup::Create(0, Expr,
                       MCFixupKind(Cpu0::fixup_Cpu0_PC24)));
    else if (Opcode == Cpu0::JSUB)
      Fixups.push_back(MCFixup::Create(0, Expr,
                       MCFixupKind(Cpu0::fixup_Cpu0_24)));
    else
      llvm_unreachable("unexpect opcode in getJumpAbsoluteTargetOpValue()");
    
    return 0;
  }

We change JSUB from Relocation Records fixup_Cpu0_24 to Non-Relocaton Records 
fixup_Cpu0_PC24 as the definition below. This change is fine since if call a 
outside defined subroutine, it will add a Relocation Record for this 
**"jsub #offset"**. At this point, we set it to Non-Relocaton Records for 
run on CPU0 Verilog machine. If one day, the CPU0 linker is appeared and the 
linker do the sections arrangement, we should adjust it back to Relocation 
Records. A good linker will reorder the sections for optimization in 
data/function access. In other word, 
keep the global variable access as close as possible to reduce cache miss 
possibility.

.. rubric:: LLVMBackendTutorialExampleCode/Chapter10_2/MCTargetDesc/Cpu0AsmBackend.cpp
.. code-block:: c++

    const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const {
      const static MCFixupKindInfo Infos[Cpu0::NumTargetFixupKinds] = {
        // This table *must* be in same the order of fixup_* kinds in
        // Cpu0FixupKinds.h.
        //
        // name                    offset  bits  flags
        ...
        { "fixup_Cpu0_24",           0,     24,   0 },
        ...
        { "fixup_Cpu0_PC24",         0,     24,  MCFixupKindInfo::FKF_IsPCRel },
        ...
      }
      ...
    }

Let's run the Chapter10_2/ with ``llvm-objdump -d`` for input files 
ch_run_backend.cpp and ch_run_sum_i.cpp to generate the hex file 
and input to cpu0s Verilog simulator to get the output result as below. You can
unmark the $display() in cpu0s.v to trace the memory binary code and destination
register change at every instruction execution. Remind ch_run_sum_i.cpp have to
compile with option ``clang -target mips-unknown-linux-gnu`` and use the clang of
your build instead of download from Xcode on iMac. The ~/llvm/release/
cmake_debug_build/bin/Debug/ is my build clang from source code.

.. rubric:: LLVMBackendTutorialExampleCode/InputFiles/ch_run_sum_i.cpp
.. literalinclude:: ../LLVMBackendTutorialExampleCode/InputFiles/ch_run_sum_i.cpp
    :lines: 7-
    :linenos:

.. code-block:: bash

  JonathantekiiMac:InputFiles Jonathan$ clang -c ch_run_backend.cpp -emit-llvm -o 
  ch_run_backend.bc
  JonathantekiiMac:InputFiles Jonathan$ /Users/Jonathan/llvm/test/cmake_debug_
  build/bin/Debug/llc -march=cpu0 -relocation-model=static -filetype=obj 
  ch_run_backend.bc -o ch_run_backend.cpu0.o
  JonathantekiiMac:InputFiles Jonathan$ /Users/Jonathan/llvm/test/cmake_debug_
  build/bin/Debug/llvm-objdump -d ch_run_backend.cpu0.o | tail -n +6| awk '{print "/* " 
  $1 " */\t" $2 " " $3 " " $4 " " $5 "\t/* " $6"\t" $7" " $8" " $9" " $10 "\t*/"}'
   > ../cpu0_verilog/raw/cpu0s.hex
   
  JonathantekiiMac:raw Jonathan$ ./cpu0s
  WARNING: cpu0s.v:386: $readmemh(cpu0s.hex): Not enough words in the file for the 
  requested range [0:28671].
  taskInterrupt(001)
  0                                                                                 
  2                                                                                 
  1073741821                                                                         
  10                                                                                 
  128                                                                               
  146                                                                               
  RET to PC < 0, finished!
  
  JonathantekiiMac:raw Jonathan$ cd ../../InputFiles/
  JonathantekiiMac:InputFiles Jonathan$ ~/llvm/release/cmake_debug_build/bin/Debug/
  clang -target mips-unknown-linux-gnu -c ch_run_sum_i.cpp -emit-llvm -o 
  ch_run_sum_i.bc
  JonathantekiiMac:InputFiles Jonathan$ /Users/Jonathan/llvm/test/cmake_debug_
  build/bin/Debug/llc -march=cpu0 -relocation-model=static -filetype=obj 
  ch_run_sum_i.bc -o ch_run_sum_i.cpu0.o
  JonathantekiiMac:InputFiles Jonathan$ /Users/Jonathan/llvm/test/cmake_debug_
  build/bin/Debug/llvm-objdump -d ch_run_sum_i.cpu0.o | tail -n +6| awk '{print 
  "/* " $1 " */\t" $2 " " $3 " " $4 " " $5 "\t/* " $6"\t" $7" " $8" " $9" " $10 
  "\t*/"}' > ../cpu0_verilog/raw/cpu0s.hex
  JonathantekiiMac:InputFiles Jonathan$ cd ../cpu0_verilog/raw/
  JonathantekiiMac:raw Jonathan$ ./cpu0s
  WARNING: cpu0s.v:386: $readmemh(cpu0s.hex): Not enough words in the file for the 
  requested range [0:28671].
  taskInterrupt(001)
  15                                                                                 
  RET to PC < 0, finished!


From the result as below, you can find the print_integer() which implemented by C 
language has more instructions while the print1_integer() which implemented by 
assembly has less instructions. But the C version is better in portability since 
the assembly version is binding with machine assembly language and make the 
assumption that the stack size of print1_integer() is 8.

.. code-block:: bash

  JonathantekiiMac:raw Jonathan$ cd ../../InputFiles/
  JonathantekiiMac:InputFiles Jonathan$ /Users/Jonathan/llvm/test/cmake_debug_build
  /bin/Debug/llvm-objdump -d ch_run_backend.cpu0.o 
  ...
  _Z13print_integeri:
       100: 09 dd ff e8                                   addiu $sp, $sp, -24
       104: 02 ed 00 14                                   st  $lr, 20($sp)
       108: 02 bd 00 10                                   st  $fp, 16($sp)
       10c: 13 bd 00 00                                   add $fp, $sp, $zero
       110: 01 2b 00 18                                   ld  $2, 24($fp)
       114: 02 2b 00 0c                                   st  $2, 12($fp)
       118: 02 2d 00 00                                   st  $2, 0($sp)
       11c: 2b 00 02 cc                                   jsub  716
       120: 02 2b 00 08                                   st  $2, 8($fp)
       124: 02 2d 00 00                                   st  $2, 0($sp)
       128: 2b 00 04 3c                                   jsub  1084
       12c: 13 db 00 00                                   add $sp, $fp, $zero
       130: 01 bd 00 10                                   ld  $fp, 16($sp)
       134: 01 ed 00 14                                   ld  $lr, 20($sp)
       138: 09 dd 00 18                                   addiu $sp, $sp, 24
       13c: 2c e0 00 00                                   ret $lr
  ...
  _Z14print1_integeri:
       5e4: 09 dd ff f8                                   addiu $sp, $sp, -8
       5e8: 02 bd 00 04                                   st  $fp, 4($sp)
       5ec: 13 bd 00 00                                   add $fp, $sp, $zero
       5f0: 01 2b 00 08                                   ld  $2, 8($fp)
       5f4: 02 2b 00 00                                   st  $2, 0($fp)
       5f8: 01 1d 00 08                                   ld  $1, 8($sp)
       5fc: 02 10 70 00                                   st  $1, 28672($zero)
       600: 13 db 00 00                                   add $sp, $fp, $zero
       604: 01 bd 00 04                                   ld  $fp, 4($sp)
       608: 09 dd 00 08                                   addiu $sp, $sp, 8
       60c: 2c e0 00 00                                   ret $lr


Unmark the $display() in cpu0s.v to trace the memory binary code and destination
register change at every instruction execution as follows,

.. code-block:: bash

  JonathantekiiMac:raw Jonathan$ ./cpu0s
  WARNING: cpu0s.v:386: $readmemh(cpu0s.hex): Not enough words in the file for the 
  requested range [0:28671].
  00000000: 2600000c
  00000004: 26000004
  00000008: 26000004
  0000000c: 26fffffc
  00000010: 09100000
  00000014: 09200000
  ...
  taskInterrupt(001)
  1530ns 00000054 : 02ed002c m[28620+44  ]=-1          SW=00000000
  1610ns 00000058 : 02bd0028 m[28620+40  ]=0           SW=00000000
  1850ns 00000064 : 022b0024 m[28620+36  ]=0           SW=10000000
  1930ns 00000068 : 022b0020 m[28620+32  ]=0           SW=10000000
  ...
  29370ns 000002ec : 022b0004 m[28548+4   ]=28672       SW=50000000
  29530ns 000002f4 : 05320000 m[28672+0   ]=  SW=50000000
  15                                                                                 
  RET to PC < 0, finished!

As above result, cpu0s.v dump the memory first after read input cpu0s.hex. 
Next, it run instructions from address 0 and print each destination 
register value in the fourth column. 
The first column is the nano seconds of timing. The second 
is instruction address. The third is instruction content. 
We have checked the **">>"** is correct on both signed and unsigned int type 
, and tracking the variable **a** value by print_integer(). The output value
**a** is 15 in this program.


We show Verilog PC output by display the I/O memory mapped address but didn't 
implementing the output hardware interface or port. The real output hardware 
interface/port is hardware output device dependent, such as RS232, speaker, 
LED, .... You should implement the I/O interface/port when you want to program 
FPGA and wire I/O device to the I/O port.



.. [#] http://www.ece.umd.edu/courses/enee359a/

.. [#] http://www.ece.umd.edu/courses/enee359a/verilog_tutorial.pdf
