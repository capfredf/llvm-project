#ifndef CAPFREDF_TEST_H
#define CAPFREDF_TEST_H
#include <vector>
#include "clang/Interpreter/Interpreter.h"
void capfredf_test(std::vector<const char *> &ArgStrs);
void capfredf_test2(clang::Interpreter &Interp);
#endif
