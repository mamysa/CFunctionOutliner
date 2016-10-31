#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/RegionPass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/DenseSet.h"
#include <fstream>
#include <vector>
#include <deque>
#include <string>



using namespace llvm;

static cl::opt<std::string> BBListFilename("bblist", 
	   		cl::desc("List of blocks' labels that are to be extracted. Must form a valid region."), 
	   		cl::value_desc("filename"), cl::Required  );

namespace {

	//@return - list of basic block labels
	static std::vector<std::string> readBBListFile(const std::string& filename) {
		std::vector<std::string> blocks;

		std::ifstream stream;
		stream.open(filename);

		std::string temp;
		while (!stream.eof()) {
			std::getline(stream, temp);
			if (temp.length() != 0) { blocks.push_back(temp); }
		}

		stream.close();
		return blocks;
	}

	//@return returns true if the current region is the one we are looking for.
	static bool isTargetRegion(Region *R, std::vector<std::string>& labels) {
		for (auto it = R->block_begin(); it != R->block_end(); ++it) {
			auto loc = std::find(labels.begin(), labels.end(), it->getName());
			if (loc == labels.end()) { 
				return false; 
			}
		}

		return true;
	}

	static DenseSet<Value *> DFSInstruction(Instruction *I) {
		DenseSet<Value *> collected;
		DenseSet<Value *> visited; 
		std::deque<Value *> stack;
		stack.push_back(I);

		while (stack.size() != 0) {
			Value *current = stack.back();
			stack.pop_back();
			
			if (visited.find(current) != visited.end()) {
				continue;
			}

			// terminate if current instruction is alloca 
			if (auto alloca = dyn_cast<AllocaInst>(current)) {
				collected.insert(alloca);
			}

		if (auto global = dyn_cast<GlobalValue>(current)) {
				collected.insert(global);
			}




			visited.insert(current);
			// FIXME more careful selection of operands for certain instructions 
			if (auto a = dyn_cast<Instruction>(current)) {
				if (auto b = dyn_cast<StoreInst>(a)) {
					stack.push_back(b->getOperand(1));
				}
				else {

					for (auto it = a->op_begin(); it != a->op_end(); ++it) {
						if (auto instr = dyn_cast<Instruction>(*it)) {
							stack.push_back(instr);	
						}
						if (auto global = dyn_cast<GlobalValue>(*it)) {
							stack.push_back(global);	
						}
					}
				}

			}

		}

		return collected;
	}



	enum DFSDirection { SUCC, PRED };
	static DenseSet<BasicBlock *> DFS(BasicBlock *BB, enum DFSDirection d) {
		DenseSet<BasicBlock *> blocks; 
		
		std::deque<BasicBlock *> stack;
		stack.push_back(BB);
		
		while (stack.size() != 0) { 
			BasicBlock *current = stack.front();
			stack.pop_front();
			// pick the block and expand it. If it has been visited before, we do not expand it.		
			if (blocks.find(current) != blocks.end()) {
				continue; 
			}
			
			blocks.insert(current);

			switch (d) {
			case SUCC: 
				for (auto it = succ_begin(current); it != succ_end(current); ++it) {
					stack.push_back(*it);
				}
				break;
			case PRED:
				for (auto it = pred_begin(current); it != pred_end(current); ++it) {
					stack.push_back(*it);
				}
				break;
			}
		}
		
		return blocks;
	}

	static void removeOwnBlocks(DenseSet<BasicBlock *>& blocks, Region *R) {
		for (auto it = R->block_begin(); it != R->block_end(); ++it) {
			blocks.erase(*it);		
		}
	}


	// We don't necessarily have to return certain things.
	// Primitive types (i32, i8, float, etc) must always be returned.
	// Array types should not be returned, C treats them as pointers.
	// Pointer type must only be returned if the actual pointer is modified, and not its contents.
	// Struct types must be returned, since they can be passed by value 
	static bool mustReturnLocal(AllocaInst *source, StoreInst *I) {
		const Type *t = source->getAllocatedType();
		if (t->isIntegerTy() || t->isFloatTy() || t->isDoubleTy() || t->isHalfTy() || t->isStructTy()) {
			return true;
		}

		if (source->getAllocatedType()->isPointerTy() && I->getOperand(1) == source) {
			return true; 
		}
			
		return false;
	}

	static bool mustReturnGlobal(GlobalValue *source, StoreInst *I) {
		const Type *t = source->getValueType();
		if (t->isIntegerTy() || t->isFloatTy() || t->isDoubleTy() || t->isStructTy()) {
			return true;
		}
		
		if (t->isPointerTy() && I->getOperand(1) == source) {
			return true;
		}

		return false;
	}


	static void analyzeOperands(Instruction *I, 
								const DenseSet<BasicBlock *>& predecessors,
								const DenseSet<BasicBlock *>& successors,
								DenseSet<Value *>& inputargs, 
								DenseSet<Value *>& outputargs) {
		DenseSet<Value *>sources = DFSInstruction(I);	
		for (auto it = sources.begin(); it != sources.end(); ++it) {
			if (auto instr = dyn_cast<AllocaInst>(*it)) {
				// first we check if source instruction is allocated outside the region,
				// in one of the predecessor basic blocks. We do not care if the instruction 
				// is actually used (stored into, etc), if we did that would cause problems 
				// for stack-allocated arrays as those can be uninitialized.
				if (predecessors.find(instr->getParent()) != predecessors.end()) {
					inputargs.insert(instr);
				} 
				// if instruction is used by some instruction is successor basic block, we add 
				// it to the output argument list only if I is store, i.e. we modify it. 
				for (auto userit = instr->user_begin(); userit != instr->user_end(); ++userit) {
					Instruction *userinstr = cast<Instruction>(*userit);	
					BasicBlock *parentBB = userinstr->getParent(); 
					if (isa<StoreInst>(I) && successors.find(parentBB) != successors.end()) {
						if (mustReturnLocal(instr, cast<StoreInst>(I))) { 
							I->dump();
							outputargs.insert(instr); 
						}
					}
				}
			}
			if (auto global = dyn_cast<GlobalValue>(*it)) {
				// globals are added to input argument list inconditionally. 
				// if current instruction is store, we also add it to return values applying
				// the same typing rules listed above
				inputargs.insert(global);
				if (isa<StoreInst>(I) && mustReturnGlobal(global, cast<StoreInst>(I))) {
					outputargs.insert(global);
				}
			}
		}
				


#if 0
		if (auto instr = dyn_cast<Instruction>(*it)) {
			for (auto uit = instr->user_begin(); uit != instr->user_end(); ++uit) {
				if (auto uinstr = dyn_cast<Instruction>(*uit)) {
					//uinstr->dump();
					BasicBlock *parentblock = uinstr->getParent();

					instr->dump();
					if (predecessors.find(parentblock) != predecessors.end()) {
						inputargs.insert(instr);
					}
					if (isa<StoreInst>(I) && successors.find(parentblock) != successors.end()) {
						if (auto alloca = dyn_cast<AllocaInst>(instr)) {
								outputargs.insert(instr);

							if (alloca->getAllocatedType()->isStructTy()) {
								outputargs.insert(instr);
							}

							if (alloca->getAllocatedType()->isIntegerTy()) {
								outputargs.insert(instr);
							}

							if (alloca->getAllocatedType()->isArrayTy()) {
								errs() << "hello\n";
								outputargs.insert(instr);
							}


						}
					}
				}
			}
		}
	}

#endif
	}

#if 0				
	//FIXME stack allocated structs are not detected!
	static void analyzeOperands(Instruction *I, 
								const DenseSet<BasicBlock *> predecessors,
								const DenseSet<BasicBlock *> successors,
								DenseSet<Value *>& inputargs, 
								DenseSet<Value *>& outputargs) {

		if (!isa<StoreInst>(I) && !isa<LoadInst>(I)) { return; }
		Value *V = DFSInstruction(I);
		assert(V != nullptr);
		V->dump();
#endif

#if 0
			if (auto instr = dyn_cast<Instruction>(V)) {
				//instr->dump();
				for (auto userit = instr->user_begin(); userit != instr->user_end(); ++userit) {
					User *user = (*userit);
					if (auto userinstr = dyn_cast<Instruction>(user)) { 
						BasicBlock *parentblock = userinstr->getParent();
						// if user is in predecessors -> input argument
						if (predecessors.find(parentblock) != predecessors.end()) {
							inputargs.insert(instr);
						}

						// add variable to output args if I is STORE
						//if (!isa<StoreInst>(I)) { continue; }
						if (successors.find(parentblock) != successors.end()) {
							outputargs.insert(instr);
						}
					}
				}
			}
#endif

			// globals are added to input args right away.
			// only returned if they are modified (i.e. used by store instruction)
#if 0
			if (auto global = dyn_cast<GlobalValue>(operand)) {
				inputargs.insert(global);
				if (!isa<StoreInst>(I)) { continue; }
				outputargs.insert(global);
			}
	}
#endif


	struct FuncExtract : public RegionPass {
		static char ID;
		FuncExtract() : RegionPass(ID) {  }
		std::vector<std::string> regionLabels;


// find successors / predecessors
// for each instruction in region 
// if instruction has users in successors -> output arg
// for each operand in instruction 
// if operand has users in  predecessors -> input arg
// if operand has users in successors -> output arg


		bool runOnRegion(Region *R, RGPassManager &RGM) override {
			if (!isTargetRegion(R, regionLabels)) { 
				errs() << "Not the region\n";
				return false; 
			}

			errs() << "Found region!\n";

			BasicBlock *b = R->getEntry();
			DenseSet<BasicBlock *> predecessors = DFS(b, PRED); 
			removeOwnBlocks(predecessors, R);

			DenseSet<BasicBlock *> successors = DFS(b, SUCC);
			removeOwnBlocks(successors, R);


			DenseSet<Value *> inputargs;
			DenseSet<Value *> outputargs;

			for (auto blockit = R->block_begin(); blockit != R->block_end(); ++blockit) 
			for (auto instrit = blockit->begin(); instrit != blockit->end(); ++instrit) {
				Instruction *I = &*instrit;
				if (!isa<StoreInst>(I) && !isa<LoadInst>(I)) { continue; }
				analyzeOperands(I, predecessors, successors, inputargs, outputargs);
			}

			errs() << "in args: \n";
			for (auto it = inputargs.begin(); it != inputargs.end(); ++it) {
				(*it)->dump();
			}

			errs() << "out args: \n";
			for (auto it = outputargs.begin(); it != outputargs.end(); ++it) {
				(*it)->dump();
			}

			return false;
		}


		bool doInitialization(Region *R, RGPassManager &RGM) override {
			regionLabels = readBBListFile(BBListFilename);
			return false;	
		}

		bool doFinalization(void) override {
			return false;	
		}

	};
}

char FuncExtract::ID = 0;
static RegisterPass<FuncExtract> X("funcextract", "Func Extract", false, false);
