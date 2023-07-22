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
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Sema/CodeCompleteConsumer.h"
#include "clang/Sema/CodeCompleteOptions.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/Debug.h"
#define DEBUG_TYPE "REPLCC"

namespace clang {

clang::CodeCompleteOptions getClangCompleteOpts() {
  clang::CodeCompleteOptions Opts;
  Opts.IncludeCodePatterns = true;
  Opts.IncludeMacros = true;
  Opts.IncludeGlobals = true;
  Opts.IncludeBriefComments = false;
  return Opts;
}

class CodeCompletionSubContext {
public:
  virtual ~CodeCompletionSubContext(){};
  virtual void
  HandleCodeCompleteResults(class Sema &S, CodeCompletionResult *InResults,
                            unsigned NumResults,
                            std::vector<CodeCompletionResult> &Results) = 0;
};

class CCSubContextRegular : public CodeCompletionSubContext {
  StringRef prefix;

public:
  CCSubContextRegular(StringRef prefix) : prefix(prefix){};
  virtual ~CCSubContextRegular(){};
  void HandleCodeCompleteResults(
      class Sema &S, CodeCompletionResult *InResults, unsigned NumResults,
      std::vector<CodeCompletionResult> &Results) override;
};

class CCSubContextCallSite : public CodeCompletionSubContext {
  StringRef calleeName;
  StringRef Prefix;
  std::optional<const FunctionDecl *> lookUp(CodeCompletionResult *InResults,
                                             unsigned NumResults);

public:
  CCSubContextCallSite(StringRef calleeName, StringRef Prefix)
      : calleeName(calleeName), Prefix(Prefix) {}
  virtual ~CCSubContextCallSite(){};
  void HandleCodeCompleteResults(
      class Sema &S, CodeCompletionResult *InResults, unsigned NumResults,
      std::vector<CodeCompletionResult> &Results) override;
};

class ReplCompletionConsumer : public CodeCompleteConsumer {
public:
  ReplCompletionConsumer(std::vector<CodeCompletionResult> &Results,
                         CodeCompletionSubContext *SubCtxt)
      : CodeCompleteConsumer(getClangCompleteOpts()),
        CCAllocator(std::make_shared<GlobalCodeCompletionAllocator>()),
        CCTUInfo(CCAllocator), Results(Results) {
    this->SubCtxt.reset(SubCtxt);
  }
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
  std::unique_ptr<CodeCompletionSubContext> SubCtxt;
};

void CCSubContextRegular::HandleCodeCompleteResults(
    class Sema &S, CodeCompletionResult *InResults, unsigned NumResults,
    std::vector<CodeCompletionResult> &Results) {
  for (unsigned I = 0; I < NumResults; ++I) {
    auto &Result = InResults[I];
    switch (Result.Kind) {
    case CodeCompletionResult::RK_Declaration:
      // if (!Filter.empty() && !(Result.Declaration->getIdentifier() &&
      //                          Result.Declaration->getIdentifier()->getName().startswith(Filter)))
      //                          {
      //   continue;
      // }
      if (Result.Hidden) {
        continue;
      }
      if (Result.Declaration->getIdentifier() &&
          Result.Declaration->getIdentifier()->getName().startswith(prefix)) {
        Results.push_back(Result);
      }
      // if (CodeCompletionString *CCS = Result.CreateCodeCompletionString(
      //         S, Context, getAllocator(), CCTUInfo,
      //         includeBriefComments())) {
      //   if (CCS->empty()) {
      //     continue;
      //   }
      //   switch ((*CCS)[0].Kind) {
      //   case CodeCompletionString::CK_ResultType:
      //     LLVM_DEBUG(llvm::dbgs() << "this is a function: " << (*CCS)[1].Text
      //     << "\n"); break;
      //   default:
      //     continue;
      //   }
      //   // if ( == ) {
      //   // } else
      //   // for (const auto& Chk : *CCS) {
      //   // }

      //   // auto S = CCS->getAsString();
      //   // auto Found = S.find(" ");
      //   // std::string D;
      //   // if (Found != std::string::npos) {
      //   //   D = S.substr(0, Found-1);
      //   // } else {
      //   //   D = S;
      //   // }
      //   // LLVM_DEBUG(llvm::dbgs() << S << "\n");
      // }
      break;
    default:
      break;
    case CodeCompletionResult::RK_Keyword:
      if (StringRef(Result.Keyword).startswith(prefix)) {
        Results.push_back(Result);
      }
      break;
    }
  }
}

std::optional<const FunctionDecl *>
CCSubContextCallSite::lookUp(CodeCompletionResult *InResults,
                             unsigned NumResults) {
  for (unsigned I = 0; I < NumResults; I++) {
    auto &Result = InResults[I];
    switch (Result.Kind) {
    case CodeCompletionResult::RK_Declaration:
      // if (!Filter.empty() && !(Result.Declaration->getIdentifier() &&
      //                          Result.Declaration->getIdentifier()->getName().startswith(Filter)))
      //                          {
      //   continue;
      // }
      if (Result.Hidden) {
        continue;
      }
      if (const auto *Function = Result.Declaration->getAsFunction()) {
        if (Function->isDestroyingOperatorDelete() ||
            Function->isOverloadedOperator()) {
          continue;
        }

        auto Name = Function->getDeclName();
        switch (Name.getNameKind()) {
        case DeclarationName::CXXConstructorName:
        case DeclarationName::CXXDestructorName:
          continue;
        default:
          if (Function->getName() == calleeName) {
            return Function;
          }
          continue;
        }
      }
      break;
    default:
      break;
    }
  }
  return std::nullopt;
}

void CCSubContextCallSite::HandleCodeCompleteResults(
    class Sema &S, CodeCompletionResult *InResults, unsigned NumResults,
    std::vector<CodeCompletionResult> &Results) {
  auto Function = lookUp(InResults, NumResults);
  if (!Function)
    return;
  for (unsigned I = 0; I < NumResults; I++) {
    auto &Result = InResults[I];
    switch (Result.Kind) {
    case CodeCompletionResult::RK_Declaration:
      // if (!Filter.empty() && !(Result.Declaration->getIdentifier() &&
      //                          Result.Declaration->getIdentifier()->getName().startswith(Filter)))
      //                          {
      //   continue;
      // }
      if (Result.Hidden) {
        continue;
      }
      if (!Result.Declaration->getIdentifier()) {
        continue;
      }
      if (auto *DD = dyn_cast<ValueDecl>(Result.Declaration)) {
        if (!DD->getName().startswith(Prefix))
          continue;

        auto ArgumentType = DD->getType();
        auto RequiredType = (*Function)->getParamDecl(0)->getType();
        if (RequiredType->isReferenceType()) {
          QualType RT = RequiredType->castAs<ReferenceType>()->getPointeeType();
          Sema::ReferenceConversions RefConv;
          Sema::ReferenceCompareResult RefRelationship =
              S.CompareReferenceRelationship(SourceLocation(), ArgumentType, RT,
                                             &RefConv);
          if (RefRelationship == Sema::Ref_Compatible) {
            Results.push_back(Result);
          }
        } else if (S.Context.hasSameType(ArgumentType, RequiredType)) {
          Results.push_back(Result);
        }
        LLVM_DEBUG(llvm::dbgs()
                   << DD->getName() << " : " << DD->getType() << "\n");
      }
      break;
    default:
      break;
    }
  }
}

void ReplCompletionConsumer::ProcessCodeCompleteResults(
    class Sema &S, CodeCompletionContext Context,
    CodeCompletionResult *InResults, unsigned NumResults) {
  // StringRef Filter = S.getPreprocessor().getCodeCompletionFilter();
  // if (SubCtxt == CCSC_CallSite) {
  //   lookup(InResults, CallSiteFuncName);
  // }
  LLVM_DEBUG(llvm::dbgs() << "\n");
  SubCtxt->HandleCodeCompleteResults(S, InResults, NumResults, Results);
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
  std::sort(CompletionStrings.begin(), CompletionStrings.end());
  return CompletionStrings;
}

static std::pair<StringRef, CodeCompletionSubContext *>
getCodeCompletionSubContext(llvm::StringRef CurInput, size_t Pos) {
  size_t LeftParenPos = CurInput.rfind("(");
  if (LeftParenPos == llvm::StringRef::npos) {
    size_t space_pos = CurInput.rfind(" ");
    llvm::StringRef Prefix;
    if (space_pos == llvm::StringRef::npos) {
      Prefix = CurInput;
    } else {
      Prefix = CurInput.substr(space_pos + 1);
    }

    return {Prefix, new CCSubContextRegular(Prefix)};
  }
  auto subs = CurInput.substr(0, LeftParenPos);
  size_t start_pos = subs.rfind(" ");
  if (start_pos == llvm::StringRef::npos) {
    start_pos = 0;
  }
  auto Prefix = CurInput.substr(LeftParenPos + 1, Pos);
  return {Prefix,
          new CCSubContextCallSite(
              subs.substr(start_pos, LeftParenPos - start_pos), Prefix)};
}

std::vector<llvm::LineEditor::Completion>
ReplListCompleter::operator()(llvm::StringRef Buffer, size_t Pos) const {
  auto Err = llvm::Error::success();
  auto Res = (*this)(Buffer, Pos, Err);
  if (Err)
    llvm::logAllUnhandledErrors(std::move(Err), llvm::errs(), "error: ");
  return Res;
}

std::vector<llvm::LineEditor::Completion>
ReplListCompleter::operator()(llvm::StringRef Buffer, size_t Pos,
                              llvm::Error &ErrRes) const {
  std::vector<llvm::LineEditor::Completion> Comps;
  std::vector<CodeCompletionResult> Results;
  auto [s, SubCtxt] = getCodeCompletionSubContext(Buffer, Pos);
  auto *CConsumer = new ReplCompletionConsumer(Results, SubCtxt);
  auto Interp = Interpreter::createForCodeCompletion(
      CB, MainInterp.getCompilerInstance(), CConsumer);

  if (auto Err = Interp.takeError()) {
    // log the error and returns an empty vector;
    ErrRes = std::move(Err);
    return {};
  }

  auto Lines = std::count(Buffer.begin(), Buffer.end(), '\n') + 1;

  if (auto Err = (*Interp)->CodeComplete(Buffer, Pos + 1, Lines)) {
    ErrRes = std::move(Err);
    return {};
  }

  for (auto c : toCodeCompleteStrings(Results)) {
    Comps.push_back(
        llvm::LineEditor::Completion(c.substr(s.size()).str(), c.str()));
  }
  return Comps;
}
} // namespace clang
