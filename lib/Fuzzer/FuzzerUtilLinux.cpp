//===- FuzzerUtilLinux.cpp - Misc utils -----------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Misc utils for Linux.
//===----------------------------------------------------------------------===//
#include "FuzzerInternal.h"
#if LIBFUZZER_LINUX
#include <stdlib.h>
namespace fuzzer {
int ExecuteCommand(const std::string &Command) {
  // We are assuming that Glibc was built with a re-entrant `system()` here.
  // FIXME: How can we check for this at compile time?
  return system(Command.c_str());
}
}
#endif // LIBFUZZER_LINUX
