#pragma once
#include <fstream>

namespace clang{
class Logger {
  std::ofstream out;
public:
  Logger(std::string FN) : out(FN, std::ios_base::out | std::ios_base::trunc) {};
  ~Logger() {
    out.close();
  }
  void debug(const std::string& Text);
};
} // namespace clang
