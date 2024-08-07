#include "tinylang/CodeGen/CGModule.h"
#include "tinylang/CodeGen/CGProcedure.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"

using namespace tinylang;

void CGModule::initialize() {
  VoidTy = llvm::Type::getVoidTy(getLLVMCtx());
  Int1Ty = llvm::Type::getInt1Ty(getLLVMCtx());
  Int32Ty = llvm::Type::getInt32Ty(getLLVMCtx());
  Int64Ty = llvm::Type::getInt64Ty(getLLVMCtx());
  Int32Zero =
      llvm::ConstantInt::get(Int32Ty, 0, /*isSigned*/ true);
}

llvm::Type *CGModule::convertType(TypeDeclaration *Ty) {
   if(llvm::Type *T = TypeCache[Ty]){
    return T;
   }

   if(llvm::isa<PervasiveTypeDeclaration>(Ty)){
      if(Ty->getName() == "INTEGER")
        return Int64Ty;
      if(Ty->getName() == "BOOLEAN")
        return Int1Ty;

   } else if(auto *AliasTy = llvm::dyn_cast<AliasTypeDeclaration>(Ty)){
    
      llvm::Type *T = convertType(AliasTy->getType());
      return TypeCache[Ty] = T;

   } else if(auto * ArrayTy = llvm::dyn_cast<ArrayTypeDeclaration>(Ty)) {

    llvm::Type *Component = convertType(ArrayTy->getType());

    Expr *Nums = ArrayTy->getNums();
    assert(llvm::cast<IntegerLiteral>(Nums) && "Expected an integer literal");

    uint64_t NumElements = llvm::cast<IntegerLiteral>(Nums)->getValue().getZExtValue();

    llvm::Type *T = 
          llvm::ArrayType::get(Component,NumElements);

    return TypeCache[Ty] = T;      
   } else if (auto *RecordTy = llvm::dyn_cast<RecordTypeDeclaration>(Ty)){
     llvm::SmallVector<llvm::Type *,4> Elements;
     for(const auto &F : RecordTy->getFields()){
      Elements.push_back(convertType(F.getType()));
     }

     llvm:Type *T = llvm::StructType::create(
          Elements,RecordTy->getName(),false
     );

     return TypeCache[Ty] = T;
   }

   llvm::report_fatal_error("Unsupport type");
}

std::string CGModule::mangleName(Decl *D) {
  std::string Mangled("_t");
  llvm::SmallVector<llvm::StringRef, 4> Parts;
  for (; D; D = D->getEnclosingDecl())
    Parts.push_back(D->getName());
  while (!Parts.empty()) {
    llvm::StringRef Name = Parts.pop_back_val();
    Mangled.append(
        llvm::Twine(Name.size()).concat(Name).str());
  }
  return Mangled;
}

llvm::GlobalObject *CGModule::getGlobal(Decl *D) {
  return Globals[D];
}

void CGModule::run(ModuleDeclaration *Mod) {
  for (auto *Decl : Mod->getDecls()) {
    if (auto *Var =
            llvm::dyn_cast<VariableDeclaration>(Decl)) {
      // Create global variables
      llvm::GlobalVariable *V = new llvm::GlobalVariable(
          *M, convertType(Var->getType()),
          /*isConstant=*/false,
          llvm::GlobalValue::PrivateLinkage, nullptr,
          mangleName(Var));
      Globals[Var] = V;
    } else if (auto *Proc =
                   llvm::dyn_cast<ProcedureDeclaration>(
                       Decl)) {
      CGProcedure CGP(*this);
      CGP.run(Proc);
    }
  }
}
