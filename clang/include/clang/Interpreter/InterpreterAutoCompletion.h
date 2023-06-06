//===--- InterpreterAutoCompletion.h - Incremental Utils --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef LLVM_CLANG_INTERPRETER_AUTO_COMPLETION_H
#define LLVM_CLANG_INTERPRETER_AUTO_COMPLETION_H
#include "llvm/LineEditor/LineEditor.h"

namespace clang{
struct GlobalEnv{
  std::vector<llvm::StringRef> names;

  void extend(llvm::StringRef name);
};


struct ReplListCompleter {

  GlobalEnv &env;
  ReplListCompleter(GlobalEnv &env) : env(env) {}
  std::vector<llvm::LineEditor::Completion> operator()(llvm::StringRef Buffer, size_t Pos) const;
};

}
#endif
