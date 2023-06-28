#include "ExternalSource.h"
#include "clang/AST/ASTImporter.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/AST/DeclarationName.h"
#include <iostream>

namespace clang{
class InterpreterASTImporter : public ASTImporter {
private:
  ExternalSource &ESource;

public:
  InterpreterASTImporter(ASTContext &ToContext, FileManager &ToFileManager,
                         ASTContext &FromContext, FileManager &FromFileManager,
                         bool MinimalImport,
                         ExternalSource& ESource):
    ASTImporter(ToContext, ToFileManager, FromContext, FromFileManager,
                MinimalImport), ESource(ESource) {
  }
  virtual ~InterpreterASTImporter() = default;

  // void Imported(Decl *From, Decl *To) override {
  //   ASTImporter::Imported(From, To);

  //   if (clang::TagDecl* toTagDecl = dyn_cast<TagDecl>(To)) {
  //     toTagDecl->setHasExternalLexicalStorage();
  //     toTagDecl->setMustBuildLookupTable();
  //     toTagDecl->setHasExternalVisibleStorage();
  //   }
  //   if (NamespaceDecl *toNamespaceDecl = dyn_cast<NamespaceDecl>(To)) {
  //     toNamespaceDecl->setHasExternalVisibleStorage();
  //   }
  //   if (ObjCContainerDecl *toContainerDecl = dyn_cast<ObjCContainerDecl>(To)) {
  //     toContainerDecl->setHasExternalLexicalStorage();
  //     toContainerDecl->setHasExternalVisibleStorage();
  //   }
  //   // Put the name of the Decl imported with the
  //   // DeclarationName coming from the parent, in  my map.
  //   if (NamedDecl *toNamedDecl = llvm::dyn_cast<NamedDecl>(To)) {
  //     NamedDecl *fromNamedDecl = llvm::dyn_cast<NamedDecl>(From);
  //     m_Source.addToImportedDecls(toNamedDecl->getDeclName(),
  //                                 fromNamedDecl->getDeclName());
  //   }
  //   if (DeclContext *toDeclContext = llvm::dyn_cast<DeclContext>(To)) {
  //     DeclContext *fromDeclContext = llvm::dyn_cast<DeclContext>(From);
  //     m_Source.addToImportedDeclContexts(toDeclContext, fromDeclContext);
  //   }
  // }
};

ExternalSource::ExternalSource(ASTContext &ChildASTCtxt, FileManager &ChildFM, ASTContext &ParentASTCtxt, FileManager &ParentFM):
  ChildASTCtxt(ChildASTCtxt),
  ChildTUDeclCtxt(ChildASTCtxt.getTranslationUnitDecl()),
  ParentASTCtxt(ParentASTCtxt),
  ParentTUDeclCtxt(ParentASTCtxt.getTranslationUnitDecl()){
  InterpreterASTImporter* importer
    = new InterpreterASTImporter(ChildASTCtxt, ChildFM, ParentASTCtxt, ParentFM,
                           /*MinimalImport : ON*/ true, *this);
  Importer.reset(importer);
}

bool ExternalSource::FindExternalVisibleDeclsByName(const DeclContext *DC, DeclarationName Name) {
  IdentifierTable &ParentIdTable = ParentASTCtxt.Idents;

  auto ParentDeclName = DeclarationName(&(ParentIdTable.get(Name.getAsString())));

  DeclContext::lookup_result lookup_result =
    ParentTUDeclCtxt->lookup(ParentDeclName);

  if (!lookup_result.empty()) {
    std::cout << "\nFindExternalVisibleDeclsByName:\t" << Name.getAsString() <<"\n";
    return true;
  }
  return false;
}

void ExternalSource::completeVisibleDeclsMap(const DeclContext *ChildDeclContext) {
  assert (ChildDeclContext && ChildDeclContext == ChildTUDeclCtxt && "No child decl context!");

  if (!ChildDeclContext->hasExternalVisibleStorage())
    return;

  // for (auto IDeclContext =
  //        ParentTUDeclCtxt->decls_begin(),
  //        EDeclContext =
  //        ParentTUDeclCtxt->decls_end();
  //      IDeclContext != EDeclContext; ++IDeclContext)
  for (auto& IDeclContext : ParentTUDeclCtxt->decls()){
    if (NamedDecl* ParentDecl = llvm::dyn_cast<NamedDecl>(IDeclContext)) {
      DeclarationName ChildDeclName = ParentDecl->getDeclName();
      if (auto *II = ChildDeclName.getAsIdentifierInfo()) {
        // StringRef name = II->getName();
        if (auto DeclOrErr = Importer->Import(ParentDecl)) {
          if (NamedDecl *importedNamedDecl = llvm::dyn_cast<NamedDecl>(*DeclOrErr)) {
            SetExternalVisibleDeclsForName(ChildDeclContext,
                                           importedNamedDecl->getDeclName(),
                                           importedNamedDecl);
          }

        } else {
          llvm::consumeError(DeclOrErr.takeError());
        }
      }
    }
  }
  ChildDeclContext->setHasExternalLexicalStorage(false);
}

} //namespace clang
