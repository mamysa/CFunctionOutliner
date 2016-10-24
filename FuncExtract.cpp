#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/RegionPass.h"
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

	enum DFSDirection { SUCC, PRED };
	static DenseSet<BasicBlock *> DFS(BasicBlock *BB, enum DFSDirection d) {
		DenseSet<BasicBlock *> blocks; 
		
		std::deque<BasicBlock *> stack;
		stack.push_back(BB);
		
		while (stack.size() != 0) { 
			BasicBlock *current = stack.back();
			stack.pop_back();
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

	static void analyzeOperands(Instruction *I, 
								const DenseSet<BasicBlock *> predecessors,
								const DenseSet<BasicBlock *> successors,
								DenseSet<Value *>& inputargs, 
								DenseSet<Value *>& outputargs) {
		// if instruction has users in some successor -> output argument
		for (auto opit = I->op_begin(); opit != I->op_end(); ++opit) {
			Value *operand = (*opit);
			if (auto instr = dyn_cast<Instruction>(operand)) {
				//instr->dump();
				for (auto userit = instr->user_begin(); userit != instr->user_end(); ++userit) {
					User *user = (*userit);
					if (auto userinstr = dyn_cast<Instruction>(user)) { 
						BasicBlock *parentblock = userinstr->getParent();
						// if user is in predecessors -> input argument
						if (predecessors.find(parentblock) != predecessors.end()) {
							inputargs.insert(instr);
						}
						if (successors.find(parentblock) != successors.end()) {
							outputargs.insert(instr);
						}

					}
				}
			}
			//if global 


		}
	}


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
				analyzeOperands(&*instrit, predecessors, successors, inputargs, outputargs);
				
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
