#include "llvm/Support/raw_ostream.h"
#include "llvm/PassSupport.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Analysis/RegionPass.h"
#include "llvm/Analysis/RegionInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/CFG.h"
#include <utility>
#include <string>
#include <bitset>
#include <llvm/IR/DebugInfo.h>

#define DEBUGTEST 0
using namespace llvm;



//if (auto *op = dyn_cast<TerminatorInst>(instrit)) {   // use this for value 
namespace { 

	class VariableTable { 
private:
		DenseMap<Value *, size_t>  *m_colIndexes;
		DenseMap<BasicBlock *, BitVector> *m_rows;
		size_t m_numCols  = 4;
		size_t m_colsUsed = 0;

		void resizeBitVectors(size_t size) {
			for (auto it = m_rows->begin(); it != m_rows->end(); ++it) {
				it->getSecond().resize(size);
			}
		}

		size_t getColumnIndex(Value *obj) {

			auto it = m_colIndexes->find(obj);
			if (it != m_colIndexes->end()) { return it->getSecond(); }

			// this is the first time we have seen this value, choose an index for it 
			// and insert into appropriate hashmap 
			size_t idx = m_colsUsed++;	
			m_colIndexes->insert(std::pair<Value *, size_t>(obj, idx));
			
			// have to resize vectors when number of values is greater than bitvectors' capacities. 
			if (m_colsUsed == m_numCols) {
				m_numCols *= 2;
				resizeBitVectors(m_numCols);
			}

			return idx;
		}

		BitVector& getRow(BasicBlock *obj) {
			auto it = m_rows->find(obj);
			if (it != m_rows->end())  { return it->getSecond(); }

			BitVector v(m_numCols);
			m_rows->insert(std::pair<BasicBlock *, BitVector>(obj, v));
			return m_rows->find(obj)->getSecond();
		}

public:
		VariableTable(void): m_colIndexes(new DenseMap<Value *, size_t>()), 
							  m_rows(new DenseMap<BasicBlock *, BitVector>())  
							  { }
		
		~VariableTable(void) { 
			delete m_colIndexes;
			delete m_rows;
		}
			
		void set(BasicBlock *row, Value *col) {
			BitVector& vec = getRow(row);
			size_t  colidx = getColumnIndex(col);
			vec.set(colidx);
		}
		
		bool get(BasicBlock *row, Value *col) {
			if (m_rows->find(row) == m_rows->end()) { return false; }
			if (m_colIndexes->find(col) == m_colIndexes->end()) { return false; }
			BitVector& vec = getRow(row);
			size_t  colidx = getColumnIndex(col);
			return vec.test(colidx);
		}

		void dump(void) {
			for (auto it = m_rows->begin(); it != m_rows->end(); ++it) {
				errs() << it->getFirst()->getName() << " ";
				BitVector& vec = it->getSecond();
				for (size_t i = 0; i < m_numCols; i++) {
					errs() << vec.test(i) << " ";
				}
				errs() << "\n";
			}
		}
	};

//TODO TESTS SHOULD BE IN THEIR OWN UNITTEST DIR. 
#if DEBUGTEST == 1 
#if 0
	void runTest1(void) {
		errs() << "Running Tests...\n";
		VariableTable<int, int> *table = new VariableTable<int, int>(2, 3);
		table->set(11, 1);
		table->set(11, 2);
		table->set(11, 1);
		table->set(11, 1);
		table->set(11, 1);
		table->set(22, 3);
		table->set(11, 3);
		table->dump();
		delete table;

		errs() << "Done running tests...\n";

	}

	void runTest2(void) {
		int rints[3] = {11, 22, 33};
		int cints[4] = {1, 2, 3, 4};
		VariableTable<int *, int *> *table = new VariableTable<int *, int *>(3, 4);
		table->set(&rints[0], &cints[0]);
		table->set(&rints[0], &cints[0]);
		table->set(&rints[2], &cints[0]);
		table->set(&rints[1], &cints[0]);
		table->set(&rints[1], &cints[3]);
		table->dump();
		delete table;
	}
#endif 

	void runTest1(void) {
		errs() << "Running Test1...\n";

		VariableTable *t = new VariableTable();


		LLVMContext context;
		BasicBlock *b1 = BasicBlock::Create(context);
		BasicBlock *b2 = BasicBlock::Create(context);
		BasicBlock *b3 = BasicBlock::Create(context);
		BasicBlock *b4 = BasicBlock::Create(context);
		Value *val1 = new Argument(Type::getInt32Ty(context));
		Value *val2 = new Argument(Type::getInt32Ty(context));
		Value *val3 = new Argument(Type::getInt32Ty(context));
		Value *val4 = new Argument(Type::getInt32Ty(context));
		Value *val5 = new Argument(Type::getInt32Ty(context));
		Value *val6 = new Argument(Type::getInt32Ty(context));

		t->set(b1, val1);
		t->set(b1, val1);
		t->set(b1, val4);
		t->set(b1, val3);
		t->set(b2, val2);
		t->set(b2, val1);
		t->set(b3, val1);
		t->set(b3, val2);
		t->set(b3, val5);
		t->dump();
		
		errs() << t->get(b4, val6) << "\n";
		errs() << t->get(b3, val6) << "\n";
		errs() << t->get(b4, val1) << "\n";

		errs() << "Done Test1...\n";
	}

	void runTest2(void) {
		errs() << "Running Test2...\n";

		errs() << "Done Test2...\n";
	}
#endif


	void constructVarTable(VariableTable *VT, Function *F) {
		static int name = 0;
		for (auto blockit = F->begin(); blockit != F->end(); ++blockit) {
			BasicBlock *BB = (&*blockit);
			if (BB->getName().size() == 0) {
				BB->setName(std::to_string(name));
				name++;
			}

			for (auto instit = BB->begin(); instit != BB->end(); ++instit) {
				Instruction *I = (&*instit);
				for (auto userit = I->user_begin(); userit != I->user_end(); ++userit) {
					User *U = (*userit);	
					if (Instruction *destinstr = dyn_cast<Instruction>(U)) {
						//destinstr->dump();
						BasicBlock *userparentblock = destinstr->getParent();
						VT->set(userparentblock, I);
					}
				}
			}
		}
	}


	struct FuncExtract : public RegionPass {
		static char ID;
		VariableTable *vars;
		bool initialized = false;
		FuncExtract() : RegionPass(ID) {  }


		bool runOnRegion(Region *R, RGPassManager &RGM) override {
			return false;
		}

		bool doInitialization(Region *R, RGPassManager &RGM) override { 
			if (!initialized) {
#if DEBUGTEST == 1
				initialized = true;
				runTest1();
#else 

				initialized = true;
				vars = new VariableTable();
				constructVarTable(vars, R->getEntry()->getParent());
				vars->dump();
				errs() << "Initialziing\n";
#endif

				// initialization logic goes here
			}
			return false;
		}

		bool doFinalization(void) override { 
			initialized = false;
			delete vars;
			errs() << "Finalizing\n";
			return false;
		}
	};
}



#if 0


	struct FuncExtract : public RegionPass {
		static char ID;
		DenseMap<BasicBlock *, bool> *processed;
		DenseSet<BasicBlock *> *blaa;
		FuncExtract() : RegionPass(ID) {  }

		bool runOnRegion(Region *region, RGPassManager &RGM) override { 

			Function *fun = region->getEntry()->getParent();
			// estimate the size of the bitmatrix 
			size_t tableSize = 0;
			for (auto blockit = fun->begin(); blockit != fun->end(); ++blockit) {
				BasicBlock *bl = &*blockit;
				tableSize += bl->size();
			}





			Module *module = fun->getParent();
			for (auto global_it  = module->global_begin(); 
					  global_it != module->global_end();
					  ++global_it)  {
				GlobalVariable *global = &*global_it;
				for (auto user_it  = global->user_begin(); 
						  user_it != global->user_end();
						  ++user_it ) {
					//TODO is the user in proper function? 
					if (Instruction *destinstr = dyn_cast<Instruction>(*user_it)) {
					if (destinstr->getFunction() == fun) {
						destinstr->dump();
					}
						
					}
				}
			}
#if 0
			for (auto block_it = region->block_begin(); block_it != region->block_end(); ++block_it) {
				BasicBlock *pred = *block_it; 

#if 0
				for (auto pred_it = pred_begin(pred); pred_it != pred_end(pred); ++pred_it){
					(*pred_it)->dump();
					errs() << "-------\n";
				}

					errs() << "-------\n";
#endif



			for (auto instr_it = (*block_it)->begin(); instr_it != (*block_it)->end(); ++instr_it) {
				instr_it->dump(); 
				BasicBlock *b = instr_it->getParent();
				errs() << "user dump\n";

			for (auto use_it = instr_it->user_begin();  use_it != instr_it->user_end(); ++use_it) {
				User *use = *use_it;
				use->dump();
				if (Instruction *instr = dyn_cast<Instruction>(use)) {
					errs() << "Yaaay i am instruction \n";
					BasicBlock *parent = instr->getParent();
				}
			
					errs() << "-------\n";
				}

			errs() << "-------\n";
			errs() << "-------\n";
			}
		}
#endif

			return false;
		}

		bool doInitialization(Region *region, RGPassManager &RGM) override { 
			static bool initialized = false;
			if (!initialized) {
				initialized = true;
#if DEBUGTEST == 1
				runTest1();
				runTest2();
#else
				BitVector *bits = new BitVector(90);
				bits->set(12);
				errs() << bits->count() << " :: size \n";	



				errs() << "Initializing\n";
#endif

			}

			return false;
		}

		bool doFinalization() override {
			//errs() << "Finalizing...\n";
			//delete processed;
			return false;
		}
	};
}

#if 0

namespace { 
	//REGION PASS
	struct FuncExtract : public FunctionPass {
		static char ID;
		DenseMap<Value *, int> *map;	
		FuncExtract() : FunctionPass(ID) { }

		bool runOnFunction(Function &f) override {
			errs().write_escaped(f.getName());
			errs() << "\n";


			DenseMap<Value *, int> *result;
			DenseMap<Value *, int> *operands;
			result = new DenseMap<Value *, int>();
			operands = new DenseMap<Value *, int>();

			for (auto blockit = f.begin(); blockit != f.end(); ++blockit) 
			for (auto instrit = blockit->begin(); instrit != blockit->end(); ++instrit)  {
				Value *instr = &*instrit; 
				DenseMapIterator<Value *, int> found = result->find(instr);

				if (found == result->end()) {
					std::pair<Value *, int> p(instr, 1);	
					if (!instr->getType()->isVoidTy()) 
						result->insert(p); 
				} else {
					std::pair<Value *, int> p(instr, found->getSecond() + 1);	
					result->erase(instr);
					result->insert(p);
				}

				errs() << "Done with instruction \n";
				for (auto operand_it = instrit->op_begin(); operand_it != instrit->op_end(); ++ operand_it) {
					found = result->find(*operand_it);
					if (found != result->end()) {
						errs() << "Doing something with: " << (*operand_it)->getName() << " \n";
					}
				}
			}


#if 0
			for (auto operand_it = instrit->op_begin(); operand_it != instrit->op_end(); ++ operand_it) {
				DenseMapIterator<Value *, int> x = map->find(*operand_it);
				if (x == map->end()) {
					std::pair<Value *, int> p(*operand_it, 1);	
					map->insert(p); 
					errs() << "Inserting: " << (*operand_it)->getName() << "\n";
					continue;
				}

				std::pair<Value *, int> p(*operand_it, x->getSecond() + 1);	

				errs() << "Inserting: " << (*operand_it)->getName() << " " << x->getSecond() + 1 <<  "\n";
				map->erase(*operand_it);
				map->insert(p);
			}
				errs() << "Done\n";
			}
#endif

			for (auto result_it = result->begin(); result_it != result->end(); ++result_it) {
				errs() << result_it->getFirst()->getName() << " " << result_it->getSecond() << "\n";

			}

			
			delete result;
			delete operands;
			return false;
		}

		bool doInitialization(Module &m) override { 
			errs() << "Initialzing\n";
			map = new DenseMap<Value *, int>();
			return false;
		}

		bool doFinalization(Module &m) override {
			errs() << "Finalizing...\n";

			delete  map;
			return false;

		}
	};
}
#endif
#endif

char FuncExtract::ID = 0;
static RegisterPass<FuncExtract> X("funcextract", "Func Extract", false, false);
