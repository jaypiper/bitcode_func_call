//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include <llvm/Support/CommandLine.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/ToolOutputFile.h>

#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils.h>

#include <llvm/IR/Function.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Instruction.h>

#include <iostream>
#include <queue>

#define LOCAL_DEBUG

#ifdef LOCAL_DEBUG
  #define Log(...) errs() << __VA_ARGS__
  #define LogRef(...) errs().write_escaped(__VA_ARGS__)
#else
  #define Log(...)
  #define LogRef(...)
#endif


using namespace llvm;
static ManagedStatic<LLVMContext> GlobalContext;
static LLVMContext &getGlobalContext() { return *GlobalContext; }
/* In LLVM 5.0, when  -O0 passed to clang , the functions generated with clang will
 * have optnone attribute which would lead to some transform passes disabled, like mem2reg.
 */
struct EnableFunctionOptPass: public FunctionPass {
    static char ID;
    EnableFunctionOptPass():FunctionPass(ID){}
    bool runOnFunction(Function & F) override{
        if(F.hasFnAttribute(Attribute::OptimizeNone))
        {
            F.removeFnAttr(Attribute::OptimizeNone);
        }
        return true;
    }
};

char EnableFunctionOptPass::ID=0;

	
///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
///Updated 11/10/2017 by fargo: make all functions
///processed by mem2reg before this pass.
struct FuncPtrPass : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  FuncPtrPass() : ModulePass(ID) {}
  std::map<Value*, std::set<Function*>> FuncMap;
  std::map<int, std::set<Function*>> CallLine;
  bool runOnModule(Module &M) override {
    Log("Hello: ");
    LogRef(M.getName()); Log('\n');
    // M.dump();
    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; fi++) {
      Function &f = *fi;
      if(!f.getName().startswith("llvm.dbg")) {
        visitFunction(&f, NULL);
      }
    }
    Log("============ANS============\n");
    disp_callLine();
    return false;
  }

  void visitFunction(Function* F, CallInst* parentInst) {
    std::map<Value*, std::set<Function*>> fMap;
    std::queue<BasicBlock*> bbPool;        // unvisited bb list
    bbPool.push(&F->getEntryBlock());
    if(parentInst) {
      // TODO: add args to fmap
    }
    while(!bbPool.empty()) {
      BasicBlock* bb = bbPool.front();
      bbPool.pop();
      for(auto inst = bb->begin(); inst != bb->end(); inst++) {
        if(PHINode* phi = dyn_cast<PHINode>(inst)) {
          //TODO
          Log( "phi typeid=" << phi->getType()->getTypeID() << "\n");
          for(BasicBlock** inBBPtr = phi->block_begin(); inBBPtr != phi->block_end(); inBBPtr++) {
            BasicBlock* inBB = *inBBPtr;
            Log(*(inBB->getFirstNonPHI()) << "\n");
            Value* inVal = phi->getIncomingValueForBlock(inBB);
            if(Function* inFunc = dyn_cast<Function>(inVal)) {
              insert2Map(fMap, inFunc, phi);
              Log("phi " << phi << " " << inFunc->getName() << "\n");
            } else if(fMap.find(inVal) != fMap.end()){
              for(Function* func : fMap[inVal]) insert2Map(fMap, func, phi);
              Log("phi " << phi << "  ..\n");
            }

          }
        } else if(CallInst* callInst = dyn_cast<CallInst>(inst)) {
          Function* callee = callInst->getCalledFunction();
          if (callee && !callee->getName().startswith("llvm.dbg")) {  // direct call
            Log("call " << callee->getName() << " " << callInst->getDebugLoc().getLine() << "\n");
            addCallLine(callInst->getDebugLoc().getLine(), callee);
            visitFunction(callee, callInst);
          } else if(!callee) {
            Value* calleeValue = callInst->getCalledValue();
            Log("callee " << calleeValue << "\n");
            if(fMap.find(calleeValue) != fMap.end()) {
              for (Function* f : fMap[calleeValue]) {
                Log("call " << f->getName() << " " << callInst->getDebugLoc().getLine() << "\n");
                addCallLine(callInst->getDebugLoc().getLine(), f);
                visitFunction(f, callInst);
              }

            }
          }
        } else if (BranchInst * branchInst = dyn_cast<BranchInst>(inst)) {
          if(branchInst->isConditional()) {
            Value* cond = branchInst->getCondition();
            if (ICmpInst * cmpInst = dyn_cast<ICmpInst>(cond)) {
              // TODO: handle unreachable branch
            }

          } else {
            // TDOO
          }
          for (BasicBlock *succ: branchInst->successors()) {
            bbPool.push(succ);
          }
        } else {

        }
      }
    }
  }
  void insert2Map(std::map<Value*, std::set<Function*>> &map, Value* funVal, Value* val) {
    if(Function* f = dyn_cast<Function>(funVal)) {
      if(map.find(val) == map.end()) {
        map[val] = std::set<Function*> {f};
      } else {
        map[val].insert(f);
      }
    }
  }

  void addCallLine(int line, Function* func) {
    if(CallLine.find(line) == CallLine.end()) {
      CallLine[line] = std::set<Function*> {func};
    } else {
      CallLine[line].insert(func);
    }
  }

  void disp_callLine() {
    for(auto pair = CallLine.begin(); pair != CallLine.end(); pair ++) {
      errs() << pair->first << " : ";
      for(Function* func : pair->second) errs() << func->getName();
      errs() << "\n";
    }
  }
};

#if 0
struct FuncPass: public FunctionPass {
  static char ID; // Pass identification, replacement for typeid
  std::map<Value*, std::set<Function*>> FuncMap;
  FuncPass() : FunctionPass(ID) {}
  bool runOnFunction(Function &F) override {
    for (inst_iterator i = inst_begin(F), ie = inst_end(F); i != ie; i++) {
      Instruction* I = &*i;
      CallInst* callInst = dyn_cast<CallInst>(I);
      for(Use* usr = I->op_begin(); usr != I->op_end(); usr++)
        errs() << "[" <<usr->get()->getName() << "(" << usr->get()->getType()->getTypeID() << "," << usr->get()->getType()->isPointerTy() << ")" << "] ";
      if(PHINode* phi = dyn_cast<PHINode>(I)) {
      }
      if(callInst){
        if(callInst->getCalledFunction()){
          std::string callFuncName = callInst->getCalledFunction()->getName().str();
          if(callFuncName != "llvm.dbg.value") {
            errs() << callInst->getDebugLoc().getLine() << " : " << callFuncName << "\n";
          }
        }

      }
    }
    return false;
  }
  void getPotentialVal(CallInst* inst) {
    BasicBlock* bb = inst->getParent();
  }
};
#endif

char FuncPtrPass::ID = 0;
// char FuncPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass", "Print function call instruction");

static cl::opt<std::string>
InputFilename(cl::Positional,
              cl::desc("<filename>.bc"),
              cl::init(""));


int main(int argc, char **argv) {
  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err;
  // Parse the command line to read the Inputfilename
  cl::ParseCommandLineOptions(argc, argv,
                            "FuncPtrPass \n My first LLVM too which does not do much.\n");

  // Load the input module
  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  llvm::legacy::PassManager Passes;

  ///Remove functions' optnone attribute in LLVM5.0
  Passes.add(new EnableFunctionOptPass());
  ///Transform it to SSA
  Passes.add(llvm::createPromoteMemoryToRegisterPass());

  /// Your pass to print Function and Call Instructions
  Passes.add(new FuncPtrPass());
  // Passes.add(new FuncPass());
  Passes.run(*M.get());
}

