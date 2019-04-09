//===-- InstructionInfoTable.h ----------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_LIB_INSTRUCTIONINFOTABLE_H
#define KLEE_LIB_INSTRUCTIONINFOTABLE_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace llvm {
  class Function;
  class Instruction;
  class Module; 
}

namespace klee {
class KInstruction;

  /* Stores debug information for a KInstruction */
  struct InstructionInfo {
    unsigned id;
    const std::string &file;
    unsigned line;
    unsigned column;
    unsigned assemblyLine;

  private:
    KInstruction *ki = nullptr;
    std::vector<const KInstruction *> liveLocals;
    bool hasValidLiveLocals = false;

  public:
    InstructionInfo(unsigned _id, const std::string &_file, unsigned _line,
                    unsigned _column, unsigned _assemblyLine)
        : id(_id), file(_file), line(_line), column(_column),
          assemblyLine(_assemblyLine) {}

    bool setKInstruction(KInstruction *kinst) {
      if (ki)
        return false;

      // only set ki if it was nullptr
      ki = kinst;
      return true;
    }

    KInstruction *getKInstruction() const {
      return ki;
    }

    void setLiveLocals(std::vector<const KInstruction *> &&set) {
      liveLocals = std::move(set);
      hasValidLiveLocals = true;
    }

    const std::vector<const KInstruction *> *getLiveLocals() const {
      if (!hasValidLiveLocals)
        return nullptr;
      return &liveLocals;
    }
  };

  /* Stores debug information for a KInstruction */
  struct FunctionInfo {
    unsigned id;
    const std::string &file;
    unsigned line;
    uint64_t assemblyLine;

  public:
    FunctionInfo(unsigned _id, const std::string &_file, unsigned _line,
                 uint64_t _assemblyLine)
        : id(_id), file(_file), line(_line), assemblyLine(_assemblyLine) {}

    FunctionInfo(const FunctionInfo &) = delete;
    FunctionInfo &operator=(FunctionInfo const &) = delete;

    FunctionInfo(FunctionInfo &&) = default;
  };

  class InstructionInfoTable {
    std::unordered_map<const llvm::Instruction *,
                       std::unique_ptr<InstructionInfo>>
        infos;
    std::unordered_map<const llvm::Function *, std::unique_ptr<FunctionInfo>>
        functionInfos;
    std::vector<std::unique_ptr<std::string>> internedStrings;

  public:
    InstructionInfoTable(const llvm::Module &m);

    unsigned getMaxID() const;
    const InstructionInfo &getInfo(const llvm::Instruction &) const;
    const FunctionInfo &getFunctionInfo(const llvm::Function &) const;
  };

}

#endif
