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
#include "llvm/Transforms/Utils/Local.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <deque>
#include <string>
#include <limits>
#include <algorithm>



using namespace llvm;

static cl::opt<std::string> BBListFilename("bblist", 
	   		cl::desc("List of blocks' labels that are to be extracted. Must form a valid region."), 
	   		cl::value_desc("filename"), cl::Required  );

static cl::opt<std::string> OutXMLFilename("out", 
	   		cl::desc("Name of the file to info to."), 
	   		cl::value_desc("filename"), cl::Required  );

namespace {

	typedef std::pair<unsigned,unsigned> AreaLoc;
	typedef std::pair<Value *, Value *>  ValuePair;

	static AreaLoc getRegionLoc(const Region *);
	static AreaLoc getFunctionLoc(const Function *);
	static Metadata * getMetadata(Value *);
	static bool declaredInArea(Metadata *, const AreaLoc&);
	static bool isArgument(Value *);
	static DenseSet<int> regionGetExitingLocs(Region *R);


	// various XML helper functions as we are saving all the extracted info
	// in XML-like format. Better than self-improvised markup.  
	static std::string XMLOpeningTag(const char *key, int numtabs) {
		std::stringstream stream;
		for (int i = 0; i < numtabs; i++) { stream << '\t'; }
		stream << "<" << key << ">" << std::endl;
		return stream.str();
	}

	static std::string XMLClosingTag(const char *key, int numtabs) {
		std::stringstream stream;
		for (int i = 0; i < numtabs; i++) { stream << '\t'; }
		stream << "</" << key << ">" << std::endl;
		return stream.str();
	}

	template <typename T>
	static std::string XMLElement(const char *key, T value, int numtabs) {
		std::stringstream stream;
		for (int i = 0; i < numtabs; i++) { stream << '\t'; }
		stream << "<" << key << ">" << value << "</" << key << ">" << std::endl;
		return stream.str();
	}

	static AreaLoc getRegionLoc(const Region *R) {
		unsigned min = std::numeric_limits<unsigned>::max();
		unsigned max = std::numeric_limits<unsigned>::min();
		
		for (auto blockIter = R->block_begin(); blockIter != R->block_end(); ++blockIter) 
		for (auto instrIter = (*blockIter)->begin(); instrIter != (*blockIter)->end(); ++instrIter) {
			const DebugLoc& x = instrIter->getDebugLoc(); 
			if (x) {
				min = std::min(min, x.getLine());
				max = std::max(max, x.getLine());
			}
		}

		return std::pair<unsigned,unsigned>(min, max);
	}

	static AreaLoc getFunctionLoc(const Function *F) {
		if ( !F->hasMetadata() || !isa<DISubprogram>(F->getMetadata(0)) ) { 
			errs() << "bad debug meta\n";
			return AreaLoc(-1, -1); 
		}

		Metadata *M = F->getMetadata(0);
		unsigned min = cast<DISubprogram>(M)->getLine(); 
		unsigned max = std::numeric_limits<unsigned>::min();

		for (auto blockIter = F->begin(); blockIter != F->end(); ++blockIter) 
		for (auto instrIter = (*blockIter).begin(); instrIter != (*blockIter).end(); ++instrIter) {
			const DebugLoc& x = instrIter->getDebugLoc();
			if (x) { 
				min = std::min(min, x.getLine());
				max = std::max(max, x.getLine()); 
			}
		}
			
		return AreaLoc(min, max); 
	}


	// we need line numbers exiting basic blocks to determine what kind of 
	// branching we have there. In case if those lines of code contain goto/return,
	// we have to do some extra work...
	static DenseSet<int> regionGetExitingLocs(Region *R) {
		DenseSet<int> out;
		for (BasicBlock *BB : R->blocks())
		for (auto succIt = succ_begin(BB); succIt != succ_end(BB); ++succIt) {
			if (!R->contains(*succIt)) {
				// need to iterate over each instruction in each basic block 
				// as terminator instruction does not always have debug metadata...
				unsigned max = std::numeric_limits<unsigned>::min();
				for (Instruction& I: BB->getInstList()) { 
					const DebugLoc& x = I.getDebugLoc();
					if (x) { max = std::max(max, x.getLine()); }
				}
				out.insert(max);
			}
		}

		return out;
	}

	// wrapper method for conveniently getting values metadata.
	// returns nullptr if metadata is not found. 
	static Metadata * getMetadata(Value *V) {
		if (auto *a = dyn_cast<AllocaInst>(V)) {
			DbgDeclareInst *DDI = FindAllocaDbgDeclare(a);
			if (DDI) { return DDI->getVariable(); }
		}

		if (auto *a = dyn_cast<GlobalVariable>(V)) {
			SmallVector<DIGlobalVariable *, 1> sm;
			a->getDebugInfo(sm);	
			return sm[0];
		}

		return nullptr;
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


	// one of the problems is detecting const ints/floats. while llvm ir
	// creates storage for such entities, they are not used anywhere as 
	// clang frontend propagates such constants across instructions.
	// I.e. instead of 
	// load 124 into constant %x; %1 = load %x; %2 = load %a; %3 = add %1 %2
	// clang does the following:
	// load 124 into constant %x; %2 = load %a; %3 = add 124 %2. 
	// Solution: look at alloca instructions that only have one user and that 
	// user is store instruction.
	static DenseSet<ValuePair> findPossibleLocalConstants(Function *F) {
		DenseSet<ValuePair> out;

		for (BasicBlock& BB: F->getBasicBlockList())
		for (Instruction& I: BB.getInstList()) {
			if (auto *alloca = dyn_cast<AllocaInst>(&I)) {
				for (User *U: alloca->users()) {
					if (auto *store = dyn_cast<StoreInst>(U)) {
						Value *operand = store->getValueOperand();
						if (isa<ConstantInt>(operand)) { out.insert(ValuePair(operand, alloca)); }
						if (isa<ConstantFP>(operand))  { out.insert(ValuePair(operand, alloca)); }
					}
				}
			}
		}
		return out;
	}

	static DenseSet<Value *> DFSInstruction(Value *I) {
		DenseSet<Value *> visited; 

		std::deque<Value *> stack;
		stack.push_back(I);

		while (stack.size() != 0) {
			Value *current = stack.back();
			stack.pop_back();
			if (visited.find(current) != visited.end()) { continue; }
			visited.insert(current);

			if (ConstantExpr *constexp = dyn_cast<ConstantExpr>(current)) {
				for (auto it = constexp->op_begin(); it != constexp->op_end(); ++it) {
					if (auto globl = dyn_cast<GlobalVariable>(*it)) { stack.push_back(globl); }
				}
			}

			if (Instruction *instr = dyn_cast<Instruction>(current)) {
				for (auto it = instr->op_begin(); it != instr->op_end(); ++it) {
					if (auto globl    = dyn_cast<GlobalVariable>(*it)) { stack.push_back(globl);    }
					if (auto instr    = dyn_cast<Instruction>(*it))    { stack.push_back(instr);    }
					if (auto constexp = dyn_cast<ConstantExpr>(*it))   { stack.push_back(constexp); }
				}
			}
		}

		// we are only interested in alloca instructions, remove everything else...
		for (Value *val: visited) {
			if (!isa<AllocaInst>(val) && !isa<GlobalVariable>(val)) { visited.erase(val); }
		}

		return visited;
	}

	//helper methods + DFS routine for finding all reachable blocks starting at some 
	//basic block BB. 
	typedef void (EnqueueBlockFunc)(std::deque<BasicBlock *>&, BasicBlock *);

	static void pushSuccessors(std::deque<BasicBlock *>& stack, BasicBlock *BB) {
		for (auto it = succ_begin(BB); it != succ_end(BB); ++it) { stack.push_back(*it); }
	}

	static void pushPredecessors(std::deque<BasicBlock *>& stack, BasicBlock *BB) {
		for (auto it = pred_begin(BB); it != pred_end(BB); ++it) { stack.push_back(*it); }
	}

	static DenseSet<BasicBlock *> DFSBasicBlocks(BasicBlock *BB, EnqueueBlockFunc enqueueFunc) {
		DenseSet<BasicBlock *> visited; 
		
		std::deque<BasicBlock *> stack;
		stack.push_back(BB);
		
		while (stack.size() != 0) { 
			BasicBlock *current = stack.front();
			stack.pop_front();
			// pick the block and expand it. If it has been visited before, we do not expand it
			if (visited.find(current) != visited.end()) { continue; }
			visited.insert(current);
			enqueueFunc(stack, current);
		}
		
		return visited;
	}

	// after DFSBasicBlocks routine we might end up having basic blocks belonging to a region 
	// in the returned basic block set. We want to remove those. 
	static void removeOwnBlocks(DenseSet<BasicBlock *>& blocks, Region *R) {
		for (auto it = R->block_begin(); it != R->block_end(); ++it) { blocks.erase(*it); }
	}


	static void analyzeOperands(Instruction *I, 
								const DenseSet<BasicBlock *>& predecessors,
								const DenseSet<BasicBlock *>& successors,
								DenseSet<Value *>& inputargs, 
								DenseSet<Value *>& outputargs,
								const DenseSet<ValuePair>& constants,
								const AreaLoc& regionBounds,
								const AreaLoc& functionBounds) {
		static DenseSet<Value *> analyzed; 
		DenseSet<Value *> sources = DFSInstruction(I);	
		for (auto it = sources.begin(); it != sources.end(); ++it) {
			// we don't have to look at values we have seen before... 
			if (analyzed.find(*it) != analyzed.end()) { continue; }
			analyzed.insert(*it);

			Metadata *M = getMetadata(*it);
			if (!M) {
				errs() << "Missing debug info for:\n";
				(*it)->dump();
				continue;
			}

			if (auto instr = dyn_cast<AllocaInst>(*it)) {
				// when compiled with -O0 flag, all function arguments are copied onto the stack
				// and debug info of corresponding alloca instructions tells us whether or not 
				// it is used to store function arguments.
				// I.E. avoid using -mem2reg opt pass
				if (isArgument(instr)) {
					inputargs.insert(instr);
				}
				// first we check if source instruction is allocated outside the region,
				// in one of the predecessor basic blocks. We do not care if the instruction 
				// is actually used (stored into, etc), if we did that would cause problems 
				// for stack-allocated arrays as those can be uninitialized.
				if (predecessors.find(instr->getParent()) != predecessors.end()) {
					if (!declaredInArea(M, regionBounds)) {
						inputargs.insert(instr);
					}
				} 
				// if instruction is used by some instruction is successor basic block, we add 
				// it to the output argument list only if I is store, i.e. we modify it. 
				for (auto userit = instr->user_begin(); userit != instr->user_end(); ++userit) {
					Instruction *userinstr = cast<Instruction>(*userit);	
					BasicBlock *parentBB = userinstr->getParent(); 
					if (successors.find(parentBB) != successors.end()) {
						if (declaredInArea(M, regionBounds) && !isArgument(instr)) {
							outputargs.insert(instr);
						}
					}
				}
			}

			// constants defined in functions such as structs/arrays are declared in the global scope now. 
			if (auto globl = dyn_cast<GlobalVariable>(*it)) {
				// if the global is defined in the function but not in the region, then it is an input 
				// argument. If it is defined in region, it is also an output argument.
				if (declaredInArea(M, functionBounds)) {
					if (!declaredInArea(M, regionBounds)) { inputargs.insert(globl);  }
					if ( declaredInArea(M, regionBounds)) { outputargs.insert(globl); }
				}
			}
		}

		// basic type constants (i.e. int / floats) have to be checked separately.
		// if there are multiple equal constants, we add all of them.
		for (Value *V: I->operand_values()) {
			if (!isa<ConstantInt>(V) && !isa<ConstantFP>(V)) { continue; }
			for (const ValuePair& constant: constants) {
				if (constant.first == V) {
					Metadata *M = getMetadata(constant.second); // get alloca instruction info;
					if (!declaredInArea(M, regionBounds)) {  inputargs.insert(constant.second); }
					if ( declaredInArea(M, regionBounds)) { outputargs.insert(constant.second); }
				}
			}
		}
	}

	// compares M's line parameter to AreaLoc, returns true if number is between.
	static bool declaredInArea(Metadata *M, const AreaLoc& A) {
		unsigned linenum = std::numeric_limits<unsigned>::max();
		if (auto *a = dyn_cast<DIGlobalVariable>(M)) { linenum = a->getLine(); }
		if (auto *a  = dyn_cast<DILocalVariable>(M)) { linenum = a->getLine(); }
		if (linenum == std::numeric_limits<unsigned>::max()) { return false; }
		return (A.first <= linenum && linenum <= A.second);
	}

	// debug info also tells us if given alloca istruction is used for storing function
	// arguments. Convenient as we don't need to manually look for matching AllocaInst. 
	static bool isArgument(Value *V) {
		DbgDeclareInst *DDI = FindAllocaDbgDeclare(V);
		if (!DDI) { return false; }
		DILocalVariable *DLV = DDI->getVariable();
		return (DLV->getArg() != 0);
	}

	// extracts the type of the provided debuginfo type as a string. Follows the pointers 
	// as necessary.
	static std::string getTypeString(DIType *T) {
		std::vector<unsigned> tags;

		Metadata *md = cast<Metadata>(T);
		
		while (true) {
			// do not need to look for anymore.
			if (isa<DIBasicType>(md)) { break; }

			// we are interested in array type. 
			if (auto a = dyn_cast<DICompositeType>(md)) {
				if (a->getTag() == dwarf::DW_TAG_array_type) { tags.push_back(dwarf::DW_TAG_pointer_type); }
				if (a->getTag() == dwarf::DW_TAG_structure_type) { tags.push_back(a->getTag()); }
				Metadata *next = a->getBaseType();
				if (next == nullptr)  { goto ret; } // no basetype property here, bailing
				md = next; 
				continue;
			}

			if (auto a = dyn_cast<DIDerivedType>(md)) {
				if (a->getTag() == dwarf::DW_TAG_pointer_type) { tags.push_back(a->getTag()); }
				if (a->getTag() == dwarf::DW_TAG_const_type  ) { tags.push_back(a->getTag()); }
				if (a->getTag() == dwarf::DW_TAG_typedef     ) { tags.push_back(a->getTag()); break; }
				Metadata *next = a->getBaseType();
				if (next == nullptr)  { goto ret; } // no basetype property here, bailing
				md = next;
			}
		}

ret:
		DIType *type = cast<DIType>(md);
		std::reverse(tags.begin(), tags.end());  

		std::string typestr;
		typestr += type->getName().str() + " ";

		for (unsigned& t: tags) {
			switch (t) {
				case dwarf::DW_TAG_pointer_type:   { typestr += "*"; 			    break; }
				case dwarf::DW_TAG_structure_type: { typestr = "struct " + typestr; break; }
				case dwarf::DW_TAG_typedef:        { 							  ; break; }
				case dwarf::DW_TAG_const_type:     { typestr += "const";            break; }
			}
		}

		return typestr; 
	}
	
	static void writeVariableInfo(Value *V, bool isOutputVar, std::ofstream& out) {
		DIVariable *DI = cast<DIVariable>(getMetadata(V));
		out << XMLOpeningTag("variable", 1);
		out << XMLElement("name", DI->getName().str(), 2);
		out << XMLElement("type", getTypeString(cast<DIType>(DI->getRawType())), 2);
		if (isOutputVar) { out << XMLElement("isoutput", true, 2); }
		out << XMLClosingTag("variable", 1);
		errs() << getTypeString(cast<DIType>(DI->getRawType())) << "\n"; //#TODO REMOVE ME LATER
	}

	static void writeLocInfo(AreaLoc& loc, const char *tag, std::ofstream& out) {
		out << XMLOpeningTag(tag, 1); 
		out << XMLElement("start", loc.first, 2);
		out << XMLElement("end",  loc.second, 2);
		out << XMLClosingTag(tag, 1); 
	}
										 
	struct FuncExtract : public RegionPass {
		static char ID;
		FuncExtract() : RegionPass(ID) {  }
		StringMap<DenseSet<StringRef>> funcs;

		bool runOnRegion(Region *R, RGPassManager &RGM) override {
			if (!isTargetRegion(R, funcs)) { return false; }

			errs() << "Found region!\n";
			
			Function *F = R->getEntry()->getParent();
			DenseSet<ValuePair> constants = findPossibleLocalConstants(F);
			AreaLoc regionBounds = getRegionLoc(R);
			AreaLoc functionBounds = getFunctionLoc(F);
			DenseSet<int> regionExit = regionGetExitingLocs(R);
			
			BasicBlock *b = R->getEntry();
			DenseSet<BasicBlock *> predecessors = DFSBasicBlocks(b, pushPredecessors); 
			removeOwnBlocks(predecessors, R);

			DenseSet<BasicBlock *> successors = DFSBasicBlocks(b, pushSuccessors);
			removeOwnBlocks(successors, R);

			DenseSet<Value *> inputargs;
			DenseSet<Value *> outputargs;
			for (auto blockit = R->block_begin(); blockit != R->block_end(); ++blockit) 
			for (auto instrit = blockit->begin(); instrit != blockit->end(); ++instrit) {
				Instruction *I = &*instrit;
				analyzeOperands(I, predecessors, successors, inputargs, outputargs, constants, regionBounds, functionBounds);
			}

			std::ofstream outfile;
			outfile.open(OutXMLFilename, std::ofstream::out);
			//writing stuff in xml-like format
			outfile << XMLOpeningTag("extractinfo", 0);
			writeLocInfo(regionBounds, "region", outfile);
			writeLocInfo(functionBounds, "function", outfile);

			// dump variable info...
			for (Value *V : inputargs)  { writeVariableInfo(V, false, outfile); }
			for (Value *V : outputargs) { writeVariableInfo(V, true,  outfile); }

			// dump region exit locs
			for (int& i : regionExit)   { outfile << XMLElement("regionexit", i, 1); }

			outfile << XMLClosingTag("extractinfo", 0);
			outfile.close();

			// TODO remove me later!
			for (Value *V: inputargs) { V->dump(); }
			for (Value *V: outputargs) { V->dump(); }

			return false;
		}


		bool doInitialization(Region *R, RGPassManager &RGM) override {
			//TODO we should probably initialize this in the constructor?
			static bool read = false;
			if (read != 0) { return false; }
			read = 1;
			readBBListFile(funcs, BBListFilename);
			errs() << "done reading\n";
			return false;	
		}

		bool doFinalization(void) override {
			//errs() << "Cleaning up!\n";
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
static RegisterPass<FuncExtract> X("funcextract", "Func Extract", true, true);
