#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/RegionPass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include <fstream>
#include <vector>
#include <deque>
#include <string>
#include <limits>



using namespace llvm;

static cl::opt<std::string> BBListFilename("bblist", 
	   		cl::desc("List of blocks' labels that are to be extracted. Must form a valid region."), 
	   		cl::value_desc("filename"), cl::Required  );

namespace {

	static bool  				  isEarlyReturnBlock(const BasicBlock *);
	static DenseSet<BasicBlock *> findEarlyReturnsInRegion(const Region *);
	static std::pair<unsigned,unsigned> getRegionLoc(DenseSet<BasicBlock *>&);
	static void getVariableDebugInfo(Function *, DenseMap<Value *, DILocalVariable *>&);
	static bool variableDeclaredInRegion(Value *, const std::pair<unsigned,unsigned>&, const DenseMap<Value *, DILocalVariable *>&);
										 

	static std::pair<unsigned,unsigned> getRegionLoc(const Region *R) {
		unsigned min = std::numeric_limits<unsigned>::max();
		unsigned max = std::numeric_limits<unsigned>::min();
		
		for (auto blockIter = R->block_begin(); blockIter != R->block_end(); ++blockIter) 
		for (auto instrIter = (*blockIter)->begin(); instrIter != (*blockIter)->end(); ++instrIter) {
			const DebugLoc& x = instrIter->getDebugLoc(); //FIXME invalid debugloc? 
			if (x) {
				min = std::min(min, x.getLine());
				max = std::max(max, x.getLine());
			}
		}
		errs() << min << " " << max << "\n";
		return std::pair<unsigned,unsigned>(min, max);
	}

	// Detects basic blocks that only serve as return, meaning instructions in it
	// only do memory reads, bitcasting and returning.
	static bool isEarlyReturnBlock(const BasicBlock *BB) {
		if (!isa<ReturnInst>(BB->getTerminator())) { return false; }
		for (auto it = BB->begin(); it != BB->end(); ++it) {
			if ( !isa<BitCastInst>(*it) && 
				 !isa<ReturnInst>(*it)  && 
				 !isa<LoadInst>(*it)    && 
				 !isa<GetElementPtrInst>(*it) ) { return false; }
		}
		return true;
	}


	// finds basic blocks in the region that can terminate early. We need this 
	// to determine where the function returns and modify C-file accordingly 
	// (i.e. when returned, we set RET_EARLY flag so that we can bail afterwards )
	static DenseSet<BasicBlock *> findEarlyReturnsInRegion(const Region *R) {
		DenseSet<BasicBlock *> out;
		Function *F = R->getEntry()->getParent();
		for (auto a = F->begin(); a != F->end(); ++a) {
			BasicBlock *retb = (&*a);
			if (!isEarlyReturnBlock(retb)) { continue; }
			for (auto b = R->block_begin(); b != R->block_end(); ++b) 
			for (auto c = succ_begin(*b)  ; c != succ_end(*b)  ; ++c) {
				if (retb == *c) {
					out.insert(*b); 
				}
			}
		}

		return out;
	}


	//@return - list of basic block labels
#if 0
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
#endif


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

	//@return returns true if the current region is the one we are looking for.
	// we are expecting the first label to be the function name prefixed with ! symbol.
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

	// returns true if region's first basic block is also a first basic block of a function 
	static bool includesEntryBasicBlock(const Region *R) {
		Function *F = R->getEntry()->getParent();
		const BasicBlock *a = &F->getEntryBlock();
		const BasicBlock *b =  R->getEntry();
		return (a == b);
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



	
	typedef void (EnqueueBlockFunc)(std::deque<BasicBlock *>, BasicBlock *);

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

	static void analyzeFunctionArguments(Region *R, 
										 DenseSet<Value *>& inputargs) {
		Function *F = R->getEntry()->getParent();
		for (auto it = F->arg_begin(); it != F->arg_end(); ++it) {
			inputargs.insert(&*it);
		}
	}


	static void analyzeOperands(Instruction *I, 
								const DenseSet<BasicBlock *>& predecessors,
								const DenseSet<BasicBlock *>& successors,
								DenseSet<Value *>& inputargs, 
								DenseSet<Value *>& outputargs,
								const std::pair<unsigned,unsigned>& regionBounds,
								const DenseMap<Value *, DILocalVariable *>& debugInfo) {
		DenseSet<Value *>sources = DFSInstruction(I);	
		for (auto it = sources.begin(); it != sources.end(); ++it) {
			if (auto instr = dyn_cast<AllocaInst>(*it)) {
				// first we check if source instruction is allocated outside the region,
				// in one of the predecessor basic blocks. We do not care if the instruction 
				// is actually used (stored into, etc), if we did that would cause problems 
				// for stack-allocated arrays as those can be uninitialized.
				if (predecessors.find(instr->getParent()) != predecessors.end()) {
					if (!variableDeclaredInRegion(instr, regionBounds, debugInfo)) 
						inputargs.insert(instr);
				} 
				// if instruction is used by some instruction is successor basic block, we add 
				// it to the output argument list only if I is store, i.e. we modify it. 
				for (auto userit = instr->user_begin(); userit != instr->user_end(); ++userit) {
					Instruction *userinstr = cast<Instruction>(*userit);	
					BasicBlock *parentBB = userinstr->getParent(); 
					if (isa<MemCpyInst>(I) && successors.find(parentBB) != successors.end()) {
						if (variableDeclaredInRegion(instr, regionBounds, debugInfo)) 
						outputargs.insert(instr);
					}
					if (isa<StoreInst>(I) && successors.find(parentBB) != successors.end()) {
						if (variableDeclaredInRegion(instr, regionBounds, debugInfo)) 
						outputargs.insert(instr); 
					}
				}
			}
#if 0
			if (auto global = dyn_cast<GlobalValue>(*it)) {
				// globals are added to input argument list inconditionally. 
				// if current instruction is store, we also add it to return values applying
				// the same typing rules listed above
				inputargs.insert(global); // FIXME also pushes memcpy??
				if (isa<StoreInst>(I)) {
					outputargs.insert(global);
				}
			}
#endif
		}
	}

	// extracts debug metadata for every local variable and stores it in the map. We need this
	// to determine where variables were originally declared.
	static void getVariableDebugInfo(Function *F, DenseMap<Value *, DILocalVariable *>& map) {
		for (BasicBlock& BB : F->getBasicBlockList()) 
		for (Instruction& I :   (&BB)->getInstList()) {
			if (!isa<AllocaInst>(&I)) 
				continue; 

			if (auto *LSM = LocalAsMetadata::getIfExists(&I)) 
			if (auto *MDV = MetadataAsValue::getIfExists((&I)->getContext(), LSM)) {
				for (User *U : MDV->users()) {
					if (DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(U)) {
						DILocalVariable *local = DDI->getVariable();
						std::pair<Value *, DILocalVariable *> kv(&I, local);
						map.insert(kv);
					}
				}
			}
		}
	}


	static bool variableDeclaredInRegion(Value *V, const std::pair<unsigned,unsigned>& regionBounds, 
										 const DenseMap<Value *, DILocalVariable *>& debugInfo) { 
		auto iterator = debugInfo.find(V);
		if (iterator == debugInfo.end()) {
			errs() << "Could not locate variable info: \n";
			V->dump();
			return false;

		}
		DILocalVariable *DLV = iterator->getSecond();
		unsigned line = DLV->getLine();
		return (regionBounds.first <= line && line <= regionBounds.second);
	}
										 


	struct FuncExtract : public RegionPass {
		static char ID;
		FuncExtract() : RegionPass(ID) {  }
		StringMap<DenseSet<StringRef>> funcs;


// find successors / predecessors
// for each instruction in region 
// if instruction has users in successors -> output arg
// for each operand in instruction 
// if operand has users in  predecessors -> input arg
// if operand has users in successors -> output arg


		bool runOnRegion(Region *R, RGPassManager &RGM) override {
			if (!isTargetRegion(R, funcs)) { 
				errs() << "Not the region\n";
				return false; 
			}

			errs() << "Found region!\n";

			std::pair<unsigned,unsigned> regionBounds = getRegionLoc(R);

			BasicBlock *b = R->getEntry();
			DenseSet<BasicBlock *> predecessors = DFS(b, PRED); 
			removeOwnBlocks(predecessors, R);

			DenseSet<BasicBlock *> successors = DFS(b, SUCC);
			removeOwnBlocks(successors, R);

			DenseSet<Value *> inputargs;
			DenseSet<Value *> outputargs;
			DenseMap<Value *, DILocalVariable *> debugInfo;

			Function *F = R->getEntry()->getParent();
			getVariableDebugInfo(F, debugInfo);

			for (auto blockit = R->block_begin(); blockit != R->block_end(); ++blockit) 
			for (auto instrit = blockit->begin(); instrit != blockit->end(); ++instrit) {
				Instruction *I = &*instrit;
				if (!isa<StoreInst>(I) && !isa<LoadInst>(I) && !isa<MemCpyInst>(I)) { continue; }
				analyzeOperands(I, predecessors, successors, inputargs, outputargs, regionBounds, debugInfo );
			}

#if 0
			if (includesEntryBasicBlock(R)) {
				//analyzeFunctionArguments(R, inputargs);
			}
#endif
			


#if 0
			for (auto it = inputargs.begin(); it != inputargs.end(); ++it) {
				auto *L = LocalAsMetadata::get(*it);
				auto *MDV = MetadataAsValue::get((*it)->getContext(), L);
				for (User *U : MDV->users()) {
					DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(U);
					// gives us <Value *, DbgDeclareInstr *> pair
					if (DDI) {
						DILocalVariable *localVar = DDI->getVariable();
						(*it)->dump();
						errs() << localVar->getName() << "\n";
						errs() << localVar->getLine() << "\n";
						Metadata *m = localVar->getType();
						if (DIBasicType *DIBT = dyn_cast<DIBasicType>(m)) {
							errs() << DIBT->getName() << "\n";
						}
						if (DICompositeType *DICT = dyn_cast<DICompositeType>(m)) {
							if (DICT->getTag() == dwarf::DW_TAG_structure_type) {

								errs() << "struct " << DICT->getName() << "\n";
							}
						}


						if (DIDerivedType *DICT = dyn_cast<DIDerivedType>(m)) {
							if (DICT->getTag() == dwarf::DW_TAG_pointer_type) {
								errs() << "pointer " << DICT->getName() << "\n";

								int ptrcount = 1;
								//gotta reach the type that is not tagged as pointer...
								Metadata *t = DICT->getRawBaseType();
								while (true) {
									if (auto *x = dyn_cast<DIDerivedType>(t)) {
										t = x->getRawBaseType();
										ptrcount++;
										continue;
									}

									if (auto *x = dyn_cast<DICompositeType>(t)) {
										errs() << x->getName() << " " << ptrcount << "\n";
										break;
									}

									if (auto *x = dyn_cast<DIBasicType>(t)) {
										errs() << x->getName() << " " << ptrcount << "\n";
										break;
									}
								}




								if (auto t2 = dyn_cast<DIDerivedType>(t)) {
									t2->dump();
								}




								//(*it)->dump();
							}
						}

					}
				}


				errs() << "--\n";
			}
#endif
			


#if 0
			Function *F = R->getEntry()->getParent();
			for (auto bit = F->begin(); bit != F->end(); ++bit) {
				BasicBlock *bb = &*bit;
				for (auto iit = bb->begin(); iit != bb->end(); ++iit) {
					if (auto instr = dyn_cast<DbgDeclareInst>(&*iit)) {





					}
				}
			}
#endif

#if 0
				if (auto instr = dyn_cast<Instruction>(*it)) {
					analyzeMetadata(instr);
				}
			}

#endif

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
			//TODO we should probably initialize this in the constructor?
			static bool read = false;
			if (read != 0) { return false; }
			errs() << "Initialzing\n";
			read = 1;
			readBBListFile(funcs, BBListFilename);
			return false;	
		}

		bool doFinalization(void) override {
			errs() << "Cleaning up!\n";
			// TODO delete buffers used by StringRefs
			for (auto it = funcs.begin(); it != funcs.end(); ++it) {
				//delete[] (*it).first().data();	
				DenseSet<StringRef>& set = it->getValue();
				for (auto b = set.begin(); b != set.end(); ++b) {
					//delete[] b->data();
				}

			}
			return false;	
		}

		~FuncExtract(void) {
			errs() << "Hello I am your destructor!\n";
		}

	};
}

char FuncExtract::ID = 0;
static RegisterPass<FuncExtract> X("funcextract", "Func Extract", false, false);
