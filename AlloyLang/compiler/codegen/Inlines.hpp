#pragma once

#include "CodeGenerator.hpp"

constexpr auto CAST_FUNCTION_NAME = "cast";
constexpr auto BIT_CAST_FUNCTION_NAME = "bit_cast";

namespace AlloyCompiler
{
	llvm::Function* generateCastFunction(ModuleTable& moduleTable, LLVMState& state, const std::string_view& fromName, const std::string_view& toName)
	{
		llvm::Type* fromType = state.NamedValues.GetType(fromName);
		llvm::Type* toType = state.NamedValues.GetType(toName);

		ASSERT(fromType != nullptr && toType != nullptr, "Invalid type names!");

		llvm::Type* returnType = toType;
		std::vector<llvm::Type*> paramTypes = { fromType };
		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);

		std::string functionName = std::string(fromName) + ":" + CAST_FUNCTION_NAME + "@" + std::string(toName);
		llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, functionName, *state.Module);

		ASSERT(function != nullptr, "Failed to generate cast function!");

		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", function);
		state.Builder->SetInsertPoint(entryBlock);

		llvm::Value* castValue = nullptr;

		if (fromType->isIntegerTy())
		{
			if (toType->isIntegerTy())
			{
				// int to int cast
			}
			else if (toType->isFloatingPointTy())
			{
				// int to float cast
			}
		}
		else if (fromType->isFloatingPointTy())
		{
			if (toType->isIntegerTy())
			{
				// float to int cast
			}
			else if (toType->isFloatingPointTy())
			{
				// float to float cast
			}
		}
		
		ASSERT(castValue != nullptr, "Invalid cast operation!");

		state.Builder->CreateRet(castValue);

		return function;
	}

	llvm::Function* generateBitCastFunction(ModuleTable& moduleTable, LLVMState& state, const std::string_view& fromName, const std::string_view& toName)
	{
		llvm::Type* fromType = state.NamedValues.GetType(fromName);
		llvm::Type* toType = state.NamedValues.GetType(toName);

		ASSERT(fromType != nullptr && toType != nullptr, "Invalid type names!");

		llvm::Type* returnType = toType;
		std::vector<llvm::Type*> paramTypes = { fromType };
		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);

		std::string functionName = std::string(fromName) + ":" + BIT_CAST_FUNCTION_NAME + "@" + std::string(toName);
		llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, functionName, *state.Module);

		ASSERT(function != nullptr, "Failed to generate bit cast function!");

		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", function);
		state.Builder->SetInsertPoint(entryBlock);

		llvm::Value* castValue = state.Builder->CreateBitCast(function->getArg(0), toType);

		ASSERT(castValue != nullptr, "Invalid cast operation!");

		state.Builder->CreateRet(castValue);

		return function;
	}
}