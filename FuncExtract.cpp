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
#include "llvm/ADT/StringSet.h"
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

	// returns true if current function and and the region can be found in the region list.
	static bool inRegionList(StringMap<StringSet<>>& SM, Function *F, Region *R) {
		// remove spaces from region name 
		std::string regionname = R->getNameStr();
		std::string::iterator n = std::remove_if(regionname.begin(), regionname.end(), 
			  [](char c) { return std::isspace(static_cast<unsigned char >(c)); });
		regionname.erase(n, regionname.end());

		const auto& it = SM.find(regionname);
		if (it == SM.end()) { return false; }
		const StringSet<>& funclist = it->getValue(); 
		if (funclist.find(F->getName()) == funclist.end()) { return false; }
		return true;
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
			if (sm.size() == 1) { return sm[0]; }
		}

		return nullptr;
	}
	
	// reads list of <functionname, regionname> from the file into the map.
	static void readRegionFile(StringMap<StringSet<>>& S, const std::string& filename) {
		std::ifstream stream;
		stream.open(filename);

		std::string temp;
		while (!stream.eof()) {
			// remove whitespace from the string
			std::getline(stream, temp);
			std::string::iterator n = std::remove_if(temp.begin(), temp.end(), 
				  [](char c) { return std::isspace(static_cast<unsigned char >(c)); });
			temp.erase(n, temp.end());

			size_t idx = temp.find(':');
			if (idx == std::string::npos) { continue; }
			std::string lhs = temp.substr(0, idx);
			std::string rhs = temp.substr(idx + 1, temp.length());

			std::pair<std::string, StringSet<>> kv(rhs, StringSet<>());
			S.insert(kv);

			StringSet<>& lst = S.find(rhs)->getValue();;
			lst.insert(lhs);
			errs() << rhs <<"\n";
		}

		stream.close();
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
								const DenseSet<BasicBlock *>& successors,
								DenseSet<Value *>& inputargs, 
								DenseSet<Value *>& outputargs,
								const AreaLoc& regionBounds,
								const AreaLoc& functionBounds) {
		static DenseSet<Value *> analyzed; 
		DenseSet<Value *> sources = DFSInstruction(I);	
		for (auto it = sources.begin(); it != sources.end(); ++it) {
			// we don't have to look at values we have seen before... 
			if (analyzed.find(*it) != analyzed.end()) { continue; }
			analyzed.insert(*it);
			
			Metadata *M = getMetadata(*it);
			if (!M) { continue; }

			if (auto instr = dyn_cast<AllocaInst>(*it)) {
				// when compiled with -O0 flag, all function arguments are copied onto the stack
				// and debug info of corresponding alloca instructions tells us whether or not 
				// it is used to store function arguments.
				// I.E. avoid using -mem2reg opt pass
				if (isArgument(instr)) {
					inputargs.insert(instr);
				}
				// is source instruction is allocated and declared outside the region? 
				if (!declaredInArea(M, regionBounds)) {
					inputargs.insert(instr);
				}
				// if source instruction is used by some instruction is some successor basic block, 
				// and it is defined inside the region, add it to output list.
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
	}

	// basic type constants (i.e. int / floats) have to be checked separately.
	// Every constant that has the same constant value and are used in the region
	// are added to the input list. You really shouldn't have equal constants defined more than
	// once, though.
	// This works fine unless you start casting such constants and you end up having literal 
	// values that do not have corresponding alloca instruction. So... don't do arbitrary 
	// casting inside the region.  
	// if outsideRegion is true, then I is outside the region and thus args is outputargs.
	// if it is false, then we are looking for inputargs.
	static void analyzeConstants(Instruction *I, 
								 bool  outsideRegion, // is I outside the region?
								 DenseSet<Value *>& args,
								 const AreaLoc& regionBounds,
								 const DenseSet<ValuePair>& constants) {
		for (Value *V: I->operand_values()) {
			if (!isa<ConstantInt>(V) && !isa<ConstantFP>(V)) { continue; }
			for (const ValuePair& constant: constants) {
				if (constant.first == V) {
					Metadata *M = getMetadata(constant.second); // get alloca instruction info;
					if (!M) { continue; }
					if (!outsideRegion && !declaredInArea(M, regionBounds)) { args.insert(constant.second); }
					if ( outsideRegion &&  declaredInArea(M, regionBounds)) { args.insert(constant.second); }
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
	// as necessary. std::pair is returned because function pointers have to be treated 
	// slightly differently and in that case we do not have to append variable name.
	static std::pair<std::string, bool> getTypeString(DIType *T, StringRef variablename) {
		std::vector<unsigned> tags;

		Metadata *md = cast<Metadata>(T);
		while (true) {
			// do not need to look for anymore.
			if (isa<DIBasicType>(md))      { break; }
			if (isa<DISubroutineType>(md)) { break; }

			// we are interested in array type. 
			if (auto a = dyn_cast<DICompositeType>(md)) {
				if (a->getTag() == dwarf::DW_TAG_array_type) { tags.push_back(dwarf::DW_TAG_pointer_type); }
				if (a->getTag() == dwarf::DW_TAG_structure_type)   { tags.push_back(a->getTag()); break; }
				if (a->getTag() == dwarf::DW_TAG_union_type)       { tags.push_back(a->getTag()); break; }
				if (a->getTag() == dwarf::DW_TAG_enumeration_type) { tags.push_back(a->getTag()); break; }
				Metadata *next = a->getBaseType();
				if (next == nullptr)  { break; } // no basetype property here, bailing
				md = next; 
				continue;
			}
 
			if (auto a = dyn_cast<DIDerivedType>(md)) {
				if (a->getTag() == dwarf::DW_TAG_pointer_type) { tags.push_back(a->getTag()); }
				if (a->getTag() == dwarf::DW_TAG_const_type  ) { tags.push_back(a->getTag()); }
				if (a->getTag() == dwarf::DW_TAG_typedef     ) { tags.push_back(a->getTag()); break; }
				Metadata *next = a->getBaseType();
				if (next == nullptr)  { break; } // no basetype property here, bailing
				md = next;
			}

		}

		DIType *type = cast<DIType>(md);
		std::reverse(tags.begin(), tags.end());  

		std::string typestr;
		// function pointers have to be handled a tad differently.
		// First argument in DISubroutineArray is return type;
		// The rest are arguments' types. We also need a name of the value for this one - 
		// terribly inconsistent but it works so far.
		if (auto *a = dyn_cast<DISubroutineType>(md)) {
			const auto& types = a->getTypeArray();
			std::string lhs, rhs;

			// get function's return type
			Metadata *rettypeinfo = types[0];
			if (rettypeinfo == nullptr) { lhs += "void "; }  // void function
			else { lhs += getTypeString(cast<DIType>(rettypeinfo), variablename).first; }

			// get function's arguments' types
			rhs += '(';
			if (types.size() == 1) { rhs += "void)"; } // we have 0 input arguments...
			for (unsigned i = 1; i < types.size(); i++) {
				rhs += getTypeString(cast<DIType>(types[i]), variablename).first;	
				if (i <  types.size() - 1) { rhs += ", ";}
				if (i == types.size() - 1) { rhs += ")"; }
			}

			// is function pointer constant or/and actually a pointer?
			for (unsigned& t: tags) {
				switch (t) {
					case dwarf::DW_TAG_pointer_type: { typestr += "*";      break; }
					case dwarf::DW_TAG_const_type:   { typestr += "const "; break; }
				}
			}

			typestr = lhs + '(' + typestr + variablename.str() + ')' + rhs;
			return std::pair<std::string, bool>(typestr, true);
		}

		// everything else is done below
		// do not insert space when we are not adding anything else...
		// also, we are not expecting values to be of type void, so type->getName should not be empty.
		// TODO we might not need the following condition 
		if (tags.size() == 0) { return std::pair<std::string, bool>(type->getName().str(), false);  }

		// void things are just empty, gotta fix that.
		// also, anonymous structs are also going to show up as 'struct void'. Passing anonymous
		// structs or pointer to structs is not possible in C so end-user will probably have to fix his code
		// to prevent such things from happening.
		if (type->getName().size() == 0) { typestr += "void "; }
		else { typestr += type->getName().str() + " "; }

		for (unsigned& t: tags) {
			switch (t) {
				case dwarf::DW_TAG_pointer_type:     { typestr += "*"; 			      break; }
				case dwarf::DW_TAG_structure_type:   { typestr = "struct " + typestr; break; }
				case dwarf::DW_TAG_union_type:       { typestr = "union "  + typestr; break; }
				case dwarf::DW_TAG_enumeration_type: { typestr = "enum " + typestr;   break; }
				case dwarf::DW_TAG_typedef:          { 							    ; break; }
				case dwarf::DW_TAG_const_type:       { typestr += "const ";           break; }
			}
		}
		return std::pair<std::string, bool>(typestr, false);
	}

	// gets function's return type
	static std::string getFunctionReturnType(const Function *F) {
		DISubprogram *SP = cast<DISubprogram>(F->getMetadata(0));
		if (auto *ST = dyn_cast<DISubroutineType>(SP->getRawType())) {
			Metadata *M = ST->getTypeArray()[0];
			if (!M) { return std::string("void"); }
			return getTypeString(cast<DIType>(M), "").first;
		}

		return std::string("unknown");
	}

	
	static void writeVariableInfo(Value *V, bool isOutputVar, std::ofstream& out) {
		Metadata *M = getMetadata(V);
		if (!M) { return; }
		DIVariable *DI = cast<DIVariable>(M);
		auto kv = getTypeString(cast<DIType>(DI->getRawType()), DI->getName());

		out << XMLOpeningTag("variable", 1);
		out << XMLElement("name", DI->getName().str(), 2);
		out << XMLElement("type", kv.first , 2);
		if (isOutputVar) { out << XMLElement("isoutput", true, 2); }
		if   (kv.second) { out << XMLElement("isfunptr", true, 2); }

		// variable is static. 
		if (auto *a = dyn_cast<GlobalVariable>(V)) {
			if (!a->isConstant() && a->hasInternalLinkage()) {
				out << XMLElement("isstatic", true, 2);	
			}
		}

		out << XMLClosingTag("variable", 1);
		errs() << kv.first << "\n";
	}

	static void writeLocInfo(AreaLoc& loc, const char *tag, std::ofstream& out) {
		out << XMLOpeningTag(tag, 1); 
		out << XMLElement("start", loc.first, 2);
		out << XMLElement("end",  loc.second, 2);
		out << XMLClosingTag(tag, 1); 
	}

											 
	struct FuncExtract : public RegionPass {
		static char ID;
		StringMap<StringSet<>> regionlist;
		
		FuncExtract() : RegionPass(ID) { readRegionFile(regionlist, BBListFilename); }
		~FuncExtract(void) { }

		bool runOnRegion(Region *R, RGPassManager &RGM) override {
			// we really shouldn't try to extract from modules with no metadata...
			Function *F = R->getEntry()->getParent();
			if (!F->hasMetadata()) { 
				errs() << "Function is missing debug metadata, skipping...\n";
				return false;
			}

			if (!inRegionList(regionlist, F, R)) { return false; }

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
				analyzeOperands(I, successors, inputargs, outputargs, regionBounds, functionBounds);
				analyzeConstants(I, false, inputargs, regionBounds, constants);
			}

			for (auto blockit = successors.begin(); blockit != successors.end(); ++blockit) 
			for (auto instrit = (*blockit)->begin(); instrit != (*blockit)->end(); ++instrit) {
				Instruction *I = &*instrit;
				analyzeConstants(I, true, outputargs, regionBounds, constants);
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
			outfile << XMLElement("funcreturntype", getFunctionReturnType(F), 1);
			errs() << "fnreturntype: " << getFunctionReturnType(F) << "\n";



			outfile << XMLClosingTag("extractinfo", 0);
			outfile.close();


			// TODO remove me later!
			for (Value *V: inputargs) { V->dump(); }
			for (Value *V: outputargs) { V->dump(); }

			return false;
		}
	};
}

char FuncExtract::ID = 0;
static RegisterPass<FuncExtract> X("funcextract", "Func Extract", true, true);
