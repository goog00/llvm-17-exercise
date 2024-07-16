#include "tinylang/CodeGen/CGModule.h"
#include "tinylang/CodeGen/CGProcedure.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"


using namespace tinylang;

void CGModule::initialize(){

    VoidTy = llvm::Type::getVoidTy(getLLVMCtx());
    Int1Ty = llvm::Type::getInt1Ty(getLLVMCtx());
    Int32Ty = llvm::Type::getInt32Ty(getLLVMCtx());
    Int64Ty = llvm::Type::getInt64Ty(getLLVMCtx());

    Int32Zero = llvm::ConstantInt::get(Int32Ty, 0, true);

}


llvm::Type *CGModule::convertType(TypeDeclaration *Ty){
    if(Ty->getName() == "INTEGER")
       return Int64Ty;
    if(Ty->getName() == "BOOLEAN")
       return Int1Ty;

    llvm::report_fatal_error("Unsuported type");     
}

std::sting CGModule::mangleName(Decl *D) {
    std::string Mangled("_t");
    llvm::SmallVector<llvm::StringRef,4> Parts;

    for(;D;D = D->getEnclosingDecl())
       Parts.push_back(D->getName());

    while(!Parts.empty()){
         llvm::StringRef Name = Parts.pop_back_val();
         Mangled.append(llvm::Twine(Name.size()).concat(Name).str());
    }  

    return Mangled; 
}



void CGModule::run(ModuleDeclaration *Mod) {
    for(auto *Decl : Mod->getDecls()) {
        if(auto *Var = llvm::dyn_cast<VariableDeclaration(Decl)>) {
            llvm::GlobalVariable *V = new llvm::GloabalVariable(*M, convertType(Var->getType()),
                                        false,
                                        llvm::GloabalValue::PrivateLinkage, nullptr,mangleName(Var)
                                        );
        Globals[Var] = V; 

        } else if (auot *Proc = llvm::dyn_cast<ProcedureDeclaration>(Decl)){
            CGProcedure CGP(*this);
            CGP.run(Proc);
        }
    }
}