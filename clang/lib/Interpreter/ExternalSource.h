#include "clang/AST/ExternalASTSource.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/FileManager.h"

namespace clang{
class InterpreterASTImporter;
class ExternalSource : public clang::ExternalASTSource{
  ASTContext &ChildASTCtxt;
  DeclContext* ChildTUDeclCtxt;
  ASTContext &ParentASTCtxt;
  DeclContext* ParentTUDeclCtxt;

  std::unique_ptr<clang::InterpreterASTImporter> Importer;

public:
  ExternalSource(ASTContext &ChildASTCtxt, FileManager &ChildFM, ASTContext &ParentASTCtxt, FileManager &ParentFM);
  bool FindExternalVisibleDeclsByName (const DeclContext *DC, DeclarationName Name) override;
  void completeVisibleDeclsMap(const clang::DeclContext *childDeclContext) override;
};
} //namespace clang
