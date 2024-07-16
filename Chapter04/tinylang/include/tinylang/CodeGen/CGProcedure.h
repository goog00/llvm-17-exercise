#ifndef TINYLANG_CODEGEN_CGPROCEDURE_H
#define TINYLANG_CODEGEN_CGPROCEDURE_H

#include "tinylang/AST/AST.h"
#include "tinylang/CodeGen/CGModule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"



namespace llvm {
    class Function;
}


namespace tinylang {

namespace CGProcedure {
    CGModule &CGM;

    llvm::IRBuilder<> Builder;

    llvm::BasicBlock *Curr;
    


    llvm::Function *Fn;


    struct BasicBlockDef {

        llvm::DenseMap<Decl *,llvm::TrackingVH<llvm::Value>> Defs;

        llvm::DenseMap<llvm::PHINode *,Decl *> IncompletePhis;

        unsigned Sealed : 1;

        BasicBlockDef() : Sealed(0) {}
    };

    llvm::DenseMap<llvm::BasicBlock *, BasicBlockDef> CurrentDef;

    void writeLocalVariable(llvm::BasicBlock *BB,Decl *Decl,llvm::Value *Val);

    llvm::Value *readLocalVariable(llvm::BasicBlock *BB,Decl *Decl);

    llvm::Value *readLocalVariableRecursive(llvm::BasicBlock *BB, Decl *Decl);


    void sealBlock(llvm::BasicBlock *BB);


    llvm::FunctionType *createFunctionType(ProcedureDeclaration *Proc);
    llvm::Function *createFunction(ProcedureDeclaration *Proc,llvm::FunctionType *FTy);





protected:
    
    void setCurr(llvm::BasicBlock *BB) {
        Curr = BB;
        Builder.SetInsertPoint(Curr);
    }

    void emitStmt(WhileStatement *Stmt);

    void emit(const StmtList &Stmts);



public:
   CGProcedure(CGModule &CGM) :  CGM(CGM), Builder(CGM.getLLVMCtx()),Curr(nullptr){};

   void run(ProcedureDeclaration *Proc);

};
}

#endif