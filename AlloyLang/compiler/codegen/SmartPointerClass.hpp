//
// Class SmartPointerClass is a collection of static functions that handle arrays and pointers allocated using the new operator
// Because llvm does not support arrays of complex types (e.g. arrays of structures), we are not using the llvm vector types but using the objects allocated by this class
// The smart pointers are actually a structure that contains:
// - The allocated pointer
// - The number of elements that are allocated
// - A reference counter (for future use)
//
#pragma once

#include "llvm/llvm.hpp"

// this is the prefix for the smart pointer structure name and the indices to access the elements of the structure
#define _SmartPointerStruct_	"_SmartPointerStruct_"
#define SmartPointerValue	0
#define SmartPointerElementCount	1
#define SmartPointerRefCount	2

namespace AlloyCompiler
{
	class SmartPointerClass
	{
	public:
		static AlloyValue create(
			LLVMState& state,
			AlloyType* PtrType,	// llvm Type as created by GetSmartPointerStruct
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
		static AlloyType* GetSmartPointerStruct(LLVMState& state, AlloyType* ElementType, bool IsArray);

		// check if given type is a smart pointer and return the pointed to type
		static AlloyType* isSmartPointer(LLVMState& state, AlloyType* alloyType, bool& isArray) {
			if (alloyType && state.smartPointerTypeMap.contains(alloyType->llvmType)) {
				std::tuple<llvm::Type*, bool> tuple = state.smartPointerTypeMap[alloyType->llvmType];
				isArray = std::get<1>(tuple);
				return AlloyType::get(std::get<0>(tuple));
			}
			else {
				return nullptr;
			}
		}
	};
}
