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
#include "llvm/LineEditor/LineEditor.h"

namespace llvm {
class StringRef;
class Error;
} // namespace llvm

namespace clang {
class Interpreter;
class CodeCompletionResult;
class IncrementalCompilerBuilder;

struct ReplListCompleter {
  IncrementalCompilerBuilder &CB;
  Interpreter &MainInterp;
  ReplListCompleter(IncrementalCompilerBuilder &CB, Interpreter &Interp) :
    CB(CB), MainInterp(Interp){};
  std::vector<llvm::LineEditor::Completion> operator()(llvm::StringRef Buffer,
                                                       size_t Pos) const;
  std::vector<llvm::LineEditor::Completion> operator()(llvm::StringRef Buffer,
                                                       size_t Pos,
                                                       llvm::Error &Err) const;
private:
  std::vector<llvm::StringRef>
  toCodeCompleteStrings(const std::vector<CodeCompletionResult> &Results) const;

};

} // namespace clang
#endif
