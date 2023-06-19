#ifndef CAPFREDF_TEST_H
#define CAPFREDF_TEST_H
#include <vector>
#include "clang/Interpreter/Interpreter.h"
void capfredf_test(std::vector<const char *> &ArgStrs, std::string FN, size_t Line, size_t Col);
void capfredf_test2(clang::Interpreter &Interp, std::string FN, size_t Line, size_t Col);
llvm::Expected<bool> capfredf_test3(clang::IncrementalCompilerBuilder& CB, std::string FN, size_t Line, size_t Col);
#endif
