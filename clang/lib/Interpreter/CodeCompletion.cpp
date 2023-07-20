//===------ CodeCompletion.cpp - Code Completion for ClangRepl -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the classes which performs code completion at the REPL.
//
//===----------------------------------------------------------------------===//

#include "clang/Interpreter/CodeCompletion.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/Interpreter.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Sema/CodeCompleteConsumer.h"
#include "clang/Sema/CodeCompleteOptions.h"
#include "clang/Sema/Sema.h"

namespace clang {

clang::CodeCompleteOptions getClangCompleteOpts() {
  clang::CodeCompleteOptions Opts;
  Opts.IncludeCodePatterns = true;
  Opts.IncludeMacros = true;
  Opts.IncludeGlobals = true;
  Opts.IncludeBriefComments = true;
  return Opts;
}

class ReplCompletionConsumer : public CodeCompleteConsumer {
public:
  ReplCompletionConsumer(std::vector<CodeCompletionResult> &Results)
      : CodeCompleteConsumer(getClangCompleteOpts()),
        CCAllocator(std::make_shared<GlobalCodeCompletionAllocator>()),
        CCTUInfo(CCAllocator), Results(Results){};
  void ProcessCodeCompleteResults(class Sema &S, CodeCompletionContext Context,
                                  CodeCompletionResult *InResults,
                                  unsigned NumResults) final;

  clang::CodeCompletionAllocator &getAllocator() override {
    return *CCAllocator;
  }

  clang::CodeCompletionTUInfo &getCodeCompletionTUInfo() override {
    return CCTUInfo;
  }

private:
  std::shared_ptr<GlobalCodeCompletionAllocator> CCAllocator;
  CodeCompletionTUInfo CCTUInfo;
  std::vector<CodeCompletionResult> &Results;
};

void ReplCompletionConsumer::ProcessCodeCompleteResults(
    class Sema &S, CodeCompletionContext Context,
    CodeCompletionResult *InResults, unsigned NumResults) {
  for (unsigned I = 0; I < NumResults; ++I) {
    auto &Result = InResults[I];
    switch (Result.Kind) {
    case CodeCompletionResult::RK_Declaration:
      if (Result.Declaration->getIdentifier()) {
        Results.push_back(Result);
      }
      break;
    default:
      break;
    case CodeCompletionResult::RK_Keyword:
      Results.push_back(Result);
      break;
    }
  }
}

std::vector<StringRef> ReplListCompleter::toCodeCompleteStrings(
    const std::vector<CodeCompletionResult> &Results) const {
  std::vector<StringRef> CompletionStrings;
  for (auto Res : Results) {
    switch (Res.Kind) {
    case CodeCompletionResult::RK_Declaration:
      if (auto *ID = Res.Declaration->getIdentifier()) {
        CompletionStrings.push_back(ID->getName());
      }
      break;
    case CodeCompletionResult::RK_Keyword:
      CompletionStrings.push_back(Res.Keyword);
      break;
    default:
      break;
    }
  }
  return CompletionStrings;
}

ReplListCompleter::ReplListCompleter(IncrementalCompilerBuilder &CB,
                                     Interpreter &Interp)
    : CB(CB), MainInterp(Interp), ErrStream(llvm::errs()){

                                  };

std::vector<llvm::LineEditor::Completion>
ReplListCompleter::operator()(llvm::StringRef Buffer, size_t Pos) const {
  std::vector<llvm::LineEditor::Completion> Comps;
  std::vector<CodeCompletionResult> Results;
  auto *CConsumer = new ReplCompletionConsumer(Results);
  auto Interp = Interpreter::createForCodeCompletion(
      CB, MainInterp.getCompilerInstance(), CConsumer);

  ErrStream << "1234567";
  if (auto Err = Interp.takeError()) {
    // log the error and returns an empty vector;

    llvm::logAllUnhandledErrors(std::move(Err), ErrStream, "error: ");
    return {};
  }

  auto Lines = std::count(Buffer.begin(), Buffer.end(), '\n') + 1;

  if (auto Err = (*Interp)->CodeComplete(Buffer, Pos + 1, Lines)) {
    llvm::logAllUnhandledErrors(std::move(Err), ErrStream, "error: ");
    return {};
  }

  size_t space_pos = Buffer.rfind(" ");
  llvm::StringRef s;
  if (space_pos == llvm::StringRef::npos) {
    s = Buffer;
  } else {
    s = Buffer.substr(space_pos + 1);
  }

  for (auto c : toCodeCompleteStrings(Results)) {
    if (c.startswith(s))
      Comps.push_back(
          llvm::LineEditor::Completion(c.substr(s.size()).str(), c.str()));
  }
  return Comps;
}
} // namespace clang
