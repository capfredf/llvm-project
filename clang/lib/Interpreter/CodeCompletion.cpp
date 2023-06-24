#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/CodeCompletion.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Sema/CodeCompleteOptions.h"
#include "clang/Sema/Sema.h"
#include <iostream>
#include <sstream>
#include <format>


namespace clang{

clang::CodeCompleteOptions getClangCompleteOpts() {
  clang::CodeCompleteOptions Opts;
  Opts.IncludeCodePatterns = true;
  Opts.IncludeMacros = true;
  Opts.IncludeGlobals = true;
  Opts.IncludeBriefComments = true;
  return Opts;
}

void ReplCompletionConsumer::ProcessCodeCompleteResults(class Sema &S, CodeCompletionContext Context,
                                                        CodeCompletionResult *InResults,
                                                        unsigned NumResults) {
  for (unsigned I = 0; I < NumResults; ++I) {
    auto &Result = InResults[I];
    std::ostringstream stringStream;
    switch (Result.Kind) {
    case CodeCompletionResult::RK_Declaration:
      if (auto *ID = Result.Declaration->getIdentifier()) {
        stringStream << "Decl ID:";
        stringStream << ID->getName().str();
        Results.push_back(Result);
      }
      break;
    default:
      break;
    case CodeCompletionResult::RK_Keyword:
      stringStream << "Symbol/Keyword:";
      stringStream << Result.Keyword;
      Results.push_back(Result);
      break;
    }

  }
}

std::vector<StringRef> ReplListCompleter::toCodeCompleteStrings(const std::vector<CodeCompletionResult> &Results) const{
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


std::vector<llvm::LineEditor::Completion> ReplListCompleter::operator()(llvm::StringRef Buffer,
                                                                        size_t Pos) const{
  std::vector<llvm::LineEditor::Completion> Comps;

  auto Interp = Interpreter::createForCodeCompletion(CB);

  if (auto Err = Interp.takeError()) {
    consumeError(std::move(Err));
    return Comps;
  }

  std::string AllCodeText = MainInterp.getAllInput() + "\nvoid dummy(){\n" + Buffer.str() + "}";
  auto Lines = std::count(AllCodeText.begin(), AllCodeText.end(), '\n') + 1;

  auto Results = (*Interp)->CodeComplete(AllCodeText, Pos + 1, Lines);


  size_t space_pos = Buffer.rfind(" ");
  llvm::StringRef s;
  if (space_pos == llvm::StringRef::npos) {
    s = Buffer;
  } else {
    s = Buffer.substr(space_pos + 1);
  }


  for (auto c : toCodeCompleteStrings(Results)) {
    if (c.startswith(s)) {
      Comps.push_back(llvm::LineEditor::Completion(c.substr(s.size()).str(), c.str()));
    }
  }
  return Comps;
}
} // namespace clang
