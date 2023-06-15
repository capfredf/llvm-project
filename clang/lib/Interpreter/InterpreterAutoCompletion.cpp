#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/InterpreterAutoCompletion.h"
#include "clang/Sema/CodeCompleteOptions.h"
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
      if (auto *ID = Result.Declaration->getIdentifier())
        std::cout << "[completion] Decl ID: " << ID->getName().str() << "\n";
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

    // Results.push_back(Result);
  }
}

ReplListCompleter::ReplListCompleter(clang::GlobalEnv &env, clang::Interpreter& Interp) : env(env), Interp(Interp){
}

std::vector<llvm::LineEditor::Completion> ReplListCompleter::operator()(llvm::StringRef Buffer,
                                                                        size_t Pos) const{
  std::vector<llvm::LineEditor::Completion> Comps;
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
}
