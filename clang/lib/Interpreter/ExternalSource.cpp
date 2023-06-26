#include "ExternalSource.h"
#include <iostream>
namespace clang{
ExternalSource::ExternalSource(ASTContext &parent, ASTContext &child) {

}

bool ExternalSource::FindExternalVisibleDeclsByName(const DeclContext *DC, DeclarationName Name) {
  std::cout << "hello world in FindExternalVisibleDeclsByName" << "\n";
  return false;
}
} //namespace clang
