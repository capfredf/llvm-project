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
    std::cout << "hello world\n";
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
