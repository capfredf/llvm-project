//===------ CodeCompletion.h - Code Completion for ClangRepl -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the classes which performs code completion at the REPL.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_INTERPRETER_CODE_COMPLETION_H
#define LLVM_CLANG_INTERPRETER_CODE_COMPLETION_H
#include "clang/Sema/CodeCompleteConsumer.h"
#include "llvm/LineEditor/LineEditor.h"

namespace clang {
class Interpreter;
class IncrementalCompilerBuilder;


struct ReplListCompleter {
  IncrementalCompilerBuilder &CB;
  Interpreter &MainInterp;
  ReplListCompleter(IncrementalCompilerBuilder &CB, Interpreter &Interp)
      : CB(CB), MainInterp(Interp){};
  std::vector<llvm::LineEditor::Completion> operator()(llvm::StringRef Buffer,
                                                       size_t Pos) const;

private:
  std::vector<StringRef>
  toCodeCompleteStrings(const std::vector<CodeCompletionResult> &Results) const;
};

} // namespace clang
#endif
