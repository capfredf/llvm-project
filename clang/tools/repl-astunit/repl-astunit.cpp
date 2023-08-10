#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Interpreter/CodeCompletion.h"
#include "clang/Interpreter/Interpreter.h"
#include "clang/Sema/CodeCompleteConsumer.h"

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/LineEditor/LineEditor.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h" // llvm_shutdown
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"

#include <iostream>

llvm::ExitOnError ExitOnErr;
int main(int argc, const char **argv) {
  // TODO create CI using CB
  // TODO LoadFromCompilerInvocation
  llvm::cl::ParseCommandLineOptions(argc, argv);

  llvm::llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.

  // std::vector<const char *> ClangArgv(ClangArgs.size());
  // std::transform(ClangArgs.begin(), ClangArgs.end(), ClangArgv.begin(),
  //                [](const std::string &s) -> const char * { return s.data(); });
  // Initialize all targets (required for device offloading)
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();

  clang::IncrementalCompilerBuilder CB;

  std::unique_ptr<clang::CompilerInstance> CI = ExitOnErr(CB.CreateCpp());

  std::cout<< "Hello, WORLD\n";
  return 0;
}
