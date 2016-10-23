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
		
			errs() << "Predecessors with self blocks removed are: \n";
			for (auto predit = predecessors.begin(); predit != predecessors.end(); ++predit) {
				errs() << (*predit)->getName() << "\n";
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
