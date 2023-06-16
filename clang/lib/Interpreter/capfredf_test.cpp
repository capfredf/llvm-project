#include "clang/Interpreter/capfredf_test.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/Utils.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Interpreter/InterpreterAutoCompletion.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Tool.h"
#include "clang/Driver/Compilation.h"
#include "llvm/Option/ArgList.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Support/Signals.h"

#include <iostream>
#include <fstream>
#include <sstream>


void disableUnsupportedOptions(clang::CompilerInvocation &CI) {
  // Disable "clang -verify" diagnostics, they are rarely useful in clangd, and
  // our compiler invocation set-up doesn't seem to work with it (leading
  // assertions in VerifyDiagnosticConsumer).
  CI.getDiagnosticOpts().VerifyDiagnostics = false;
  CI.getDiagnosticOpts().ShowColors = false;

  // Disable any dependency outputting, we don't want to generate files or write
  // to stdout/stderr.
  CI.getDependencyOutputOpts().ShowIncludesDest = clang::ShowIncludesDestination::None;
  CI.getDependencyOutputOpts().OutputFile.clear();
  CI.getDependencyOutputOpts().HeaderIncludeOutputFile.clear();
  CI.getDependencyOutputOpts().DOTOutputFile.clear();
  CI.getDependencyOutputOpts().ModuleDependencyOutputDir.clear();

  // Disable any pch generation/usage operations. Since serialized preamble
  // format is unstable, using an incompatible one might result in unexpected
  // behaviours, including crashes.
  CI.getPreprocessorOpts().ImplicitPCHInclude.clear();
  CI.getPreprocessorOpts().PrecompiledPreambleBytes = {0, false};
  CI.getPreprocessorOpts().PCHThroughHeader.clear();
  CI.getPreprocessorOpts().PCHWithHdrStop = false;
  CI.getPreprocessorOpts().PCHWithHdrStopCreate = false;
  // Don't crash on `#pragma clang __debug parser_crash`

  CI.getPreprocessorOpts().DisablePragmaDebugCrash = true;

  // Always default to raw container format as clangd doesn't registry any other
  // and clang dies when faced with unknown formats.
  // CI.getHeaderSearchOpts().ModuleFormat =
  //     PCHContainerOperations().getRawReader().getFormats().front().str();

  CI.getFrontendOpts().Plugins.clear();
  CI.getFrontendOpts().AddPluginActions.clear();
  CI.getFrontendOpts().PluginArgs.clear();
  CI.getFrontendOpts().ProgramAction = clang::frontend::ParseSyntaxOnly;
  CI.getFrontendOpts().ActionName.clear();

  // These options mostly affect codegen, and aren't relevant to clangd. And
  // clang will die immediately when these files are not existed.
  // Disable these uninteresting options to make clangd more robust.
  CI.getLangOpts()->NoSanitizeFiles.clear();
  CI.getLangOpts()->XRayAttrListFiles.clear();
  CI.getLangOpts()->ProfileListFiles.clear();
  CI.getLangOpts()->XRayAlwaysInstrumentFiles.clear();
  CI.getLangOpts()->XRayNeverInstrumentFiles.clear();
}


static llvm::Expected<const llvm::opt::ArgStringList *>
GetCC1Arguments(clang::DiagnosticsEngine *Diagnostics,
                clang::driver::Compilation *Compilation) {
  // We expect to get back exactly one Command job, if we didn't something
  // failed. Extract that job from the Compilation.
  const clang::driver::JobList &Jobs = Compilation->getJobs();
  if (!Jobs.size() || !llvm::isa<clang::driver::Command>(*Jobs.begin()))
    return llvm::createStringError(llvm::errc::not_supported,
                                   "Driver initialization failed. "
                                   "Unable to create a driver job");

  // The one job we find should be to invoke clang again.
  const clang::driver::Command *Cmd = llvm::cast<clang::driver::Command>(&(*Jobs.begin()));
  if (llvm::StringRef(Cmd->getCreator().getName()) != "clang")
    return llvm::createStringError(llvm::errc::not_supported,
                                   "Driver initialization failed");

  return &Cmd->getArguments();
}

llvm::Expected<std::unique_ptr<clang::CompilerInstance>> MyCreateCI(const llvm::opt::ArgStringList &Argv);

llvm::Expected<std::unique_ptr<clang::CompilerInstance>>
createArgs(std::vector<const char *> &ClangArgv) {
  // If we don't know ClangArgv0 or the address of main() at this point, try
  // to guess it anyway (it's possible on some platforms).
  std::string MainExecutableName =
    llvm::sys::fs::getMainExecutable(nullptr, nullptr);

  ClangArgv.insert(ClangArgv.begin(), MainExecutableName.c_str());

  // Prepending -c to force the driver to do something if no action was
  // specified. By prepending we allow users to override the default
  // action and use other actions in incremental mode.
  // FIXME: Print proper driver diagnostics if the driver flags are wrong.
  // We do C++ by default; append right after argv[0] if no "-x" given
  //ClangArgv.insert(ClangArgv.end(), "-Xclang");
  ClangArgv.insert(ClangArgv.end(), "-Xclang");
  ClangArgv.insert(ClangArgv.end(), "-fincremental-extensions");
  //ClangArgv.insert(ClangArgv.end(), "-code-completion-at=/home/capfredf/tmp/hello_world.cpp:7:3");
  ClangArgv.insert(ClangArgv.end(), "-c");

  // Put a dummy C++ file on to ensure there's at least one compile job for the
  // driver to construct.
  ClangArgv.push_back("<<< inputs >>>");

  // Buffer diagnostics from argument parsing so that we can output them using a
  // well formed diagnostic object.
  llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new clang::DiagnosticIDs());
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts =
    clang::CreateAndPopulateDiagOpts(ClangArgv);
  clang::TextDiagnosticBuffer *DiagsBuffer = new clang::TextDiagnosticBuffer;
  clang::DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagsBuffer);

  clang::driver::Driver Driver(/*MainBinaryName=*/ClangArgv[0],
                        llvm::sys::getProcessTriple(), Diags);
  Driver.setCheckInputsExist(false); // the input comes from mem buffers
  llvm::ArrayRef<const char *> RF = llvm::ArrayRef(ClangArgv);
  std::unique_ptr<clang::driver::Compilation> Compilation(Driver.BuildCompilation(RF));

  if (Compilation->getArgs().hasArg(clang::driver::options::OPT_v))
    Compilation->getJobs().Print(llvm::errs(), "\n", /*Quote=*/false);

  auto res = GetCC1Arguments(&Diags, Compilation.get());
  if (auto err = res.takeError()) {
    return nullptr;
  }
  return MyCreateCI(**res);
}


// std::unique_ptr<clang::CompilerInvocation>
// buildCompilerInvocation(clang::DiagnosticConsumer &D, std::vector<const char *> &ArgStrs) {
//   const auto ClangArgV = createArgs(ArgStrs);
//   if (ClangArgV.size() == 0)
//     return nullptr;
//   // if (auto Err = OptClangArgV.takeError())
//   //   return nullptr;

//   clang::CreateInvocationOptions CIOpts;
//   // CIOpts.VFS = Inputs.TFS->view(Inputs.CompileCommand.Directory);
//   // CIOpts.CC1Args = {};
//   // CIOpts.RecoverOnError = true;
//   CIOpts.Diags =
//     clang::CompilerInstance::createDiagnostics(new clang::DiagnosticOptions, &D, false);
//   // CIOpts.ProbePrecompiled = false;
//   std::unique_ptr<clang::CompilerInvocation> CI = createInvocation(ArgStrs, CIOpts);
//   CI->getFrontendOpts().DisableFree = false;
//   CI->getLangOpts()->CommentOpts.ParseAllComments = true;
//   CI->getLangOpts()->RetainCommentsFromSystemHeaders = true;

//   disableUnsupportedOptions(*CI);
//   return CI;
// }


llvm::Expected<std::unique_ptr<clang::CompilerInstance>>
MyCreateCI(const llvm::opt::ArgStringList &Argv){
  std::unique_ptr<clang::CompilerInstance> Clang(new clang::CompilerInstance());
  clang::IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new clang::DiagnosticIDs());

  // Register the support for object-file-wrapped Clang modules.
  // FIXME: Clang should register these container operations automatically.
  // auto PCHOps = Clang->getPCHContainerOperations();
  // PCHOps->registerWriter(std::make_unique<ObjectFilePCHContainerWriter>());
  // PCHOps->registerReader(std::make_unique<ObjectFilePCHContainerReader>());

  // Buffer diagnostics from argument parsing so that we can output them using
  // a well formed diagnostic object.
  clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> DiagOpts = new clang::DiagnosticOptions();
  clang::TextDiagnosticBuffer *DiagsBuffer = new clang::TextDiagnosticBuffer;
  clang::DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagsBuffer);
  bool Success = clang::CompilerInvocation::CreateFromArgs(
      Clang->getInvocation(), llvm::ArrayRef(Argv.begin(), Argv.size()), Diags);

  // Infer the builtin include path if unspecified.
  if (Clang->getHeaderSearchOpts().UseBuiltinIncludes &&
      Clang->getHeaderSearchOpts().ResourceDir.empty())
    Clang->getHeaderSearchOpts().ResourceDir =
      clang::CompilerInvocation::GetResourcesPath(Argv[0], nullptr);

  // Create the actual diagnostics engine.
  Clang->createDiagnostics();
  if (!Clang->hasDiagnostics())
    return llvm::createStringError(llvm::errc::not_supported,
                                   "Initialization failed. "
                                   "Unable to create diagnostics engine");

  DiagsBuffer->FlushDiagnostics(Clang->getDiagnostics());
  if (!Success)
    return llvm::createStringError(llvm::errc::not_supported,
                                   "Initialization failed. "
                                   "Unable to flush diagnostics");

  // FIXME: Merge with CompilerInstance::ExecuteAction.

  Clang->setTarget(clang::TargetInfo::CreateTargetInfo(
      Clang->getDiagnostics(), Clang->getInvocation().TargetOpts));
  if (!Clang->hasTarget())
    return llvm::createStringError(llvm::errc::not_supported,
                                   "Initialization failed. "
                                   "Target is missing");

  Clang->getTarget().adjust(Clang->getDiagnostics(), Clang->getLangOpts());

  // Don't clear the AST before backend codegen since we do codegen multiple
  // times, reusing the same AST.
  Clang->getCodeGenOpts().ClearASTBeforeBackend = false;

  Clang->getFrontendOpts().DisableFree = false;
  Clang->getCodeGenOpts().DisableFree = false;

  return std::move(Clang);
}

static void LLVMErrorHandler(void *UserData, const char *Message,
                             bool GenCrashDiag) {
  auto &Diags = *static_cast<clang::DiagnosticsEngine *>(UserData);

  Diags.Report(clang::diag::err_fe_error_backend) << Message;

  // Run the interrupt handlers to make sure any special cleanups get done, in
  // particular that we remove files registered with RemoveFileOnSignal.
  llvm::sys::RunInterruptHandlers();

  // We cannot recover from llvm errors.  When reporting a fatal error, exit
  // with status 70 to generate crash diagnostics.  For BSD systems this is
  // defined as an internal software error. Otherwise, exit with status 1.

  exit(GenCrashDiag ? 70 : 1);
}


void capfredf_test(std::vector<const char *> &ArgStrs) {
  auto CConsumer = std::make_unique<clang::ReplCompletionConsumer>();
  clang::SyntaxOnlyAction Action;
  auto* FN = "/home/capfredf/tmp/hello_world.cpp";
  auto* DummyFN = "<<< inputs >>>";
  std::ifstream FInput(FN);
  std::stringstream InputStram;
  InputStram << FInput.rdbuf();
  std::string Content = InputStram.str();

  std::unique_ptr<llvm::MemoryBuffer> Buffer = llvm::MemoryBuffer::getMemBuffer(Content, FN);

  ArgStrs.push_back("-xc++");
  auto OptClang = createArgs(ArgStrs);
  // if (ClangArgV.size() == 0)
  //   return;

  // auto OptClang = CreateCI(ClangArgV);
  if (auto Err = OptClang.takeError()) {
    std::cout << "failed too" << "\n";
    return;
  }
  auto Clang = std::move(*OptClang);
  // Clang->LoadRequestedPlugins();
  Clang->getPreprocessorOpts().addRemappedFile(DummyFN, Buffer.get());
  Clang->getPreprocessorOpts().SingleFileParseMode = true;

  Clang->getLangOpts().SpellChecking = false;
  Clang->getLangOpts().DelayedTemplateParsing = false;

  auto &FrontendOpts = Clang->getFrontendOpts();
  FrontendOpts.CodeCompleteOpts = clang::getClangCompleteOpts();
  FrontendOpts.CodeCompletionAt.FileName = std::string(DummyFN);
  FrontendOpts.CodeCompletionAt.Line = 7;
  FrontendOpts.CodeCompletionAt.Column = 3;


  // Clang->setInvocation(std::move(CI));
  // Clang->createDiagnostics(&D, false);
  // Clang->getPreprocessorOpts().SingleFileParseMode = true;;
  Clang->setCodeCompletionConsumer(CConsumer.release());
  // Clang->setTarget(clang::TargetInfo::CreateTargetInfo(
  //     Clang->getDiagnostics(), Clang->getInvocation().TargetOpts));

  // llvm::install_fatal_error_handler(LLVMErrorHandler,
  //                                   static_cast<void *>(&Clang->getDiagnostics()));
  Buffer.release();

  Action.BeginSourceFile(*Clang, Clang->getFrontendOpts().Inputs[0]);
  if (llvm::Error Err = Action.Execute()) {
    std::cout << "failed" << "\n";
    return;
  }
  Action.EndSourceFile();
}
