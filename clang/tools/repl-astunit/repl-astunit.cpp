#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Interpreter/CodeCompletion.h"
#include "clang/Interpreter/Interpreter.h"
#include "clang/Sema/CodeCompleteConsumer.h"
#include "clang/Serialization/PCHContainerOperations.h"

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/LineEditor/LineEditor.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h" // llvm_shutdown
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/ADT/SmallVector.h"

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

  // Perform the remapping of source files.
  const char* complete_filename="/home/capfredf/tmp/hello1.cpp";

  llvm::SmallVector<clang::ASTUnit::RemappedFile, 4> RemappedFiles;

  if (llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> MB =
      llvm::MemoryBuffer::getFileOrSTDIN(complete_filename, true); !MB.getError())

    RemappedFiles.push_back(std::make_pair(complete_filename, MB->release()));

  unsigned complete_line = 2, complete_column = 1;
  std::vector<clang::CodeCompletionResult> CCResults;
  std::unique_ptr<clang::CompilerInstance> CI = ExitOnErr(CB.CreateCpp());
  auto DiagOpts = clang::DiagnosticOptions();
  auto consumer = clang::ReplCompletionConsumer(CCResults);
  std::unique_ptr<clang::Interpreter> Interp = ExitOnErr(clang::Interpreter::create(std::move(CI)));
  auto* InterpCI = const_cast<clang::CompilerInstance*>(Interp->getCompilerInstance());
  auto diag = InterpCI->getDiagnosticsPtr();
  clang::ASTUnit* AU =
    clang::ASTUnit::LoadFromCompilerInvocationAction(const_cast<clang::CompilerInstance*>(Interp->getCompilerInstance())->getInvocationPtr(),
                                                     std::make_shared<clang::PCHContainerOperations>(),
                                                     diag);
  {
    llvm::SmallVector<clang::StoredDiagnostic, 8> sd = {};
    llvm::SmallVector<const llvm::MemoryBuffer *, 1> tb = {};
    InterpCI->getFrontendOpts().Inputs[0] = clang::FrontendInputFile(complete_filename, clang::Language::CXX, clang::InputKind::Source);

    AU->CodeComplete(complete_filename, complete_line, complete_column,
                   RemappedFiles, false,
                   false,
                   false,
                   consumer,
                   std::make_shared<clang::PCHContainerOperations>(),
                   *diag,
                   InterpCI->getLangOpts(),
                   InterpCI->getSourceManager(),
                   InterpCI->getFileManager(),
                   sd, tb);
  }

  for (auto Res : CCResults) {
    switch (Res.Kind) {
    case clang::CodeCompletionResult::RK_Declaration:
      if (auto *ID = Res.Declaration->getIdentifier()) {
        std::cout << " ID " << ID->getName().str() << "\n";
      }
      break;
    case clang::CodeCompletionResult::RK_Keyword:
      std::cout << " sym " << Res.Keyword << "\n";
      break;
    default:
      break;
    }
  }

  return 0;
}
