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


std::unique_ptr<clang::CompilerInvocation>
buildCompilerInvocation(clang::DiagnosticConsumer &D, std::vector<const char *> &ArgStrs) {
  clang::CreateInvocationOptions CIOpts;
  // CIOpts.VFS = Inputs.TFS->view(Inputs.CompileCommand.Directory);
  // CIOpts.CC1Args = {};
  // CIOpts.RecoverOnError = true;
  // CIOpts.Diags =
  //   clang::CompilerInstance::createDiagnostics(new clang::DiagnosticOptions, &D, false);
  // CIOpts.ProbePrecompiled = false;
  std::unique_ptr<clang::CompilerInvocation> CI = createInvocation(ArgStrs, CIOpts);
  CI->getFrontendOpts().DisableFree = false;
  CI->getLangOpts()->CommentOpts.ParseAllComments = true;
  CI->getLangOpts()->RetainCommentsFromSystemHeaders = true;

  disableUnsupportedOptions(*CI);
  return CI;
}

void capfredf_test(std::vector<const char *> &ArgStrs) {
  auto CConsumer = std::make_unique<clang::ReplCompletionConsumer>();
  clang::SyntaxOnlyAction Action;
  auto FN = "/home/capfredf/tmp/hello_world.cpp";
  std::ifstream FInput(FN);
  std::stringstream InputStram;
  InputStram << FInput.rdbuf();
  std::string Content = InputStram.str();

  std::unique_ptr<llvm::MemoryBuffer> Buffer = llvm::MemoryBuffer::getMemBuffer(Content, FN);
  clang::DiagnosticConsumer D;

  std::string MainExecutableName =
      llvm::sys::fs::getMainExecutable(nullptr, nullptr);

  ArgStrs.insert(ArgStrs.begin(), MainExecutableName.c_str());

  ArgStrs.insert(ArgStrs.end(), "-c");
  ArgStrs.push_back("<<< inputs >>>");
  auto CI = buildCompilerInvocation(D, ArgStrs);

  auto &FrontendOpts = CI->getFrontendOpts();
  CI->getLangOpts()->SpellChecking = false;
  CI->getLangOpts()->DelayedTemplateParsing = false;
  // FrontendOpts.CodeCompleteOpts = clang::getClangCompleteOpts();
  // FrontendOpts.CodeCompletionAt.FileName = std::string(FN);
  // FrontendOpts.CodeCompletionAt.Line = 3;
  // FrontendOpts.CodeCompletionAt.Column = 3;

  CI->getPreprocessorOpts().addRemappedFile(CI->getFrontendOpts().Inputs[0].getFile(), Buffer.get());

  auto Clang = std::make_unique<clang::CompilerInstance>();
  Clang->setInvocation(std::move(CI));
  Clang->createDiagnostics(&D, false);
  Clang->getPreprocessorOpts().SingleFileParseMode = true;;
  Clang->setCodeCompletionConsumer(CConsumer.release());
  Clang->setTarget(clang::TargetInfo::CreateTargetInfo(
      Clang->getDiagnostics(), Clang->getInvocation().TargetOpts));

  Buffer.release();

  Action.BeginSourceFile(*Clang, Clang->getFrontendOpts().Inputs[0]);
  if (llvm::Error Err = Action.Execute()) {
    std::cout << "failed" << "\n";
    return;
  }
  Action.EndSourceFile();
}
