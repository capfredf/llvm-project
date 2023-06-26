#include "clang/AST/ExternalASTSource.h"
#include "clang/AST/ASTContext.h"

namespace clang{
class ExternalSource : public clang::ExternalASTSource{
public:
  ExternalSource(ASTContext& Parent, ASTContext& Child);
  bool FindExternalVisibleDeclsByName (const DeclContext *DC, DeclarationName Name) override;
};
} //namespace clang
