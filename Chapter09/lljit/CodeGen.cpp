#include "CodeGen.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class ToIRVisitor : public ASTVisitor {
    Module *M;
    IRBuilder<> Builder;
    StringMap<size_t> &JITtedFunctionsMap;
    Type *Int32Ty;

    Value *V;
    StringMap<Value *> nameMap;

public:
    ToIRVisitor(Module *M, 
                StringMap<size_t> &JITtedFunctions)
                : M(M), Builder(M->getContext()),JITtedFunctionsMap(JITtedFunctions) {

        Int32Ty = Type::getInt32Ty(M->getContext());
    }

    void genFuncEvaluationCall(AST *Tree) { Tree->accept(*this);}

    void run(AST *Tree) {
        Tree->accept(*this);
        Builder.CreateRet(V);
    }

    Function *genUserDefinedFunction(llvm::StringRef FnName) {

        if(Function *FuncFromModule = M->getFunction(FnName))
          return FuncFromModule;

        Function *UserDefinedFunction = nullptr;
        auto FnNameToArgCount = JITtedFunctionsMap.find(FnName);

        if(FnNameToArgCount != JITtedFunctionsMap.end() ) {
            std::vector<Type *> IntArgs(FnNameToArgCount->second,Int32Ty);
            FunctionType *FuncType = FunctionType::get(Int32Ty,IntArgs,false);
            UserDefinedFunction = Function::Create(FuncType, GlobalValue::ExternalLinage,FnName,M);
        } 
        return UserDefinedFunction;
    }

    virtual void visit(Factor &Node) override {
        if (Node.getKind() == Factor::Ident) {
            V = nameMap[Node.getVal()];

        } else {
            int intval;
            Node.getVal().getAsInteger(10,intval);
            V = ConstantInt::get(Int32Ty, intval, true);
        }
    }


    virtual void visit(DefDecl &Node) override {
        llvm::StringRef FnName = Node.getFnName();
        llvm::SmallVector<llvm::StringRef, 8> FunctionVars = Node.getVars();

        (JITtedFunctionsMap)[FnName] = FunctionVars.size();

        Function *DefFunc = genUserDefinedFunction(FnName);
        if(!DefFunc) {
            llvm::errs() << "";
            return;
        }

        BasicBlock *BB = BasicBlock::Create(M->getContext(),"entry",DefFunc);
        
        Builder.SetInsertPoint(BB);

        unsigned FIdx = 0;
        for (auto &FArg : DefFunc->args()) {
            nameMap[FunctionVars[FIdx]] = &FArg;
            FArg.setName(FunctionVars[FIdx++]);
        }
        
        Node.getExpr()->accept(*this);
        

    }


    virtual void visit(FuncCallFromDef &Node) override {
        llvm::StringRef CalcExprFunName = "calc_expr_func";

        FunctionType *CalcExprFunTy = FunctionType::get(Int32Ty, {}, false);
        Function *CalcExprFun = Function::Create(CalcExprFunTy, GlobalValue::ExternalLinkage, CalcEprFunName, M);

        BasicBlock *BB = BasicBlock::Create(M->getContext(),"entry", CalcExprFun);
        Builder.SetInsertPoint(BB);

        llvm::StringRef CalleeFnName = Node.getFnName();
        Function *CalleeFn = genUserDefinedFunction(CalleeFnName);

        if(!CalleeFn){
            llvm::errs() << "Cannot retrieve the original callee function!\n";
            return;
        }

        auto CalleeFnVars = Node.getArgs();
        llvm::SmallVector<Value *> IntParams;

        for(unsigned i = 0, end = CalleeFnVars.size(); i != end; ++i){
            int ArgsToIntType;
            CalleeFnVars[i].getAsInteger(10, ArgsToIntType);
            Value *IntParam = ConstantInt::get(Int32Ty, ArgsToIntType, true);
            IntParams.push_back(IntParam);

        }

        Value *Res = Builder.CreateCall(CalleeFn,IntParams, "calc_expr_res");
        Builder.CreateRet(Res);
    };

};    
}//namespace


void CodeGen::compileToIR(AST *Tree, Module *M,StringMap<size_t> &JITtedFunctions) {
    ToIRVisitor ToIR(M,JITtedFunctions);

    ToIR.run(Tree);
    M->print(outs(),nullptr);

}

void CodeGen::prepareCalculationCallFunc(AST *FuncCall, Module *M, llvm::StringRef FnName, StringMap<size_t> &JITtedFunctions) {
    ToIRVisitor ToIR(M, JITtedFunctions);
    ToIR.genFuncEvaluationCall(FuncCall);
    M->print(outs(),nullptr);
}