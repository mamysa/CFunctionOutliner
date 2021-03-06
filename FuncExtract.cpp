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

static cl::opt<std::string> OutDirectory("out", 
	   		cl::desc("Name of the file to info to."), 
	   		cl::value_desc("outputdirectory"), cl::Required  );

namespace {
	typedef std::pair<unsigned,unsigned> AreaLoc;
	typedef std::pair<Value *, Value *>  ValuePair;

	struct VariableInfo { 
		std::string name; 
		std::string type; 
		bool typehasname;  // sometimes we need to include variable name into the type definition.
		bool isfunptr; 
		bool isconstq; 
		bool isstatic; 
		bool isarrayt;
	};

	// XML writer helper.
	static std::string XMLOpeningTag(const char *, int);
	static std::string XMLClosingTag(const char *, int);
	template<typename T> static std::string XMLElement(const char *, T, int);

	// various I/O / region validation funcs.
	static void readRegionFile(StringMap<StringSet<>>&, const std::string&);
	static bool inRegionList(StringMap<StringSet<>>&, Function *, Region *);
	static std::string generateFilename(Function *, Region *);
	static void writeVariableInfo(VariableInfo&, bool, std::ofstream&);
	static void writeLocInfo(AreaLoc&, const char *, std::ofstream&);

	// various functions dealing with finding line numbers for various things.
	static inline AreaLoc getBBLoc(const BasicBlock *);
	static AreaLoc getRegionLoc(const Region *);
	static AreaLoc getFunctionLoc(const Function *);
	static DenseSet<int> regionGetExitingLocs(Region *);

	static Metadata * getMetadata(Value *);
	static bool declaredInArea(Metadata *, const AreaLoc&);
	static bool isArgument(Value *);
	static DenseSet<BasicBlock *> collectSuccessorBasicBlocks(Region *);
	static DenseSet<Value *> DFSInstruction(Value *);
	static DenseSet<ValuePair> findBasicConstants(Function *, const AreaLoc&);
	static void findInputs(Instruction *, const AreaLoc&, const AreaLoc&, const DenseSet<ValuePair>&,
						   DenseSet<Value *>&, DenseSet<Value *>&);
	static void findOutputs(Instruction *, const AreaLoc&, const AreaLoc&, const DenseSet<ValuePair>&,
						    DenseSet<Value *>&, DenseSet<Value *>&);
	static VariableInfo getTypeString(DIType *, StringRef);
	static VariableInfo getVariableInfo(Value *);
	static std::string getFunctionReturnType(const Function *);


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

	template<typename T> static std::string XMLElement(const char *key, T value, int numtabs) {
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

	// generates the filename for the xml file output of the pass will be written to.
	// follows the following format: functionname_startregion_endregion
	static std::string generateFilename(Function *F, Region *R) {
		std::string funcname =  F->getName().str();

#define NUMNAMES 2
		std::string sname = R->getEntry()->getName().str();;
		std::string ename = "fnend";
		if (R->getExit()) { ename= R->getExit()->getName().str(); }

		std::string blocknames[NUMNAMES] = { sname, ename };

		for (std::string& s: blocknames) {
			std::string::iterator n = std::remove_if(s.begin(), s.end(), [](char c) { return c == '.'; });
			s.erase(n, s.end());
		}
#undef NUMNAMES

		return funcname + '_' + blocknames[0] + '_'  + blocknames[1];
	}

	// finds first / last line numbers of the basic block. 
	static inline AreaLoc getBBLoc(const BasicBlock *BB) {
		unsigned min = std::numeric_limits<unsigned>::max();
		unsigned max = std::numeric_limits<unsigned>::min();

		for (const Instruction& I: BB->getInstList()) {
			const DebugLoc& x = I.getDebugLoc(); 
			if (x) {
				min = std::min(min, x.getLine());
				max = std::max(max, x.getLine());
			}
		}

		return AreaLoc(min, max);
	}

	// finds first / last line numbers of the region. 
	// inaccurate when region contains an entry basic block due to function
	// arguments being pushed onto the stack.
	static inline AreaLoc getRegionLoc(const Region *R) {
		unsigned min = std::numeric_limits<unsigned>::max();
		unsigned max = std::numeric_limits<unsigned>::min();

		for (BasicBlock *BB: R->blocks()) {
			AreaLoc a = getBBLoc(BB);
			min = std::min(min, a.first);
			max = std::max(max, a.second);
		}

		return AreaLoc(min, max); 
	}

	// finds first / last line numbers of the function. 
	static inline AreaLoc getFunctionLoc(const Function *F) {
		Metadata *M = F->getMetadata(0);
		unsigned min = cast<DISubprogram>(M)->getLine(); 
		unsigned max = std::numeric_limits<unsigned>::min();

		for (const BasicBlock& BB: F->getBasicBlockList()) {
			AreaLoc a = getBBLoc(&BB);
			min = std::min(min, a.first);
			max = std::max(max, a.second);
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

			StringSet<>& lst = S.find(rhs)->getValue();
			lst.insert(lhs);
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
	static DenseSet<ValuePair> findBasicConstants(Function *F, const AreaLoc& functionBounds) {
		DenseSet<ValuePair> out;

		for (BasicBlock& BB: F->getBasicBlockList())
		for (Instruction& I: BB.getInstList()) {
			if (auto *alloca = dyn_cast<AllocaInst>(&I)) {
				if (alloca->getNumUses() != 1) { continue; }
				for (User *U: alloca->users()) {
					if (auto *store = dyn_cast<StoreInst>(U)) {
						Value *operand = store->getValueOperand();
						ValuePair pair(operand, alloca);
						if (isa<ConstantInt>(operand)) { out.insert(pair); }
						if (isa<ConstantFP>(operand))  { out.insert(pair); }
					}
				}
			}
		}

		// we also have to look for things like local static consts.
		for (GlobalVariable& G: F->getParent()->globals()) {
			Metadata *M = getMetadata(&G);
			if (!M) { continue; }
			if (G.isConstant() && declaredInArea(M, functionBounds)) {
				Value *operand = G.getOperand(0);
				ValuePair pair(operand, &G);
				if (isa<ConstantInt>(operand)) { out.insert(pair); }
				if (isa<ConstantFP>(operand))  { out.insert(pair); }
			}
		}

		return out;
	}

// finds all reachable basic blocks after exiting from the region.
	static DenseSet<BasicBlock *> collectSuccessorBasicBlocks(Region *R) {
		DenseSet<BasicBlock *> visited; 
		std::deque<BasicBlock *> stack;

		stack.push_back(R->getEntry());
		while (stack.size() != 0) { 
			// pick the block and expand it. If it has been visited before, we do not expand it
			BasicBlock *current = stack.front();
			stack.pop_front();
			if (visited.find(current) != visited.end()) { continue; }
			visited.insert(current);
			for (auto it = succ_begin(current); it != succ_end(current); ++it) { stack.push_back(*it); }
		}

		// remove basic blocks belonging to the region.
		for (auto it = R->block_begin(); it != R->block_end(); ++it) { visited.erase(*it); }
		return visited;
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
				//Instruction *BB = constexp->getAsInstruction();
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

	
	static void findInputs(Instruction *I, 
						   const AreaLoc& funcloc, 
						   const AreaLoc& regionloc,
						   const DenseSet<ValuePair>& constants,
						   DenseSet<Value *>& previous,
						   DenseSet<Value *>& arglist) {
		DenseSet<Value *> sources = DFSInstruction(I);	
		for (Value *V: sources) {
			// we don't have to look at values we have seen before... 
			if (previous.find(V) != previous.end()) { continue; }
			previous.insert(V);

			Metadata *M = getMetadata(V);
			if (!M) { continue; }

			if (auto *instr = dyn_cast<AllocaInst>(V)) {
				if (isArgument(instr))             { arglist.insert(instr); }
				if (!declaredInArea(M, regionloc)) { arglist.insert(instr); }
			}

			// globals must de declared inside the function.
			if (auto *globl = dyn_cast<GlobalVariable>(V)) {
				if (declaredInArea(M, funcloc) && !declaredInArea(M, regionloc)) { 
					arglist.insert(globl); 
				}
			}
		}

		// if we happen to have some instructions using magic numbers, check those against 
		// basic consts list.
		for (Value *V : I->operands()) {
			if (!isa<ConstantInt>(V) && !isa<ConstantFP>(V)) { continue; }
			for (const ValuePair& constant: constants) {
				if (constant.first == V) {
					Metadata *M = getMetadata(constant.second); // get alloca instruction info;
					if (!M) { continue; }
					if (!declaredInArea(M, regionloc)) { arglist.insert(constant.second); }
				}
			}
		}
	}

	static void findOutputs(Instruction *I, 
						    const AreaLoc& funcloc, 
						    const AreaLoc& regionloc,
						    const DenseSet<ValuePair>& constants,
						    DenseSet<Value *>& previous,
						    DenseSet<Value *>& arglist) {
		DenseSet<Value *> sources = DFSInstruction(I);	
		for (Value *V: sources) {
			// we don't have to look at values we have seen before... 
			if (previous.find(V) != previous.end()) { continue; }
			previous.insert(V);

			Metadata *M = getMetadata(V);
			if (!M) { continue; }
			if (auto *instr = dyn_cast<AllocaInst>(V)) {
				if (declaredInArea(M, regionloc) && !isArgument(instr)) { 
					arglist.insert(instr); 
				}
			}

			// globals (const qualified structures) must de declared inside the function.
			if (auto *globl = dyn_cast<GlobalVariable>(V)) {
				if (declaredInArea(M, funcloc) && declaredInArea(M, regionloc)) { 
					arglist.insert(globl); 
				}
			}
		}

		for (Value *V : I->operands()) {
			if (!isa<ConstantInt>(V) && !isa<ConstantFP>(V)) { continue; }
			for (const ValuePair& constant: constants) {
				if (constant.first == V) {
					Metadata *M = getMetadata(constant.second); // get alloca instruction info;
					if (!M) { continue; }
					if (declaredInArea(M, regionloc)) { arglist.insert(constant.second); }
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
	static VariableInfo getTypeString(DIType *T, StringRef variablename) {
		std::vector<unsigned> tags;
		std::vector<DINodeArray> ranges; // for array size indexes

		Metadata *md = cast<Metadata>(T);
		while (true) {
			// do not need to look for anymore.
			if (isa<DIBasicType>(md))      { break; }
			if (isa<DISubroutineType>(md)) { break; }

			// we are interested in array type. 
			if (auto a = dyn_cast<DICompositeType>(md)) {
				auto t = a->getTag();
				if (t == dwarf::DW_TAG_array_type)       { tags.push_back(t); ranges.push_back(a->getElements()); }
				if (t == dwarf::DW_TAG_structure_type)   { tags.push_back(t); break; }
				if (t == dwarf::DW_TAG_union_type)       { tags.push_back(t); break; }
				if (t == dwarf::DW_TAG_enumeration_type) { tags.push_back(t); break; }
				Metadata *next = a->getBaseType();
				if (next == nullptr)  { break; } // no basetype property here, bailing
				md = next; 
				continue;
			}
 
			if (auto a = dyn_cast<DIDerivedType>(md)) {
				auto t = a->getTag();
				if (t == dwarf::DW_TAG_pointer_type) { tags.push_back(t); }
				if (t == dwarf::DW_TAG_const_type  ) { tags.push_back(t); }
				if (t == dwarf::DW_TAG_typedef     ) { tags.push_back(t); break; }
				Metadata *next = a->getBaseType();
				if (next == nullptr)  { break; } // no basetype property here, bailing
				md = next;
			}

		}

		DIType *type = cast<DIType>(md);
		//std::reverse(tags.begin(), tags.end());  

		VariableInfo ret = {"", "", false, false, false, false, false};
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
			else { lhs += getTypeString(cast<DIType>(rettypeinfo), "").type; }

			// get function's arguments' types
			rhs += '(';
			if (types.size() == 1) { rhs += "void)"; } // we have 0 input arguments...
			for (unsigned i = 1; i < types.size(); i++) {
				rhs += getTypeString(cast<DIType>(types[i]), "").type;	
				if (i <  types.size() - 1) { rhs += ", ";}
				if (i == types.size() - 1) { rhs += ")"; }
			}

			// is function pointer constant or/and actually a pointer?
			for (unsigned& t: tags) {
				switch (t) {
					case dwarf::DW_TAG_pointer_type: { typestr = " * " + typestr;     break; }
					case dwarf::DW_TAG_const_type:   { typestr = " const " + typestr; break; }
				}
			}

			if (tags.size() != 0 && tags[0] == dwarf::DW_TAG_const_type) { ret.isconstq = true; }
			ret.type = lhs + "( " + typestr + variablename.str() + " )" + rhs;
			ret.isfunptr = true;
			ret.typehasname = true;
			return ret; 
		}

		// depending on the type, we might need to add basetype name before or after 
		bool baseTypeAdded   = false;	
		std::string baseType = (type->getName().size() == 0) ? " void " : " " + type->getName().str() + " ";

		for (unsigned& t: tags) {
			switch (t) {
				case dwarf::DW_TAG_pointer_type:     { typestr = " * " + typestr; break; }
				case dwarf::DW_TAG_structure_type:   { typestr = "struct" + baseType + typestr; baseTypeAdded = true; break; }
				case dwarf::DW_TAG_union_type:       { typestr = "union"  + baseType + typestr; baseTypeAdded = true; break; }
				case dwarf::DW_TAG_enumeration_type: { typestr = "enum "  + baseType + typestr; baseTypeAdded = true; break; }
				case dwarf::DW_TAG_typedef:          { typestr = baseType + typestr; baseTypeAdded = true; break; }
				case dwarf::DW_TAG_const_type:       { typestr = "const " + typestr; break; }
				case dwarf::DW_TAG_array_type:       { 
					ret.typehasname = true;
					DINodeArray rangelist = ranges.back(); ranges.pop_back();
					typestr = " ( " + typestr + " " + variablename.str() + " ) ";
					for (auto elem = rangelist.begin(); elem != rangelist.end(); ++elem) {
						if (auto a = cast<DISubrange>(*elem)) {
							typestr += " [" + std::to_string(a->getCount()) + "] ";
						}
					}
				}
			}
		}

		if (!baseTypeAdded) { typestr = baseType + typestr; }
		// const qualified variables and arrays do not have to be restored.
		if (tags.size() != 0 && tags[0] == dwarf::DW_TAG_const_type) { ret.isconstq = true; }
		if (tags.size() != 0 && tags[0] == dwarf::DW_TAG_array_type) { ret.isarrayt = true; }
		ret.type = typestr;
		return ret; 
	}

	// self-explanatory. 
	static std::string getFunctionReturnType(const Function *F) {
		DISubprogram *SP = cast<DISubprogram>(F->getMetadata(0));
		if (auto *ST = dyn_cast<DISubroutineType>(SP->getRawType())) {
			Metadata *M = ST->getTypeArray()[0];
			if (!M) { return std::string("void"); }
			return getTypeString(cast<DIType>(M), "").type;
		}

		return std::string("unknown");
	}

	static VariableInfo getVariableInfo(Value *V) {
		Metadata *M = getMetadata(V);
		if (!M) { return {"", "", false, false, false, false}; }
		DIVariable *DI = cast<DIVariable>(M);
		auto varinfo = getTypeString(cast<DIType>(DI->getRawType()), DI->getName());
		varinfo.name = DI->getName().str();

		// variable is static. 
		if (auto *a = dyn_cast<GlobalVariable>(V)) {
			if (!a->isConstant() && a->hasInternalLinkage()) { varinfo.isstatic = true; }
		}

		return varinfo;
	}
	
	static void writeVariableInfo(VariableInfo& info, bool isOutputVar, std::ofstream& out) {
		if (info.name.length() == 0) { return; }
		out << XMLOpeningTag("variable", 1);
		out << XMLElement("name", info.name, 2);
		out << XMLElement("type", info.type, 2);
		if (isOutputVar)      { out << XMLElement("isoutput", true, 2);    }
		if (info.typehasname) { out << XMLElement("typehasname", true, 2); }
		if (info.isfunptr) { out << XMLElement("isfunptr", true, 2); }
		if (info.isconstq) { out << XMLElement("isconstq", true, 2); }
		if (info.isstatic) { out << XMLElement("isstatic", true, 2); }
		if (info.isarrayt) { out << XMLElement("isarrayt", true, 2); }
		out << XMLClosingTag("variable", 1);
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
			std::string outfilename = generateFilename(F, R);
			AreaLoc regionBounds = getRegionLoc(R);
			AreaLoc functionBounds = getFunctionLoc(F);
			DenseSet<int> regionExit = regionGetExitingLocs(R);

			DenseSet<ValuePair> constants = findBasicConstants(F, functionBounds);
			DenseSet<BasicBlock *> successors = collectSuccessorBasicBlocks(R);

			DenseSet<Value *> inputargs;
			DenseSet<Value *> outputargs;

			DenseSet<Value *> inputprevious;
			DenseSet<Value *> outputprevious;

			// find inputs / outputs.
			for (BasicBlock *BB: R->blocks()) 
			for (Instruction& I: BB->getInstList()) {
				findInputs(&I, functionBounds, regionBounds, constants, inputprevious, inputargs); 
			}

			for (BasicBlock *BB: successors)
			for (Instruction& I: BB->getInstList()) {
				findOutputs(&I, functionBounds, regionBounds, constants, outputprevious, outputargs); 
			}

			//write collected info using xml-like format
			std::ofstream outfile;
			outfile.open(OutDirectory + outfilename + ".xml", std::ofstream::out);
			outfile << XMLOpeningTag("extractinfo", 0);
			writeLocInfo(regionBounds, "region", outfile);
			writeLocInfo(functionBounds, "function", outfile);

			// dump variable info...
			for (Value *V : inputargs)  { 
				VariableInfo info = getVariableInfo(V);
				writeVariableInfo(info , false, outfile); 
			}

			for (Value *V : outputargs) { 
				VariableInfo info = getVariableInfo(V);
				writeVariableInfo(info, true,  outfile); 
			}

			// dump region exit locs
			for (int& i : regionExit)   { outfile << XMLElement("regionexit", i, 1); }
			outfile << XMLElement("funcreturntype", getFunctionReturnType(F), 1);
			outfile << XMLElement("funcname", outfilename, 1);
			outfile << XMLElement("toplevel", R->isTopLevelRegion(), 1);
			outfile << XMLClosingTag("extractinfo", 0);
			outfile.close();

			return false;
		}
	};
}

char FuncExtract::ID = 0;
static RegisterPass<FuncExtract> X("funcextract", "Func Extract", true, true);
