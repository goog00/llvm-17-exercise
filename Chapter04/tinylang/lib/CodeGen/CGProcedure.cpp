
#include "tinylang/CodeGen/CGProcedure.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Casting.h"

using namespace tinylang;




void *CGProcedure::writeLocalVariable(llvm::BasicBlock *BB,Decl *Decl,llvm::Value *Val){
    assert(BB && "Basic block is nullptr");

    assert((llvm::isa<VariableDeclaration>(Decl) ||
            llvm::isa<FormalParameterDeclaration>(Decl)) &&
            "Declaration must be variable or formal parameter");

    assert(Val && "Value is nullptr");
    CurrentDef[BB].Defs[Decl] = Val;

   

}

llvm::Value * CGProcedure::readLocalVariabe(llvm::BasicBlock *BB, Decl *Decl){
    assert(BB && "Basic block is nullptr");
    assert((llvm::isa<VariableDeclaration>(Decl) || 
    llvm::isa<FormalParameterDeclaration>(Decl)) &&
    "Declaration must be variable or formal paramater");
    auto Val = CurrentDef[BB].Defs.find(Decl);
    if(Val != CurrentDef[BB].Defs.end())
       return Val->second;
    return readLocalVariableRecursive(BB,Decl);   
}


llvm::Value *CGProcedure::readLocalVariableRecursive(llvm::BasicBlock *BB,Decl *Decl) {
    llvm::Value *Val = nullptr;
    if(!CurrentDef[BB].Sealed) {
        llvm::PHINode *Phi = addEmptyPhi(BB,Decl);
        CurrentDef[BB].IncompletePhis[Phi] = Decl;
        Val = Phi;
    } else if(auto *PredBB = BB->getSinglePredecessor()){
        Val = readLocalVariable(PredBB,Decl);
    } else {
        llvm::PHINode *Phi = addEmptyPhi(BB,Decl);
        writeLocalVariable(BB,Decl,Phi);
        Val = addPhiOperands(BB,Decl,Phi);
    } 
    writeLocalVariable(BB,Decl,Val);
    return Val;
}


llvm::PHINode * CGProcedure::addEmptyPhi(llvm::BasicBlock *BB, Decl *Decl) {
    return BB->empty() 
        ? llvm::PHINode::Create(mapType(Decl),0,"",BB)
        : llvm::PHINode::Create(mapType(Decl),0,"",&BB->front());

}


llvm::Value *CGProcedure::addPhiOperands(llvm::BasicBlock *BB, Decl *Decl,llvm::PHINode *Phi) {
    for(auto *PredBB : llvm::Predecessors(BB)) {
        Phi->addIncoming(readLocalVariable(PredBB,Decl),PredBB);
    }

    return optimizePhi(Phi);

}


llvm::Value * CGProcedure::optimizePhi(llvm::PHINode *Phi){
    llvm::Value *Same = nullptr;
    for(llvm::Value *V : Phi->incoming_values()){
        if(V == Same || V == Phi)
            continue;
        if(Same && V != Same){
            return Phi;
        }    
        Same = V;

    }

    if(Same == nullptr){
        Same = llvm::UndefValue::get(Phi->getType());
    }

    llvm::SmallVector<llvm::PHINode *,8> CandidatePhis;
    for(llvm::Use &U : Phi->uses()){
        if(auto *P = llvm::dyn_cast<llvm::PhiNode>(U.getUser()))
           if(P != Phi)
             CandidatePhis.push_back(P);
    }

    Phi->replaceAllUsesWith(Same);
    Phi->eraseFromParent();

    for(auto *P : CandidatePhi)
      optimizePhi(P);

    return Same;  
}



void CGProcedure::sealBlock(llvm::BasicBlock *BB) {
    assert(!CurrentDef[BB].Sealed && "Attempt to seal already sealed block");

    for(auto PhiDecl : CurrentDef[BB].IncompletePhis) {
        addPhiOperands(BB,PhiDecl.second,PhiDecl.first);
    }

    CurrentDef[BB].IncompletePhis.clear();
    CurrentDef[BB].Sealed = true;
}

void CGProcedure::writeVariable(llvm::BasicBlock *BB, Decl *D, llvm::Value *Val){
    if(auto *V = llvm::dyn_cast<VariableDeclaration>(D)) {
        if(V->getEncloseingDecl() == Proc)
             writeLocalVariable(BB,D,Val);
        else if(V->getEnclosingDecl() == CGm.getModuleDeclaration()) {
            Builder.CreateStore(Val, CGM.getGlobal(D));
        } else 
           llvm::report_fatal_error("Nested procedures not yet supported"); 
    } else if(auto *FP = llvm::dyn_cast<FormalParameterDeclaration>(D)){
        if(FP->isVar()) {
            Builder.CreateStore(Val,FormalParams[FP]);
        } else 
          writeLocalVariable(BB,D,Val);
    } else 
      llvm::report_fatal_error("Unsupported declaration");
}


llvm::Value *CGProcedure::readVariable(llvm::BasicBlock *BB, Decl *D){
    if(auto *V = llvm::dyn_cast<VariableDeclaration>(D)){
        if(V->getEnclosingDecl() == Proc){
            return readLocalVariable(BB,D);
        } else if(V->getEnclosingDecl() == CGM.getMoudlDeclaration()) {
            return Builder.CreateLoad(mapType(D),CGM.getGlobal(D));
        } else {
            llvm::report_fatal_error("Nested procedure not yet support");
        }   
    } else if(auto *FP = llvm::dyn_cast<FormalParameterDeclaration>(D)) {
        if(FP->isVar()) {
            return Builder.CreateLoad(mapType(FP,false),FormalParams[FP]);
        } else 
          return readLocalVariable(BB,D);
    } else 
       llvm::report_fatal_error("Unsupported declaration");
}


llvm::Type *CGProcedure::mapType(Decl *Decl, bool HonorReference){
    if(auto *FP = llvm::dyn_cast<FormalParameterDeclaration>(Decl)) {
        if(FP->isVar() && HonorReference)
           return llvm::PointerType::get(CGM.getLLVMCtx(),0);

        return CGM.converType(FP->getType());   
    }

    if(auto *V = llvm::dyn_cast<VariableDeclaration>(Decl))
       return CGM.convertType(V->getType());
    return CGM.convertType(llvm::cast<TypeDeclaration>(Decl));   
}


llvm::FunctionType *CGProcedure::createFunctionType(ProcedureDeclaration *Proc){
    llvm::Type *ResultTy = CGM.VoidTy;
    if(Proc->getRetType()){
        ResultTy = mapType(Proc->getRetType());
    }

    auto FormalParams = Proc->getFormalParams();
    llvm::SmallVector<llvm::Type *,8> ParamTypes;
    for(auto FP : FormalParams) {
        llvm::Type *Ty = mapType(FP);
        ParamTypes.push_back(Ty);
    }

    return llvm::FunctionType::get(ResultTy,ParamTypes,/*IsVarArgs=*/false);

}

llvm::Function * CGProcedure::createFunction(ProcedureDeclaration *Proc,llvm::FunctionType *FTy) {
    llvm::Function *Fn = llvm::Function::Create(
                    Fty,llvm::GlobalValue::ExternalLinkage,
                    CGM.mangleName(Proc),CGM.getModule());

    for(auto Pair : llvm::enumerate(Fn->args())){
        llvm::Argument &Arg = Pair.value();
        FormalParameterDeclaration *FP = Proc->getFormalParams()[Pair.index()];

        if(FP->isVar()) {
            llvm::AttrBuilder Attr(CGm.getLLVMCtx());
            llvm::TypeSize Sz = CGM.getModule()->getDataLayout().getTypeStoreSize(CGM.ConvertType(FP->getType()));

            Attr.addDereferenceableAttr(Sz);
            Attr.addAttribute(llvm::Attribute::NoCapture);
            Arg.addAttrs(Attr);

        }

        Arg.setName(FP->getName());
    }   

    return Fn;             
}




void CGProcedure::emitStmt(WhileStatement *Stmt){

    llvm::BasicBlock *WhileCondBB = llvm::BasicBlock::Create(CGM.getLLVMCtx(),"while.cond",Fn);

    llvm::BasicBlock *WhileBodyBB = llvm::BasicBlock::Create(CGM.getLLVMCtx(),"while.body",Fn);

    llvm::BasicBlock *AfterWhileBB = llvm::BasicBlock::Create(CGM.getLLVMCtx(),"after.while",Fn);


    Builder.CreateBr(WhileCondBB);
    sealBlock(Curr);

    setCurr(WhileCondBB);

    llvm::Value *Cond = emitExpr(Stmt->getCond());
    Builder.CreateCondBr(Cond,WhileBodyBB,AfterWhileBB);

    setCurr(WhileBodyBB);
    emit(Stmt->getWhileStmts());
    Builder.CreateBr(WhileCondBB);
    sealBlock(WhileCondBB);
    sealBlock(Curr);

    setCurr(AfterWhileBB);

}

void CGProcedure::emit(const StmtList &Stmts) {
    for(auto *S : Stmts) {
        if(auto *Stmt = llvm::dyn_cast<AssignmentStatement>(S))
          emitStmt(Stmt);

        else if (auto *Stmt = llvm::dyn_cast<ProcedureCallStatement>(S))

          emitStmt(Stmt);  

        else if(auto *Stmt = llvm::dyn_cast<IfStatement>(S))
          emitStmt(Stmt);

        else if(auto *Stmt = llvm::dyn_cast<WhileStatement>(S))
          emitStmt(Stmt);

        else if(auto *Stmt = llvm::dyn_cast<ReturnStatement>(S))
          emitStmt(Stmt);

        else 
          llvm_unreachable("unknown statement");        
    }
}


void CGProcedure::run(ProcedureDeclaration *Proc) {
    this->Proc = Proc;
    Fty = createFunctionType(Proc);
    Fn = createFunction(Proc,Fty);

    llvm::BasicBlock *BB = llvm::BasicBlock::Create(CGM.getLLVMCtx(),"entry",Fn);
    setCurr(BB);

    for(auto Pair : llvm::enumerate(Fn->args())) {
        llvm::Argument *Arg = &Pair.value();
        FormalParameterDeclaration *FP = Proc->getFormalParams()[Pair.index()];

        FormalParams[FP] = Arg;
        writeLocalVariable(Curr,FP,Arg);
    }

    for(auto *D : Proc->getDecls()) {
        if(auto *Var = llvm::dyn_cast<VariableDeclaration>(D)) {
            llvm::Type *Ty = mapType(Var);
            if(Ty->isAggregateType()) {
                llvm::Value *Val = Builder.CreateAlloca(Ty);
                writeLocalVariable(Curr,Var,Val);
            }
        }
    }

    auto Block = Proc->getStmts();
    emit(Proc->getStmts());
    if(!Curr->getTerminator()) {
        Builder.CreateRetVoid();
    }

    sealBlock(Curr);
}