#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Interpreter/ExternalSource.h"
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

clang::IncrementalCompilerBuilder CB;
const char* complete_filename="<<<completion>>>";

llvm::Expected<std::unique_ptr<clang::Interpreter>> createInterpreter() {
  llvm::Expected<std::unique_ptr<clang::CompilerInstance>> CIOrErr = CB.CreateCpp();
  if (auto Err = CIOrErr.takeError()) {
    return std::move(Err);
  }

  llvm::Expected<std::unique_ptr<clang::Interpreter>> InterpOrErr = clang::Interpreter::create(std::move(*CIOrErr));

  if (auto Err = InterpOrErr.takeError()) {
    return std::move(Err);
  }
  return InterpOrErr;
}

static void codeComplete(llvm::StringRef Prefix, unsigned complete_column, const clang::CompilerInstance* ParentCI) {
  std::unique_ptr<llvm::MemoryBuffer> MB = llvm::MemoryBuffer::getMemBufferCopy(Prefix, complete_filename);
  llvm::SmallVector<clang::ASTUnit::RemappedFile, 4> RemappedFiles;

  RemappedFiles.push_back(std::make_pair(complete_filename, MB.release()));
  std::vector<clang::CodeCompletionResult> CCResults;

  auto DiagOpts = clang::DiagnosticOptions();
  auto consumer = clang::ReplCompletionConsumer(CCResults);

  auto InterpOrErr = createInterpreter();
  if (auto Err = InterpOrErr.takeError()) {
    llvm::consumeError(std::move(Err));
  }
  auto* InterpCI = const_cast<clang::CompilerInstance*>((*InterpOrErr)->getCompilerInstance());

  auto diag = InterpCI->getDiagnosticsPtr();


  clang::ASTUnit* AU =
    clang::ASTUnit::LoadFromCompilerInvocationAction(InterpCI->getInvocationPtr(),
                                                     std::make_shared<clang::PCHContainerOperations>(),
                                                     diag);
  {
    llvm::SmallVector<clang::StoredDiagnostic, 8> sd = {};
    llvm::SmallVector<const llvm::MemoryBuffer *, 1> tb = {};
    InterpCI->getFrontendOpts().Inputs[0] = clang::FrontendInputFile(complete_filename, clang::Language::CXX, clang::InputKind::Source);


    // I still don't get why the frontends allow the input file to be different from the file to be completed, but it does not do code completion when they are different.....
    // This has been really confusing since I started working on Clang/LLVM
    AU->CodeComplete(complete_filename, 1, complete_column,
                     RemappedFiles, false,
                     false,
                     false,
                     consumer,
                     std::make_shared<clang::PCHContainerOperations>(),
                     *diag,
                     InterpCI->getLangOpts(),
                     InterpCI->getSourceManager(),
                     InterpCI->getFileManager(),
                     sd, tb,
                     [ParentCI](clang::CompilerInstance& CI) -> void {
                       clang::ExternalSource *myExternalSource = new clang::ExternalSource(
                                                                                           CI.getASTContext(), CI.getFileManager(), ParentCI->getASTContext(),
                                                                                           ParentCI->getFileManager());
                       llvm::IntrusiveRefCntPtr<clang::ExternalASTSource> astContextExternalSource(
                                                                                                   myExternalSource);
                       CI.getASTContext().setExternalSource(astContextExternalSource);
                       CI.getASTContext().getTranslationUnitDecl()->setHasExternalVisibleStorage(true);
                     });
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


}

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

  auto Interp = ExitOnErr(createInterpreter());
  llvm::Error res = Interp->ParseAndExecute("int foo = 42;");
  if (res) {
    llvm::consumeError(std::move(res));
  }
  codeComplete("f", 2, Interp->getCompilerInstance());
  return 0;
}
