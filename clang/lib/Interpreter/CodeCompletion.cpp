#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/CodeCompletion.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Sema/CodeCompleteOptions.h"
#include "clang/Sema/Sema.h"
#include <iostream>
#include <sstream>
#include "Logger.h"
#include <format>


namespace clang{
Logger CompletionLogger{"completion.log"};

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
  CompletionLogger.debug("Start ProcessCodeComplete");
  for (unsigned I = 0; I < NumResults; ++I) {
    auto &Result = InResults[I];
    // Class members that are shadowed by subclasses are usually noise.
    // if (Result.Hidden && Result.Declaration &&
    //     Result.Declaration->isCXXClassMember())
    //   continue;
    // if (//!Opts.IncludeIneligibleResults &&
    //     (Result.Availability == CXAvailability_NotAvailable ||
    //      Result.Availability == CXAvailability_NotAccessible))
    //   continue;
    // if (Result.Declaration &&
    //     !Context.getBaseType().isNull() // is this a member-access context?
    //     && true // isExcludedMember(*Result.Declaration)
    //     )
    //   continue;
    // // Skip injected class name when no class scope is not explicitly set.
    // // E.g. show injected A::A in `using A::A^` but not in "A^".
    // if (Result.Declaration && !Context.getCXXScopeSpecifier() &&
    //     true)
    //     // isInjectedClass(*Result.Declaration))
    //   continue;
    std::ostringstream stringStream;
    switch (Result.Kind) {
    case CodeCompletionResult::RK_Declaration:
      if (auto *ID = Result.Declaration->getIdentifier()) {
        stringStream << "Decl ID:";
        stringStream << ID->getName().str();
        CompletionLogger.debug(stringStream.str());
        // std::cout << "[completion] Decl ID: " << ID->getName().str() << "\n";
        Results.push_back(Result);
      }
      // if (auto* VD = llvm::dyn_cast<clang::VarDecl>(Result.getDeclaration())) {
      //   std::cout << "[completion] hello world " << VD->getName().str() << "\n";
      // }
      break;
    default:
      break;
    case CodeCompletionResult::RK_Keyword:
      stringStream << "Symbol/Keyword:";
      stringStream << Result.Keyword;
      CompletionLogger.debug(stringStream.str());
      Results.push_back(Result);
      break;
    // case CodeCompletionResult::RK_Macro:
    //   std::cout << "[Completion] macro " << Result.Macro->getName().str() << "\n";
    //   break;
    // case CodeCompletionResult::RK_Pattern:
    //   std::cout << "[Completion] pattern " << Result.Pattern->getAllTypedText() << "\n";
    //   break;
    }
    // We choose to never append '::' to completion results in clangd.
    // std::cout << "[completion] hello world " << Result.Kind << "\n";
    // if (Result.Kind == CodeCompletionResult::RK_Declaration)
    //   if (auto* VD = llvm::dyn_cast<clang::VarDecl>(Result.getDeclaration())) {
    //     std::cout << "[completion] hello world " << VD->getName().str() << "\n";
    //   }


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
      // std::cout << "[completion] Decl ID: " << ID->getName().str() << "\n";
      // if (auto* VD = llvm::dyn_cast<clang::VarDecl>(Result.getDeclaration())) {
      //   std::cout << "[completion] hello world " << VD->getName().str() << "\n";
      // }
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
