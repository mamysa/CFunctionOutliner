#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/RegionPass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/DenseSet.h"
#include <fstream>
#include <vector>
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
	bool isTargetRegion(Region *R, std::vector<std::string>& labels) {
		for (auto it = R->block_begin(); it != R->block_end(); ++it) {
			auto loc = std::find(labels.begin(), labels.end(), it->getName());
			if (loc == labels.end()) { 
				return false; 
			}
		}

		return true;
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
