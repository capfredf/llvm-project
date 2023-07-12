#include "clang/Interpreter/CodeCompletion.h"
#include "clang/Interpreter/Interpreter.h"

#include "clang/Frontend/CompilerInstance.h"
#include "llvm/LineEditor/LineEditor.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

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
  if (auto R = Interp->ParseAndExecute("int foo = 12;")) {
    consumeError(std::move(R));
    return;
  }
  auto Completer = ReplListCompleter(CB, *Interp);
  std::vector<llvm::LineEditor::Completion> comps =
      Completer(std::string("f"), 1);
  EXPECT_EQ((size_t)2, comps.size()); // foo and float
  EXPECT_EQ(comps[0].TypedText, std::string("oo"));
}

TEST(CodeCompletionTest, SanityNoneValid) {
  auto Interp = createInterpreter();
  if (auto R = Interp->ParseAndExecute("int foo = 12;")) {
    consumeError(std::move(R));
    return;
  }
  auto Completer = ReplListCompleter(CB, *Interp);
  std::vector<llvm::LineEditor::Completion> comps =
      Completer(std::string("babanana"), 8);
  EXPECT_EQ((size_t)0, comps.size()); // foo and float
}

TEST(CodeCompletionTest, TwoDecls) {
  auto Interp = createInterpreter();
  if (auto R = Interp->ParseAndExecute("int application = 12;")) {
    consumeError(std::move(R));
    return;
  }
  if (auto R = Interp->ParseAndExecute("int apple = 12;")) {
    consumeError(std::move(R));
    return;
  }
  auto Completer = ReplListCompleter(CB, *Interp);
  std::vector<llvm::LineEditor::Completion> comps =
      Completer(std::string("app"), 3);
  EXPECT_EQ((size_t)2, comps.size());
}

TEST(CodeCompletionTest, CompFunDeclsNoError) {
  auto Interp = createInterpreter();
  auto Completer = ReplListCompleter(CB, *Interp);
  auto Err = llvm::Error::success();
  std::vector<llvm::LineEditor::Completion> comps =
      Completer(std::string("void app("), 9, Err);
  EXPECT_EQ((bool)Err, false);
}

TEST(CodeCompletionTest, TempTypedDirected) {
  auto Interp = createInterpreter();
  if (auto R = Interp->ParseAndExecute("int application = 12;")) {
    consumeError(std::move(R));
    return;
  }
  if (auto R = Interp->ParseAndExecute("char apple = '2';")) {
    consumeError(std::move(R));
    return;
  }
  if (auto R = Interp->ParseAndExecute("void add(int &SomeInt){}")) {
    consumeError(std::move(R));
    return;
  }
  auto Completer = ReplListCompleter(CB, *Interp);
  std::vector<llvm::LineEditor::Completion> comps =
      Completer(std::string("add("), 3);
  EXPECT_EQ((size_t)1, comps.size());
}
} // anonymous namespace
