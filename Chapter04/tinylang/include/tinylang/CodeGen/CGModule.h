#ifndef TINYLANG_CODEGEN_CGMODULE_H
#define TINYLANG_CODEGEN_CGMODULE_H

#include "tinylang/AST/AST.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

namespace tinylang {

class CGModule {

    llvm::Module *M;
    ModuleDeclaration *Mod;

    llvm::DenseMap<Decl *,llvm::GlobalObject *> Globals;

public:
    llvm::Type *VoidTy;
    llvm::Type *Int1Ty;
    llvm::Type *Int32Ty;
    llvm::Type *Int64Ty;
    llvm::Constant *Int32Zero;

public:
    CGModule(llvm::Module *M) : M(M) {initialize();}
    void initialize();

    llvm::LLVMContext &getLLVMCtx() {return M->getContext();}    
    llvm::Module *getModule() {return M;}

    llvm::Type *convertType(TypeDeclaration *Ty);
    std::string mangleName(Decl *D);
 

    void run(ModuleDeclaration *Mod);

}

}



#endif