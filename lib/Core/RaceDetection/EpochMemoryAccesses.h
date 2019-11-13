#pragma once

#include "ObjectAccesses.h"

#include <unordered_map>

namespace klee {
  /// Tracks all accesses to memory objects within one epoch.
  /// Entries are stored hierarchical:
  ///   @class{EpochMemoryAccesses}
  ///   └-> per MemoryObject -> @class{ObjectAccesses}
  class EpochMemoryAccesses {
    private:
      // Keys are the ids of @class{MemoryObject} objects
      std::unordered_map<std::uint64_t, ObjectAccesses> memoryOperations;

    public:
      EpochMemoryAccesses() = default;
      EpochMemoryAccesses(const EpochMemoryAccesses& t) = default;

      void trackMemoryOperation(const MemoryOperation& op);

      void pruneDataForMemoryObject(const MemoryObject* obj);

      std::optional<std::reference_wrapper<const ObjectAccesses>> getMemoryAccessesOfThread(
              const MemoryObject* mo) const;
  };
}