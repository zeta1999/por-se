#pragma once

#include "klee/Config/Version.h"

#include "llvm/AsmParser/Parser.h"
#include "llvm/AsmParser/SlotMapping.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"

#include "gtest/gtest.h"

#include <sstream>

using namespace llvm;

std::unique_ptr<Module> parseAssembly(LLVMContext &ctx, StringRef source) {
  SMDiagnostic Error;
  SlotMapping Mapping;
  auto m = parseAssemblyString(source, Error, ctx, &Mapping);
  if (!m) {
    std::stringstream ss;
    raw_os_ostream os(ss);
    Error.print("parseAssembly", os);
    os.flush();
    ADD_FAILURE() << ss.str();
  }
  return m;
}
