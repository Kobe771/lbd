//===- lib/ReaderWriter/ELF/Cpu0/Cpu0LinkingContext.h ---------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_READER_WRITER_ELF_CPU0_LINKER_CONTEXT_H
#define LLD_READER_WRITER_ELF_CPU0_LINKER_CONTEXT_H

#include "Cpu0TargetHandler.h"

#include "lld/ReaderWriter/ELFLinkingContext.h"

#include "llvm/Object/ELF.h"
#include "llvm/Support/ELF.h"

namespace lld {
namespace elf {
#if 1
/// \brief cpu0 internal references.
enum {
  /// \brief The 32 bit index of the relocation in the got this reference refers
  /// to.
  LLD_R_CPU0_GOTRELINDEX = 1024,
};
#endif

class Cpu0LinkingContext LLVM_FINAL : public ELFLinkingContext {
public:
  Cpu0LinkingContext(llvm::Triple triple)
      : ELFLinkingContext(triple, std::unique_ptr<TargetHandlerBase>(
                                  new Cpu0TargetHandler(*this))) {}

  virtual bool isLittleEndian() const { return false; }

  virtual void addPasses(PassManager &) const;

  virtual uint64_t getBaseAddress() const {
    if (_baseAddress == 0)
      return 0x000000;
    return _baseAddress;
  }

  virtual bool isDynamicRelocation(const DefinedAtom &,
                                   const Reference &r) const {
    switch (r.kind()){
//    case llvm::ELF::R_CPU0_RELATIVE:
    case llvm::ELF::R_CPU0_GLOB_DAT:
      return true;
    default:
      return false;
    }
  }

  virtual bool isPLTRelocation(const DefinedAtom &,
                               const Reference &r) const {
    switch (r.kind()){
    case llvm::ELF::R_CPU0_JUMP_SLOT:
    case llvm::ELF::R_CPU0_RELGOT:
      return true;
    default:
      return false;
    }
  }

  /// \brief Cpu0 has two relative relocations
  /// a) for supporting IFUNC - R_CPU0_RELGOT
  /// b) for supporting relative relocs - R_CPU0_RELATIVE
  virtual bool isRelativeReloc(const Reference &r) const {
    switch (r.kind()) {
    case llvm::ELF::R_CPU0_RELGOT:
#if 0
    case llvm::ELF::R_CPU0_RELATIVE:
      return true;
#endif
    default:
      return false;
    }
  }

  virtual ErrorOr<Reference::Kind> relocKindFromString(StringRef str) const;
  virtual ErrorOr<std::string> stringFromRelocKind(Reference::Kind kind) const;

};
} // end namespace elf
} // end namespace lld

#endif
