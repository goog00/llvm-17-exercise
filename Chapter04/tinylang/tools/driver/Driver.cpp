#include "tinylang/Basic/Diagnostic.h"
#include "tinylang/Basic/Version.h"
#include "tinylang/Parser/Parser.h"
#include "tinylang/CodeGen/CodeGenerator.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include "llvm/TargetParser/Host.h"

using namespace llvm;
using namespace tinylang;

static codegen::RegisterCodeGenFlags CGF;

static llvm::cl::opt<std::string> InputFile(llvm::Positional,
                              llvm::cl::desc("input-file"),
                              cl::init("-"));

static llvm::cl::opt<std::string> MTriple("mtriple",
                llvm::cl::desc("Override target triple for module"));   

static llvm::cl::opt<bool> EmitLLVM("emit-llvm",
                llvm::cl::desc("Emit IR code instead of assembler"),
                llvm::cl::init(false));

llvm::TargetMachine *
createTargetMachine(const char *Argv0){
  llvm::Triple Triple = llvm::Triple(
              !MTriple.empty() 
                ? llvm::Triple::normalize(MTriple)
                : llvm::sys::getDefaultTargetTriple());

  llvm::TargetOptions TargetOptions = codegen::InitTargetOptionsFromCodeGenFlags(Triple);
  std::string CPUStr = codegen::getCPUStr();
  std::string FeatureStr = codegen::GetFeaturesStr();

  std::string Error;
  const llvm::Target *Target = 
         llvm::TargetRegistry::lookupTarget(codegen::getMArch(),Triple,Error);

   if(!Target) {
    llvm::WithColor::error(llvm::errs(),Argv0) << Error;
    return nullptr;
   } 

  llvm::TargetMachine *TM = Target->createTargetMachine(Triple.getTriple(),
                CPUStr,FeatureStr,TargetOptions,
                std::optional<llvm::Reloc::Model>(codegen::getRelocModel()));
  return TM;             
}


bool emit(StringRef Argv0,llvm::Module *M, llvm::TargetMachine *TM) {
  CodeGenerator FileType = codegen::getFileType();
  if(OutputFilename.empty()){
    if(InputFilename == "-"){
      OutputFilename = "-";
    } else {
        if(InputFilename.endswith(".mod"))
            OutputFilename = InputFilename.drop_back(4).str();
    }
  }
}

int main(int argc_, const char **argv_) {
  llvm::InitLLVM X(argc_, argv_);
  llvm::SmallVector<const char *, 256> argv(argv_ + 1,
                                            argv_ + argc_);

  llvm::outs() << "Tinylang "
               << tinylang::getTinylangVersion() << "\n";

  for (const char *F : argv) {
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>>
        FileOrErr = llvm::MemoryBuffer::getFile(F);
    if (std::error_code BufferError =
            FileOrErr.getError()) {
      llvm::errs() << "Error reading " << F << ": "
                   << BufferError.message() << "\n";
      continue;
    }

    llvm::SourceMgr SrcMgr;
    DiagnosticsEngine Diags(SrcMgr);

    // Tell SrcMgr about this buffer, which is what the
    // parser will pick up.
    SrcMgr.AddNewSourceBuffer(std::move(*FileOrErr),
                              llvm::SMLoc());

    auto TheLexer = Lexer(SrcMgr, Diags);
    auto TheSema = Sema(Diags);
    auto TheParser = Parser(TheLexer, TheSema);
    TheParser.parse();
  }
}