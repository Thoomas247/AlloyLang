#include "CodeGenerator.hpp"
#include "LibraryFunctions.hpp"

//
// LibraryFunctions.cpp contains various procedures to generate llvm functions that can be called from various code locations to reduce code repetition
//

namespace AlloyCompiler
{
	extern llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function, const std::string_view& varName, llvm::Type* type, int numElements = 0);

	llvm::Function* generateStructureComparisonFunction(ModuleTable& moduleTable, LLVMState& state) {
		//
		// Generate a function named _CreateStructCompare_ to compare two structures or enumerations
		// Function signature : Value* _CreateStructCompare_(Value* Left, Value* Right)
		//
		
		llvm::Type* returnType = nullptr;
		std::vector<llvm::Type*> paramTypes;
		llvm::FunctionType* functionType = nullptr;

		// check if function already exists and return it
		llvm::Function* function = state.Module->getFunction(StructCompareFunctionName);
		if (function) {
			goto exit;
		}

		// generate the function declaration

		// set the return type
		returnType = llvm::IntegerType::get(*state.Context, 1);

		// function takes two pointers and twp pointer sizes as parameters
		paramTypes.push_back(llvm::PointerType::get(*state.Context, 0));
		paramTypes.push_back(llvm::IntegerType::get(*state.Context, 64));
		paramTypes.push_back(llvm::PointerType::get(*state.Context, 0));
		paramTypes.push_back(llvm::IntegerType::get(*state.Context, 64));

		functionType = llvm::FunctionType::get(returnType, paramTypes, true);
		function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, StructCompareFunctionName, *state.Module);

		if (function != nullptr) {
			// push a new scope for the function
			state.NamedValues.PushScope(StructCompareFunctionName);

			// give names to the parameters
			function->getArg(0)->setName("Left");
			function->getArg(1)->setName("LeftSize");
			function->getArg(2)->setName("Right");
			function->getArg(3)->setName("RighSize");

			// create a new builder for this function, this will allow us to generate multiple functions in parallel
			llvm::IRBuilder<>* FuncBuilder = new llvm::IRBuilder<>(*state.Context);

			// create a new basic block to start insertion into
			llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", function);
			FuncBuilder->SetInsertPoint(entryBlock);

			// create an allocation for the return value
			llvm::AllocaInst* retInst = createEntryBlockAlloca(function, "ret", returnType);
			// default return value is false
			FuncBuilder->CreateStore(llvm::ConstantInt::get(*state.Context, llvm::APInt(1, 0)), retInst);

			/* create allocations for function arguments
			for (size_t index = 0; index < function->arg_size(); index++)
			{
				llvm::Argument* arg = function->getArg(index);
				llvm::Type* subType = nullptr;
				llvm::AllocaInst* allocaInst = nullptr;

				// create an alloca for this variable
				allocaInst = createEntryBlockAlloca(function, arg->getName().str(), arg->getType());

				// store the initial value into the alloca
				FuncBuilder->CreateStore(arg, allocaInst);

				// add arguments to named values
				state.NamedValues.InsertValue(std::string(arg->getName()), allocaInst, subType, false, false);
			}
			*/

			llvm::BasicBlock* MemCmpBlock = llvm::BasicBlock::Create(*state.Context, "exit", function);

			// every function has an exit block for cleanup and setting the return value
			llvm::BasicBlock* FuncExitBlock = llvm::BasicBlock::Create(*state.Context, "exit", function);

			// first make sure the struct sizes are equal
			llvm::Value* cmp = FuncBuilder->CreateICmpEQ(function->getArg(1), function->getArg(3));
			// set the return value
			FuncBuilder->CreateStore(cmp, retInst);
			FuncBuilder->CreateCondBr(cmp, FuncExitBlock, MemCmpBlock);
			
			FuncBuilder->SetInsertPoint(MemCmpBlock);

			// size is the same, continue by comparing memory
			// ...
			FuncBuilder->CreateBr(FuncExitBlock);

			FuncBuilder->SetInsertPoint(FuncExitBlock);
			FuncBuilder->CreateRet(FuncBuilder->CreateLoad(returnType, retInst));
			
			state.NamedValues.PopScope();
			delete FuncBuilder;
		}
		else {
			ASSERT(false, "Check the code used to create the function declaration!");
		}

	exit:
		return function;
	}

}