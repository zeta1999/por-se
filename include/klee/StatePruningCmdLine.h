#ifndef KLEE_STATEPRUNINGFLAGS_H
#define KLEE_STATEPRUNINGFLAGS_H

#include "llvm/Support/CommandLine.h"

namespace klee {

inline llvm::cl::opt<bool> PruneStates("state-pruning",
                                       llvm::cl::desc("Enable pruning of states (default=on)"),
                                       llvm::cl::init(true));

inline llvm::cl::opt<bool> DebugStatePruning("debug-state-pruning",
                                             llvm::cl::desc("Log state pruning debug info to stderr (default=off)"),
                                             llvm::cl::init(false));


inline llvm::cl::opt<std::size_t> MaxContextSwitchDegree(
  "max-csd",
  llvm::cl::desc("Only explore alternatives with context swtch degree up to this limit.  Set to 0 to disable (default=10)"),
  llvm::cl::init(10));


}
#endif // KLEE_STATEPRUNINGFLAGS_H
