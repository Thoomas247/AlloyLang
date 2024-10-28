#pragma once

#include "llvm/llvm.hpp"

#define _SmartPointerStruct_	"_SmartPointerStruct_"
#define SmartPointerValue	0
#define SmartPointerElementCount	1
#define SmartPointerRefCount	2

namespace AlloyCompiler
{
	class SmartPointerClass
	{
	public:
		static llvm::Value* create(
			LLVMState& state,
			llvm::Type* PtrType,	// llvm Type as created by GetSmartPointerStruct
			llvm::Value* Ptr,		// The actual pointer that we are storing
			llvm::Value* Count		// The number of elements pointed to by the pointer
		);

		//
		// given a smart pointer, returns the actual pointer
		//
		static llvm::Value* get(
			LLVMState& state,
			llvm::Value* Ptr		// The smart pointer
		);

		//
		// store the actual pointer into the smart pointer
		//
		static void set(
			LLVMState& state,
			llvm::Value* Ptr,		// The smart pointer
			llvm::Value* actualPtr	// the actual pointer to store
		);

		//
		// given a smart pointer representing an array or a single value, set the value at a specific index
		//
		static void setValue(LLVMState& state, llvm::Value* Ptr, int index, llvm::Value* value) { setValue(state, Ptr, llvm::ConstantInt::get(*state.Context, llvm::APInt(32, index, true)), value); }
		static void setValue(LLVMState& state, llvm::Value* Ptr, llvm::Value* index, llvm::Value* value);

		//
		// given a smart pointer representing an array or a single value, get the value at a specific index
		//
		static llvm::Value* getValue(LLVMState& state, llvm::Value* Ptr, llvm::Value*& MemberPtr, int index) { return getValue(state, Ptr, MemberPtr, llvm::ConstantInt::get(*state.Context, llvm::APInt(32, index, true))); }
		static llvm::Value* getValue(LLVMState& state, llvm::Value* Ptr, llvm::Value*& MemberPtr, llvm::Value* index);

		// return the structure type associated with a specific pointer type
		static llvm::Type* GetSmartPointerStruct(LLVMState& state, llvm::Type* ElementType, bool IsArray);

		// check if given type is a smart pointer and return the pointed to type
		static llvm::Type* isSmartPointer(LLVMState& state, llvm::Type* llvmType, bool& isArray) {
			if (state.smartPointerTypeMap.contains(llvmType)) {
				std::tuple<llvm::Type*, bool> tuple = state.smartPointerTypeMap[llvmType];
				isArray = std::get<1>(tuple);
				return std::get<0>(tuple);
			}
			else {
				return nullptr;
			}
		}
	};
}

