//===-- llvm/lib/CodeGen/AsmPrinter/CodeViewDebug.h ----*- C++ -*--===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains support for writing Microsoft CodeView debug info.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_CODEGEN_ASMPRINTER_CODEVIEWDEBUG_H
#define LLVM_LIB_CODEGEN_ASMPRINTER_CODEVIEWDEBUG_H

#include "DebugHandlerBase.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/DebugInfo/CodeView/MemoryTypeTableBuilder.h"
#include "llvm/DebugInfo/CodeView/TypeIndex.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Target/TargetLoweringObjectFile.h"

namespace llvm {

class StringRef;
class LexicalScope;

/// \brief Collects and handles line tables information in a CodeView format.
class LLVM_LIBRARY_VISIBILITY CodeViewDebug : public DebugHandlerBase {
  MCStreamer &OS;
  codeview::MemoryTypeTableBuilder TypeTable;

  /// Represents the most general definition range.
  struct LocalVarDefRange {
    /// Indicates that variable data is stored in memory relative to the
    /// specified register.
    int InMemory : 1;

    /// Offset of variable data in memory.
    int DataOffset : 31;

    /// Offset of the data into the user level struct. If zero, no splitting
    /// occurred.
    uint16_t StructOffset;

    /// Register containing the data or the register base of the memory
    /// location containing the data.
    uint16_t CVRegister;

    /// Compares all location fields. This includes all fields except the label
    /// ranges.
    bool isDifferentLocation(LocalVarDefRange &O) {
      return InMemory != O.InMemory || DataOffset != O.DataOffset ||
             StructOffset != O.StructOffset || CVRegister != O.CVRegister;
    }

    SmallVector<std::pair<const MCSymbol *, const MCSymbol *>, 1> Ranges;
  };

  static LocalVarDefRange createDefRangeMem(uint16_t CVRegister, int Offset);
  static LocalVarDefRange createDefRangeReg(uint16_t CVRegister);

  /// Similar to DbgVariable in DwarfDebug, but not dwarf-specific.
  struct LocalVariable {
    const DILocalVariable *DIVar = nullptr;
    SmallVector<LocalVarDefRange, 1> DefRanges;
  };

  struct InlineSite {
    SmallVector<LocalVariable, 1> InlinedLocals;
    SmallVector<const DILocation *, 1> ChildSites;
    const DISubprogram *Inlinee = nullptr;
    unsigned SiteFuncId = 0;
  };

  // For each function, store a vector of labels to its instructions, as well as
  // to the end of the function.
  struct FunctionInfo {
    /// Map from inlined call site to inlined instructions and child inlined
    /// call sites. Listed in program order.
    std::unordered_map<const DILocation *, InlineSite> InlineSites;

    /// Ordered list of top-level inlined call sites.
    SmallVector<const DILocation *, 1> ChildSites;

    SmallVector<LocalVariable, 1> Locals;

    DebugLoc LastLoc;
    const MCSymbol *Begin = nullptr;
    const MCSymbol *End = nullptr;
    unsigned FuncId = 0;
    unsigned LastFileId = 0;
    bool HaveLineInfo = false;
  };
  FunctionInfo *CurFn;

  /// The set of comdat .debug$S sections that we've seen so far. Each section
  /// must start with a magic version number that must only be emitted once.
  /// This set tracks which sections we've already opened.
  DenseSet<MCSectionCOFF *> ComdatDebugSections;

  /// Switch to the appropriate .debug$S section for GVSym. If GVSym, the symbol
  /// of an emitted global value, is in a comdat COFF section, this will switch
  /// to a new .debug$S section in that comdat. This method ensures that the
  /// section starts with the magic version number on first use. If GVSym is
  /// null, uses the main .debug$S section.
  void switchToDebugSectionForSymbol(const MCSymbol *GVSym);

  /// The next available function index for use with our .cv_* directives. Not
  /// to be confused with type indices for LF_FUNC_ID records.
  unsigned NextFuncId = 0;

  codeview::TypeIndex VoidFnTyIdx;

  /// Get a type index for a generic void function type.
  codeview::TypeIndex getGenericFunctionTypeIndex();

  InlineSite &getInlineSite(const DILocation *InlinedAt,
                            const DISubprogram *Inlinee);

  void recordFuncIdForSubprogram(const DISubprogram *SP);

  static void collectInlineSiteChildren(SmallVectorImpl<unsigned> &Children,
                                        const FunctionInfo &FI,
                                        const InlineSite &Site);

  /// Remember some debug info about each function. Keep it in a stable order to
  /// emit at the end of the TU.
  MapVector<const Function *, FunctionInfo> FnDebugInfo;

  /// Map from DIFile to .cv_file id.
  DenseMap<const DIFile *, unsigned> FileIdMap;

  /// All inlined subprograms in the order they should be emitted.
  SmallSetVector<const DISubprogram *, 4> InlinedSubprograms;

  /// Map from DI metadata nodes to CodeView type indices. Primarily indexed by
  /// DIType* and DISubprogram*.
  DenseMap<const DINode *, codeview::TypeIndex> TypeIndices;

  typedef std::map<const DIFile *, std::string> FileToFilepathMapTy;
  FileToFilepathMapTy FileToFilepathMap;
  StringRef getFullFilepath(const DIFile *S);

  unsigned maybeRecordFile(const DIFile *F);

  void maybeRecordLocation(DebugLoc DL, const MachineFunction *MF);

  void clear() {
    assert(CurFn == nullptr);
    FileIdMap.clear();
    FnDebugInfo.clear();
    FileToFilepathMap.clear();
  }

  /// Emit the magic version number at the start of a CodeView type or symbol
  /// section. Appears at the front of every .debug$S or .debug$T section.
  void emitCodeViewMagicVersion();

  void emitTypeInformation();

  void emitInlineeLinesSubsection();

  void emitDebugInfoForFunction(const Function *GV, FunctionInfo &FI);

  void emitInlinedCallSite(const FunctionInfo &FI, const DILocation *InlinedAt,
                           const InlineSite &Site);

  typedef DbgValueHistoryMap::InlinedVariable InlinedVariable;

  void collectVariableInfo(const DISubprogram *SP);

  void collectVariableInfoFromMMITable(DenseSet<InlinedVariable> &Processed);

  /// Records information about a local variable in the appropriate scope. In
  /// particular, locals from inlined code live inside the inlining site.
  void recordLocalVariable(LocalVariable &&Var, const DILocation *Loc);

  void emitLocalVariable(const LocalVariable &Var);

public:
  CodeViewDebug(AsmPrinter *Asm);

  void setSymbolSize(const llvm::MCSymbol *, uint64_t) override {}

  /// \brief Emit the COFF section that holds the line table information.
  void endModule() override;

  /// \brief Gather pre-function debug information.
  void beginFunction(const MachineFunction *MF) override;

  /// \brief Gather post-function debug information.
  void endFunction(const MachineFunction *) override;

  /// \brief Process beginning of an instruction.
  void beginInstruction(const MachineInstr *MI) override;
};
} // End of namespace llvm

#endif
