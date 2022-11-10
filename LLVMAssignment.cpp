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
  std::map<Value*, std::set<Function*>> fMap;
  bool runOnModule(Module &M) override {
    Log("Hello: ");
    LogRef(M.getName()); Log('\n');
    // M.dump();
    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; fi++) {
      Function &f = *fi;
      if(!f.getName().startswith("llvm.dbg")) {
        visitFunction(&f, NULL, fMap);
      }
    }
    Log("============ANS============\n");
    disp_callLine();
    return false;
  }

  void visitFunction(Function* F, CallInst* parentInst, std::map<Value*, std::set<Function*>> &parentMap) {
    Log("visit topmap = " << &parentMap << "\n");
    Log("[" << F->getName() << "]......\n");
    if(parentInst) Log(*parentInst << "\n");
    std::queue<BasicBlock*> bbPool;        // unvisited bb list
    std::queue<std::map<Value*, std::set<Function*>>> bbMap;
    std::queue<BasicBlock*> bbParent;
    bbPool.push(&F->getEntryBlock());
    bbMap.push({});
    bbParent.push(NULL);
    std::map<Value*, std::set<Function*>> topMap;
    BasicBlock* parentbb;
    if(parentInst) {
      for(int i = 0; i < parentInst->getNumArgOperands(); i++) {
        Log(i << ": " << parentInst->getArgOperand(i) << " " << F->getArg(i) << "\n");
        if(Function* argFunc = dyn_cast<Function>(parentInst->getArgOperand(i)))
          insert2Map(bbMap.front(), argFunc, F->getArg(i));
        else
          mapCopy2(parentMap, parentInst->getArgOperand(i), bbMap.front(), F->getArg(i));
      }
    }
    while(!bbPool.empty()) {
      BasicBlock* bb = bbPool.front();
      bbPool.pop();
      topMap = bbMap.front();
      bbMap.pop();
      parentbb = bbParent.front();
      bbParent.pop();

      for(auto inst = bb->begin(); inst != bb->end(); inst++) {
        if(PHINode* phi = dyn_cast<PHINode>(inst)) {
          //TODO
          Log( "phi typeid=" << phi->getType()->getTypeID() << "\n");
          if(parentbb) {
            BasicBlock* inBB = parentbb;
            Value* inVal = phi->getIncomingValueForBlock(inBB);
            if(Function* inFunc = dyn_cast<Function>(inVal)) {
              insert2Map(topMap, inFunc, phi);
            } else {
              mapCopy(topMap, inVal, phi);
            }

          }
        } else if(CallInst* callInst = dyn_cast<CallInst>(inst)) {
          Function* callee = callInst->getCalledFunction();
          if (callee && !callee->getName().startswith("llvm.dbg")) {  // direct call
            Log("call " << callee->getName() << " " << callInst->getDebugLoc().getLine() << "\n");
            addCallLine(callInst->getDebugLoc().getLine(), callee);
            visitFunction(callee, callInst, topMap);
            Log("topmap = " << &topMap << "\n");
          } else if(!callee) {
            Value* calleeValue = callInst->getCalledValue();
            if(topMap.find(calleeValue) != topMap.end()) {
              for (Function* f : topMap[calleeValue]) {
                Log("abss " << topMap[calleeValue].size() << " " << f << " " << callInst << " " << calleeValue<< "\n");
                for(Function* tmp : topMap[calleeValue]) Log(tmp->getName() << " ");
                Log("\n");
                Log("call " << f->getName() << " " << callInst->getDebugLoc().getLine() << "\n");
                addCallLine(callInst->getDebugLoc().getLine(), f);
                Log("topmap = " << &topMap << "\n");
                visitFunction(f, callInst, topMap);
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
            bbMap.push(topMap);
            bbParent.push(bb);
          }
        } else if (ReturnInst* retInst = dyn_cast<ReturnInst>(inst)) {
          if(parentInst) {
            Log("ret " << parentInst << " " << retInst->getReturnValue() << "\n" << *inst << "\n");
            mapCopy2(topMap, retInst->getReturnValue(), parentMap, parentInst);
          }
          Log("[finish " << F->getName() << "]\n");
        }
      }
    }
  }
  void insert2Map(std::map<Value*, std::set<Function*>> &map, Value* funVal, Value* val) {
    if(Function* f = dyn_cast<Function>(funVal)) {
      Log("insert " << f->getName() << " " << f << " to " << val << " in " << &map << "\n");
      if(map.find(val) == map.end()) {
        map[val] = std::set<Function*> {f};
      } else {
        map[val].insert(f);
      }
    }
  }

  void mapCopy(std::map<Value*, std::set<Function*>> &map, Value* oldVal, Value* newVal) {
    if(map.find(oldVal) != map.end()){
      for(Function* func : map[oldVal]) {
        Log("copy " << func->getName() << " from " << oldVal << " to " << newVal << "\n");
        insert2Map(map, func, newVal);
      }
    }
  }

  void mapCopy2(std::map<Value*, std::set<Function*>> &oldMap, Value* oldVal, std::map<Value*, std::set<Function*>> &newMap, Value* newVal) {
    Log((oldMap.find(oldVal) == oldMap.end()) << " oldmap  " << &oldMap << " oldVal " << oldVal << " new " << &newMap<<  "\n");
    if(oldMap.find(oldVal) != oldMap.end()){
      for(Function* func : oldMap[oldVal]) {
        Log("copy2 " << func->getName() << " from " << oldVal << " to " << newVal << "\n");
        insert2Map(newMap, func, newVal);
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
      int is_start = 1;
      for(Function* func : pair->second) {
        if(!is_start) errs() << ", ";
        else is_start = 0;
        errs() << func->getName();
      }
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

