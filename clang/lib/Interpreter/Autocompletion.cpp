#include "llvm/LineEditor/LineEditor.h"

namespace clang{
struct GlobalEnv{
  std::vector<llvm::StringRef> names;

  void extend(llvm::StringRef name) {
    names.push_back(name);
  }
};


struct ReplListCompleter {

  GlobalEnv &env;
  ReplListCompleter(GlobalEnv &env) : env(env) {}
  std::vector<llvm::LineEditor::Completion> operator()(llvm::StringRef Buffer,
                                                       size_t Pos) const{
    std::vector<llvm::LineEditor::Completion> Comps;
    // first wew look for a space
    // if space is not found from right, then use the whole typed string
    // Otherwise, use Buffer[found_idx:] to search for completion candidates.
    size_t space_pos = Buffer.rfind(" ");
    llvm::StringRef s;
    if (space_pos == llvm::StringRef::npos) {
      s = Buffer;
    } else {
      s = Buffer.substr(space_pos + 1);
    }

    // if s is empty, return an empty vector;
    if (s.empty()) {
      return Comps;
    }

    for (auto c : env.names) {
      if (c.startswith(s)) {
        Comps.push_back(llvm::LineEditor::Completion(c.substr(s.size()).str(), c.str()));
      }
    }
    return Comps;
  }
};

}
