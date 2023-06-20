#include "clang/Interpreter/Interpreter.h"
#include "clang/Interpreter/CodeCompletion.h"

#include "llvm/LineEditor/LineEditor.h"

#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/Error.h"


#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace clang;
namespace {
auto CB = clang::IncrementalCompilerBuilder();

static std::unique_ptr<Interpreter> createInterpreter() {
  auto CI = cantFail(CB.CreateCpp());
  return cantFail(clang::Interpreter::create(std::move(CI)));
}

TEST(CodeCompletionTest, Sanity) {
  auto Interp = createInterpreter();
  Interp->startRecordingInput();
  if (auto R = Interp->ParseAndExecute("int foo = 12;")) {
    consumeError(std::move(R));
    return;
  }
  auto Completer = ReplListCompleter(CB, *Interp);
  std::vector<llvm::LineEditor::Completion> comps = Completer(std::string("f"), 1);
  EXPECT_EQ((size_t)1, comps.size());
  EXPECT_EQ(comps[0].TypedText, std::string("oo"));
}
} // anonymous namespace
