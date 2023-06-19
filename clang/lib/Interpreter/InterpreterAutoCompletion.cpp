#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/InterpreterAutoCompletion.h"
#include "clang/Sema/CodeCompleteOptions.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Sema/Sema.h"
#include <iostream>

namespace clang{
void GlobalEnv::extend(llvm::StringRef name) {
  names.push_back(name);
}

clang::CodeCompleteOptions getClangCompleteOpts() {
  clang::CodeCompleteOptions Opts;
  Opts.IncludeCodePatterns = true;
  Opts.IncludeMacros = true;
  Opts.IncludeGlobals = true;
  Opts.IncludeBriefComments = true;
  return Opts;
}



// ReplCompletionConsumer(llvm::unique_function<void()> ResultsCallback)
ReplCompletionConsumer::ReplCompletionConsumer() : CodeCompleteConsumer(getClangCompleteOpts()),
      // CCContext(CodeCompletionContext::CCC_Other),
      CCAllocator(std::make_shared<GlobalCodeCompletionAllocator>()),
      CCTUInfo(CCAllocator)
{
    // assert(this->ResultsCallback);
}

void ReplCompletionConsumer::ProcessCodeCompleteResults(class Sema &S, CodeCompletionContext Context,
                                                        CodeCompletionResult *InResults,
                                                        unsigned NumResults) {
  std::cout << "Start ProcessCodeComplete\n";
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
    switch (Result.Kind) {
    case CodeCompletionResult::RK_Declaration:
      if (auto *ID = Result.Declaration->getIdentifier()) {
        std::cout << "[completion] Decl ID: " << ID->getName().str() << "\n";
        Results.push_back(Result);
      }
      // if (auto* VD = llvm::dyn_cast<clang::VarDecl>(Result.getDeclaration())) {
      //   std::cout << "[completion] hello world " << VD->getName().str() << "\n";
      // }
      break;
    default:
      break;
    // case CodeCompletionResult::RK_Keyword:
    //   std::cout << "[Completion] keyword " << Result.Keyword << "\n";
    //   break;
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

std::vector<StringRef> ReplCompletionConsumer::toCodeCompleteStrings() {
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
    default:
      break;
    }
  }
  return CompletionStrings;
}

llvm::Expected<std::unique_ptr<Interpreter>>
recreateInterpreter(clang::IncrementalCompilerBuilder& CB) {
  auto CIOrErr = CB.CreateCpp();
  if (auto Err = CIOrErr.takeError()) {
    return std::move(Err);
  }
  auto InterpOrErr = clang::Interpreter::create(std::move(*CIOrErr));
  if (auto Err = InterpOrErr.takeError()) {
    return std::move(Err);
  }
  return std::move(*InterpOrErr);
}


std::vector<llvm::LineEditor::Completion> ReplListCompleter::operator()(llvm::StringRef Buffer,
                                                                        size_t Pos) const{
  std::vector<llvm::LineEditor::Completion> Comps;
  auto Interp = recreateInterpreter(CB);
  if (!Interp) {
    return Comps;
  }


  auto* CConsumer = new clang::ReplCompletionConsumer();

  auto* Clang = const_cast<CompilerInstance*>((*Interp)->getCompilerInstance());
  Clang->getPreprocessorOpts().SingleFileParseMode = true;

  Clang->getLangOpts().SpellChecking = false;
  Clang->getLangOpts().DelayedTemplateParsing = false;

  auto &FrontendOpts = Clang->getFrontendOpts();
  // clang::Preprocessor& PP = Clang->getPreprocessor();
  FrontendOpts.CodeCompleteOpts = clang::getClangCompleteOpts();
  // FrontendOpts.CodeCompletionAt.FileName = std::string(DummyFN);
  // FrontendOpts.CodeCompletionAt.Line = 7;
  // FrontendOpts.CodeCompletionAt.Column = 3;


  // Clang->setInvocation(std::move(CI));
  // Clang->createDiagnostics(&D, false);
  // Clang->getPreprocessorOpts().SingleFileParseMode = true;;
  Clang->setCodeCompletionConsumer(CConsumer);
  Clang->getSema().CodeCompleter = CConsumer;
  // Clang->setTarget(clang::TargetInfo::CreateTargetInfo(
  //     Clang->getDiagnostics(), Clang->getInvocation().TargetOpts));

  std::string AllCodeText = MainInterp.getAllInput() + "\n" + Buffer.str();
  auto Lines = std::count(AllCodeText.begin(), AllCodeText.end(), '\n') + 1;

  (*Interp)->CodeComplete(AllCodeText, Pos + 1, Lines);

  // auto AllInput = (*Interp)->getAllInput();
  // auto Nlines = std::count(AllInput.begin(), AllInput.end(), '\n') + 1;
  // std::cout << "\n" << Nlines << " " << Pos << "\n";
  // first wew look for a space
  // if space is not found from right, then use the whole typed string
  // Otherwise, use Buffer[found_idx:] to search for completion candidates.



  // size_t space_pos = Buffer.rfind(" ");
  // llvm::StringRef s;
  // if (space_pos == llvm::StringRef::npos) {
  //   s = Buffer;
  // } else {
  //   s = Buffer.substr(space_pos + 1);
  // }

  // if s is empty, return an empty vector;
  // if (s.empty()) {
  //   return Comps;
  // }

  // for (auto c : env.names) {
  //   if (c.startswith(s)) {
  //     Comps.push_back(llvm::LineEditor::Completion(c.substr(s.size()).str(), c.str()));
  //   }
  // }
  return Comps;
}
} // namespace clang
