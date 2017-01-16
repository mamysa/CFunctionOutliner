#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/RegionPass.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"
#include <fstream>

using namespace llvm;

static cl::opt<std::string> BBListFilename("bblist", 
	   		cl::desc("List of blocks' labels that are to be extracted. Must form a valid region."), 
	   		cl::value_desc("filename"), cl::Required  );

namespace {

	static bool isTargetRegion(const Region *R, const StringMap<DenseSet<StringRef>>& regionlabels) {
		Function *F = R->getEntry()->getParent();
		auto fnit = regionlabels.find(F->getName());
		if (fnit == regionlabels.end()) { return false; }
		const DenseSet<StringRef>& blocks = fnit->getValue();

		int numblocks = 0;
		for (auto it = R->block_begin(); it != R->block_end(); ++it) {
			if (blocks.find(it->getName()) == blocks.end()) { return false; }
			numblocks++;
		}

		return (numblocks == (int)blocks.size());
	}


	static void readBBListFile(StringMap<DenseSet<StringRef>>& F, 
										  const std::string filename) {
		std::ifstream stream;
		stream.open(filename);

		std::string tempstr;
		StringRef   current;

		while (!stream.eof()) { 
			std::getline(stream, tempstr);
			if (tempstr.length() == 0) { continue; }

			// for some reason i cannot store std::string inside llvm data structures (doesn't compile)
			// so i have to create stringrefs....
			// string does not have to be null-terminated for stringref to work
			char *buf = new char[tempstr.length()];
			std::memcpy(buf, tempstr.c_str(), tempstr.length()); 
			StringRef str(buf, tempstr.length());
			str = str.trim();

			if (tempstr.find("!") == 0) {
				str = str.ltrim("!");
				std::pair<StringRef, DenseSet<StringRef>> kv(str, DenseSet<StringRef>());
				F.insert(kv);
				current = str;
			} else {
				auto it = F.find(current);
				if (it == F.end()) { 
					errs() << "Found basic block without parent\n"; 
					continue; 
				}
				(*it).getValue().insert(str);
			}
		}

		stream.close();
	}




	

	static std::vector<BasicBlock *> collectRegion(const Region *R) {
		std::vector<BasicBlock *> out;

		for (auto it = R->block_begin(); it != R->block_end(); ++it) {
			out.push_back(*it);
		}

		return out;
	}

	struct CodeExtractorTest: public RegionPass {
		static char ID;
		CodeExtractorTest() : RegionPass(ID) {  }
		StringMap<DenseSet<StringRef>> funcs;

		bool runOnRegion(Region *R, RGPassManager &RGM) override {
			if (!isTargetRegion(R, funcs)) { 
				errs() << "Not the region\n";
				return false; 
			}
			
			errs() << "found region!\n";
			auto bbs = collectRegion(R);
			CodeExtractor extractor(bbs, nullptr, false);
			errs() << extractor.isEligible() << "\n";

			SetVector<Value *> in;
			SetVector<Value *> outz;

			Function *F = extractor.extractCodeRegion();
			F->dump();
			extractor.extractCodeRegion(in, outz);
			errs() << "in \n";
			for (auto it = in.begin(); it != in.end(); ++it) {
				(*it)->dump();
			}

			errs() << "out \n";
			for (auto it = outz.begin(); it != outz.end(); ++it) {
				(*it)->dump();
			}


		




			return false;
		}

		bool doInitialization(Region *R, RGPassManager &RGM) override {
			readBBListFile(funcs, BBListFilename);
			return false;	
		}

		bool doFinalization(void) override {
			return false;	
		}
	};
}


char CodeExtractorTest::ID = 0;
static RegisterPass<CodeExtractorTest> X("codeextracttest", "code extract test", false, false);
