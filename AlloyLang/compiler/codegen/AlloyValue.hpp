//
// The AlloyValue enhances the llvm::Value class by adding the exact type of the value as needed by the Alloy compiler
//

#pragma once

namespace AlloyCompiler
{
	class AlloyValue
	{
	public:
		AlloyValue() : Ptr(nullptr), Value(nullptr), Type(nullptr), isConst(false) {}
		AlloyValue& operator=(const AlloyValue& right) { 
			Ptr = right.Ptr; 
			Value = right.Value; 
			Type = right.Type; 
			isConst = right.isConst; 
			parentType = right.parentType;
			return *this;
		}

		static bool convertValueToType(LLVMState& state, llvm::Value*& value, AlloyType* newType);
		static bool convertValueToType(LLVMState& state, AlloyValue& value, AlloyType* newType);

		static inline llvm::ConstantInt* getConstantInt(llvm::LLVMContext& llvmContext, unsigned bits, int value) {
			return llvm::ConstantInt::get(llvmContext, llvm::APInt(bits, value, true)); 
		}

		// returns true if object has been initialized
		inline bool isValid() { return Type != nullptr && (Ptr != nullptr || Value != nullptr); }

		AlloyValue(llvm::Value* Value, AlloyType* Type, llvm::Value* Ptr = nullptr, bool isConst = false)
			: Value(Value), Type(Type), Ptr(Ptr), isConst(isConst) {}

		llvm::Value* Ptr;		// for variables, this is the pointer to the variable
								// also stores the pointer to members of structure, smart pointers, array elements, ...
		llvm::Value* Value;
		AlloyType* Type;

		// for enums and structs, this holds the type of the structure that contains the variable, needed for accurately comparing values
		AlloyType* parentType = nullptr;

		bool freeOnExit = false;// whether we should free the pointer
		bool isConst = false;	// indicates that we are pointing to constant variables
	};
}
