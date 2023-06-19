//===--- InterpreterAutoCompletion.h - Incremental Utils --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef LLVM_CLANG_INTERPRETER_AUTO_COMPLETION_H
#define LLVM_CLANG_INTERPRETER_AUTO_COMPLETION_H
#include "clang/Sema/CodeCompleteConsumer.h"
#include "Interpreter.h"
#include "llvm/LineEditor/LineEditor.h"

namespace clang{
struct GlobalEnv{
  std::vector<llvm::StringRef> names;

  void extend(llvm::StringRef name);
};

clang::CodeCompleteOptions getClangCompleteOpts();


struct ReplCompletionConsumer : public CodeCompleteConsumer {

  ReplCompletionConsumer();
  void ProcessCodeCompleteResults(class Sema &S, CodeCompletionContext Context,
                                  CodeCompletionResult *InResults,
                                  unsigned NumResults) final;

  clang::CodeCompletionAllocator& getAllocator() override {
    return *CCAllocator;
  }

  clang::CodeCompletionTUInfo& getCodeCompletionTUInfo() override {
    return CCTUInfo;
  }

  std::vector<StringRef> toCodeCompleteStrings();

private:
  std::shared_ptr<GlobalCodeCompletionAllocator> CCAllocator;
  CodeCompletionTUInfo CCTUInfo;
  std::vector<CodeCompletionResult> Results;
};


struct ReplListCompleter {
  IncrementalCompilerBuilder& CB;
  Interpreter& MainInterp;
  ReplListCompleter(IncrementalCompilerBuilder& CB, Interpreter& Interp) : CB(CB), MainInterp(Interp) {};
  std::vector<llvm::LineEditor::Completion> operator()(llvm::StringRef Buffer, size_t Pos) const;
};

}
#endif
