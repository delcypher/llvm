//===- MappedBlockStream.h - Reads stream data from a PDBFile ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEBUGINFO_PDB_RAW_MAPPEDBLOCKSTREAM_H
#define LLVM_DEBUGINFO_PDB_RAW_MAPPEDBLOCKSTREAM_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/DebugInfo/CodeView/StreamInterface.h"
#include "llvm/Support/Error.h"
#include <cstdint>
#include <vector>

namespace llvm {
namespace pdb {

class PDBFile;

class MappedBlockStream : public codeview::StreamInterface {
public:
  MappedBlockStream(uint32_t StreamIdx, const PDBFile &File);

  Error readBytes(uint32_t Offset,
                  MutableArrayRef<uint8_t> Buffer) const override;
  Error getArrayRef(uint32_t Offset, ArrayRef<uint8_t> &Buffer,
                    uint32_t Length) const override;

  uint32_t getLength() const override { return StreamLength; }

private:
  uint32_t StreamLength;
  std::vector<uint32_t> BlockList;
  const PDBFile &Pdb;
};

} // end namespace pdb
} // end namespace llvm

#endif // LLVM_DEBUGINFO_PDB_RAW_MAPPEDBLOCKSTREAM_H
