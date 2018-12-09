#ifndef KLEE_MEMORYFINGERPRINT_H
#define KLEE_MEMORYFINGERPRINT_H

#include "klee/Config/config.h"
#include "klee/Expr.h"

#ifndef __cpp_rtti
// stub for typeid to use CryptoPP without RTTI
template <typename T> const std::type_info &FakeTypeID(void) {
  assert(0 && "CryptoPP tries to use typeid()");
}
#define typeid(a) FakeTypeID < a > ()
#endif
#include <cryptopp/blake2.h>
#ifndef __cpp_rtti
#undef typeid
#endif

#include <array>
#include <iomanip>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace llvm {
class BasicBlock;
class Instruction;
} // namespace llvm

namespace klee {
class KFunction;
class KInstruction;

class MemoryFingerprintDelta;

class MemoryFingerprint_CryptoPP_BLAKE2b;
class MemoryFingerprint_Dummy;

// Set default implementation
using MemoryFingerprint = MemoryFingerprint_CryptoPP_BLAKE2b;

template <typename Derived, std::size_t hashSize> class MemoryFingerprintT {

protected:
  using hash_t = std::array<std::uint8_t, hashSize>;
  using dummy_t = std::set<std::string>;

public:
  typedef
      typename std::conditional<hashSize == 0, dummy_t, hash_t>::type value_t;

private:
  Derived &getDerived() { return *(static_cast<Derived *>(this)); }

  value_t fingerprintValue{};
  std::unordered_map<const Array *, std::uint64_t> symbolicReferences;

  template <typename T, typename std::enable_if<std::is_same<T, hash_t>::value,
                                                int>::type = 0>
  void executeXOR(T &dst, const T &src) {
    for (std::size_t i = 0; i < hashSize; ++i) {
      dst[i] ^= src[i];
    }
  }

  template <typename T, typename std::enable_if<std::is_same<T, hash_t>::value,
                                                int>::type = 0>
  void executeAdd(T &dst, const T &src) {
    executeXOR(dst, src);
  }

  template <typename T, typename std::enable_if<std::is_same<T, hash_t>::value,
                                                int>::type = 0>
  void executeRemove(T &dst, const T &src) {
    executeXOR(dst, src);
  }

  template <typename T, typename std::enable_if<std::is_same<T, dummy_t>::value,
                                                int>::type = 0>
  void executeAdd(T &dst, const T &src) {
    for (auto &elem : src) {
      if (elem.empty())
        continue;

      if (dst.find(elem) != dst.end()) {
        std::string debug_str;
        llvm::raw_string_ostream debug(debug_str);
        llvm::errs() << "FAILED: \n";
        getDerived().decodeAndPrintFragment(debug, elem, true);
        llvm::errs() << debug.str() << "\n";
        llvm::errs() << "DST: " << toString(dst) << "\n";
        llvm::errs().flush();
        assert(0 && "fragment already in fingerprint");
      }
      dst.insert(elem);
    }
  }

  template <typename T, typename std::enable_if<std::is_same<T, dummy_t>::value,
                                                int>::type = 0>
  void executeRemove(T &dst, const T &src) {
    for (auto &elem : src) {
      if (elem.empty())
        continue;

      auto pos = dst.find(elem);
      if (pos == dst.end()) {
        std::string debug_str;
        llvm::raw_string_ostream debug(debug_str);
        llvm::errs() << "FAILED: \n";
        getDerived().decodeAndPrintFragment(debug, elem, true);
        llvm::errs() << debug.str() << "\n";
        llvm::errs() << "DST: " << toString(dst) << "\n";
        llvm::errs().flush();
        assert(0 && "fragment not in fingerprint");
      }
      dst.erase(pos);
    }
  }

protected:
  // buffer that holds current hash after calling generateHash()
  value_t buffer = {};

private:
  // information on what went into buffer
  bool bufferContainsSymbolic = false;
  std::unordered_map<const Array *, std::uint64_t> bufferSymbolicReferences;

  void resetBufferRefCount() {
    bufferContainsSymbolic = false;
    bufferSymbolicReferences.clear();
  }

public:
  MemoryFingerprintT() = default;
  MemoryFingerprintT(const MemoryFingerprintT &) = default;

  void addToFingerprint();
  void removeFromFingerprint();

  void addToFingerprintAndDelta(MemoryFingerprintDelta &delta);
  void removeFromFingerprintAndDelta(MemoryFingerprintDelta &delta);
  void addToDeltaOnly(MemoryFingerprintDelta &delta);
  void removeFromDeltaOnly(MemoryFingerprintDelta &delta);
  void addDelta(MemoryFingerprintDelta &delta);
  void removeDelta(MemoryFingerprintDelta &delta);

  value_t getFingerprint(std::vector<ref<Expr>> &expressions);

  value_t getFingerprintWithDelta(std::vector<ref<Expr>> &expressions,
                                  MemoryFingerprintDelta &delta);

  void updateExpr(const ref<Expr> expr);

  void updateConstantExpr(const ConstantExpr &expr);

  template <typename T, typename std::enable_if<std::is_same<T, hash_t>::value,
                                                int>::type = 0>
  static std::string toString(const T &fingerprintValue) {
    std::stringstream result;
    for (const auto byte : fingerprintValue) {
      result << std::hex << std::setfill('0') << std::setw(2);
      result << static_cast<unsigned int>(byte);
    }
    return result.str();
  }

  template <typename T, typename std::enable_if<std::is_same<T, dummy_t>::value,
                                                int>::type = 0>
  static std::string toString(const T &fingerprintValue) {
    return Derived::toString_impl(fingerprintValue);
  }

  bool updateWriteFragment(std::uint64_t address, ref<Expr> value);
  bool updateLocalFragment(std::uint64_t threadID,
                           std::uint64_t stackFrameIndex,
                           const llvm::Instruction *inst, ref<Expr> value);
  bool updateArgumentFragment(std::uint64_t threadID, std::uint64_t sfIndex,
                              const KFunction *kf, std::uint64_t argumentIndex,
                              ref<Expr> value);
  bool updateProgramCounterFragment(std::uint64_t threadID,
                                    std::uint64_t sfIndex,
                                    const llvm::Instruction *i);
  bool updateFunctionFragment(std::uint64_t threadID, std::uint64_t sfIndex,
                              const KFunction *callee,
                              const KInstruction *caller);
  bool updateExternalCallFragment(std::uint64_t externalFunctionCallCounter);
};

template <typename T>
class MemoryFingerprintOstream : public llvm::raw_ostream {
private:
  T &hash;
  std::uint64_t pos = 0;

public:
  explicit MemoryFingerprintOstream(T &_hash) : hash(_hash) {}
  void write_impl(const char *ptr, std::size_t size) override;
  uint64_t current_pos() const override { return pos; }
  ~MemoryFingerprintOstream() override { assert(GetNumBytesInBuffer() == 0); }
};

class MemoryFingerprint_CryptoPP_BLAKE2b
    : public MemoryFingerprintT<MemoryFingerprint_CryptoPP_BLAKE2b, 32> {
  friend class MemoryFingerprintT<MemoryFingerprint_CryptoPP_BLAKE2b, 32>;
  using Base = MemoryFingerprintT<MemoryFingerprint_CryptoPP_BLAKE2b, 32>;

private:
  CryptoPP::BLAKE2b blake2b{false, 32};
  MemoryFingerprintOstream<CryptoPP::BLAKE2b> ostream{blake2b};

  void generateHash();
  void clearHash();
  void updateUint8(const std::uint8_t value);
  void updateUint64(const std::uint64_t value);
  llvm::raw_ostream &updateOstream();

public:
  MemoryFingerprint_CryptoPP_BLAKE2b() = default;
  MemoryFingerprint_CryptoPP_BLAKE2b(const MemoryFingerprint_CryptoPP_BLAKE2b &other) : Base(other) { }
  MemoryFingerprint_CryptoPP_BLAKE2b(MemoryFingerprint_CryptoPP_BLAKE2b &&) = delete;
  MemoryFingerprint_CryptoPP_BLAKE2b& operator=(MemoryFingerprint_CryptoPP_BLAKE2b &&) = delete;
  ~MemoryFingerprint_CryptoPP_BLAKE2b() = default;
};

class MemoryFingerprint_Dummy
    : public MemoryFingerprintT<MemoryFingerprint_Dummy, 0> {
  friend class MemoryFingerprintT<MemoryFingerprint_Dummy, 0>;
  using Base = MemoryFingerprintT<MemoryFingerprint_Dummy, 0>;

private:
  std::string current;
  bool first = true;
  llvm::raw_string_ostream ostream{current};

  void generateHash();
  void clearHash();
  void updateUint8(const std::uint8_t value);
  void updateUint64(const std::uint64_t value);
  llvm::raw_ostream &updateOstream();

  static std::string toString_impl(dummy_t fingerprintValue);

public:
  MemoryFingerprint_Dummy() = default;
  MemoryFingerprint_Dummy(const MemoryFingerprint_Dummy &other) : Base(other) { }
  MemoryFingerprint_Dummy(MemoryFingerprint_Dummy &&) = delete;
  MemoryFingerprint_Dummy& operator=(MemoryFingerprint_Dummy &&) = delete;
  ~MemoryFingerprint_Dummy() = default;

  struct DecodedFragment {
    std::size_t writes = 0;
    bool containsSymbolicValue = false;
    bool hasPathConstraint = false;
    bool output = false;
  };
  static DecodedFragment decodeAndPrintFragment(llvm::raw_ostream &os,
                                                std::string fragment,
                                                bool showMemoryOperations);
};

// NOTE: MemoryFingerprint needs to be a complete type
class MemoryFingerprintDelta {
private:
  friend class MemoryState;

  template <typename Derived, std::size_t hashSize>
  friend void MemoryFingerprintT<Derived, hashSize>::addToFingerprintAndDelta(
      MemoryFingerprintDelta &delta);

  template <typename Derived, std::size_t hashSize>
  friend void
  MemoryFingerprintT<Derived, hashSize>::removeFromFingerprintAndDelta(
      MemoryFingerprintDelta &delta);

  template <typename Derived, std::size_t hashSize>
  friend void MemoryFingerprintT<Derived, hashSize>::addToDeltaOnly(
      MemoryFingerprintDelta &delta);

  template <typename Derived, std::size_t hashSize>
  friend void MemoryFingerprintT<Derived, hashSize>::removeFromDeltaOnly(
      MemoryFingerprintDelta &delta);

  template <typename Derived, std::size_t hashSize>
  friend void MemoryFingerprintT<Derived, hashSize>::addDelta(
      MemoryFingerprintDelta &delta);

  template <typename Derived, std::size_t hashSize>
  friend void MemoryFingerprintT<Derived, hashSize>::removeDelta(
      MemoryFingerprintDelta &delta);

  MemoryFingerprint::value_t fingerprintValue{};
  std::unordered_map<const Array *, std::uint64_t> symbolicReferences;
};

} // namespace klee

// NOTE: MemoryFingerprintDelta needs to be a complete type
#define INCLUDE_MEMORYFINGERPRINT_BASE_H
// include definitions of MemoryFingerprintT member methods
#include "MemoryFingerprint_Base.h"
#undef INCLUDE_MEMORYFINGERPRINT_BASE_H

#endif
