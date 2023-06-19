#include "Logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
using namespace clang;

void Logger::debug(const std::string& Text){
  auto now = std::chrono::system_clock::now();
  auto now_time = std::chrono::system_clock::to_time_t(now);
  out << "[" << std::put_time(std::localtime(&now_time), "%c") << "] ";
  out << Text << "\n";
  out.flush();
}
