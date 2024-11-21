#include "llvm/llvm.hpp"
#include "NamedValues.hpp"
#include "CodeGenerator.hpp"
#include "AlloyType.hpp"
#include "AlloyValue.hpp"
#include "LibraryFunctions.hpp"
#include "Inlines.hpp"
#include "SmartPointerClass.hpp"

//
// LibraryFunctions.cpp contains various procedures to generate llvm functions that can be called from various code locations to reduce code repetition
//

namespace AlloyCompiler
{
	extern llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function, const std::string_view& varName, const AlloyType* type, int numElements = 0);

	llvm::Function* generateMemCmpFunctionDeclaration(const ModuleTable& moduleTable, LLVMState& state) {
		//
		// Generate a the declaration for a function named memcmp to call the standard memcmp function
		// Function signature : Value* memcmp(Value* Left, int64 LeftSize, Value* Right, int64 RightSize)
		//

		llvm::Type* returnType = nullptr;
		std::vector<llvm::Type*> paramTypes;
		llvm::FunctionType* functionType = nullptr;

		// check if function already exists and return it
		llvm::Function* function = state.Module->getFunction(MemCmpFunctionName);
		if (function) {
			goto exit;
		}

		// generate the function declaration

		// set the return type
		returnType = AlloyType::get("i32")->llvmType;

		// function takes two pointers and one size as parameters
		paramTypes.push_back(AlloyType::getPointerType("i8")->llvmType);
		paramTypes.push_back(AlloyType::getPointerType("i8")->llvmType);
		paramTypes.push_back(AlloyType::get("i32")->llvmType);

		functionType = llvm::FunctionType::get(returnType, paramTypes, false);
		function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, MemCmpFunctionName, *state.Module);

	exit:
		return function;
	}
		
	llvm::Function* generateStructureComparisonFunction(const ModuleTable& moduleTable, LLVMState& state) {
		//
		// Generate a function named _CreateStructCompare_ to compare two structures or enumerations
		// Function signature : Value* _CreateStructCompare_(Value* Left, int64 LeftSize, Value* Right, int64 RightSize)
		//
		
		const AlloyType* returnType = nullptr;
		std::vector<llvm::Type*> paramTypes;
		llvm::FunctionType* functionType = nullptr;

		// check if function already exists and return it
		llvm::Function* function = state.Module->getFunction(StructCompareFunctionName);
		if (function) {
			goto exit;
		}

		// generate the function declaration

		// set the return type
		returnType = AlloyType::get("bool");

		// function takes two pointers and two pointer sizes as parameters
		paramTypes.push_back(AlloyType::getPointerType("i8")->llvmType);
		paramTypes.push_back(AlloyType::get("i32")->llvmType);
		paramTypes.push_back(AlloyType::getPointerType("i8")->llvmType);
		paramTypes.push_back(AlloyType::get("i32")->llvmType);

		functionType = llvm::FunctionType::get(returnType->llvmType, paramTypes, false);
		function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, StructCompareFunctionName, *state.Module);

		if (function != nullptr) {
			// create a new builder for this function, this will allow us to generate multiple functions in parallel
			llvm::IRBuilder<>* FuncBuilder = new llvm::IRBuilder<>(*state.Context);

			// create a new basic block to start insertion into
			llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", function);
			FuncBuilder->SetInsertPoint(entryBlock);

			// create an allocation for the return value
			llvm::AllocaInst* retInst = createEntryBlockAlloca(function, "ret", returnType);
			// default return value is false
			FuncBuilder->CreateStore(llvm::ConstantInt::get(*state.Context, llvm::APInt(1, 0)), retInst);

			llvm::BasicBlock* MemCmpBlock = llvm::BasicBlock::Create(*state.Context, "exit", function);
			llvm::BasicBlock* RevertResultBlock = llvm::BasicBlock::Create(*state.Context, "inverse", function);

			// every function has an exit block for cleanup and setting the return value
			llvm::BasicBlock* FuncExitBlock = llvm::BasicBlock::Create(*state.Context, "exit", function);

			// first make sure the struct sizes are equal
			llvm::Value* cmp = FuncBuilder->CreateICmpNE(function->getArg(1), function->getArg(3));
			// set the return value
			FuncBuilder->CreateCondBr(cmp, FuncExitBlock, MemCmpBlock);
			
			FuncBuilder->SetInsertPoint(MemCmpBlock);

			// size is the same, continue by comparing memory
			// retrieve the memcmp library function
			llvm::Function* memcmpfn = generateMemCmpFunctionDeclaration(moduleTable, state);
			if (memcmp != nullptr) {
				// and call it with the pointers and sizes of the two structures
				std::vector<llvm::Value*> args;
				args.push_back(function->getArg(0));	// first pointer
				args.push_back(function->getArg(2));	// second pointer
				args.push_back(function->getArg(1));	// size
				llvm::Value* result = FuncBuilder->CreateCall(memcmpfn, args);

				// memcmp returns 0 if the memory content is equal, so we need to revert the result by comparing the value to 0
				FuncBuilder->CreateCondBr(FuncBuilder->CreateICmpEQ(result, llvm::ConstantInt::get(*state.Context, llvm::APInt(result->getType()->getIntegerBitWidth(), 0))),
					RevertResultBlock, FuncExitBlock);

				// memcmp return a value different from 0, which means the structures are not equal
				FuncBuilder->SetInsertPoint(RevertResultBlock);
				FuncBuilder->CreateStore(llvm::ConstantInt::get(*state.Context, llvm::APInt(1, 1)), retInst);
			}
			else {
				ASSERT(false, "Something is wrong in the library function " MemCmpFunctionName "!");
				return nullptr;
			}

			FuncBuilder->CreateBr(FuncExitBlock);

			FuncBuilder->SetInsertPoint(FuncExitBlock);
			FuncBuilder->CreateRet(FuncBuilder->CreateLoad(returnType->llvmType, retInst));
			
			delete FuncBuilder;
		}
		else {
			ASSERT(false, "Check the code used to create the function declaration!");
		}

	exit:
		return function;
	}

}