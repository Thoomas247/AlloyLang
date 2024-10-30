#include "CodeGenerator.hpp"
#include "SmartPointerClass.hpp"

namespace AlloyCompiler
{
	std::vector<llvm::Value*> getGEPIndex(LLVMState& state, int index);
	
	/*static*/
	llvm::Value* SmartPointerClass::create(
		LLVMState& state,
		llvm::Type* PtrType,	// llvm Type as created by GetSmartPointerStruct
		llvm::Value* Ptr,		// The actual pointer that we are storing
		llvm::Value* Count		// The number of elements pointed to by the pointer
		)
	{
		llvm::AllocaInst* alloca = state.Builder->CreateAlloca(PtrType);

		if (alloca) {
			// store the actual pointer value in the first element of SmartPointerStruct
			llvm::Value* memberPtr = state.Builder->CreateStructGEP(PtrType, alloca, SmartPointerValue);
			state.Builder->CreateStore(Ptr, memberPtr);

			// store the count of elements
			memberPtr = state.Builder->CreateStructGEP(PtrType, alloca, SmartPointerElementCount);
			state.Builder->CreateStore(Count, memberPtr);

			// store the ref count (for future use)
			memberPtr = state.Builder->CreateStructGEP(PtrType, alloca, SmartPointerRefCount);
			state.Builder->CreateStore(llvm::ConstantInt::get(*state.Context, llvm::APInt(64, 1)), memberPtr);
		}

		return state.Builder->CreateLoad(PtrType, alloca);
	}
	
	/*static*/
	llvm::Value* SmartPointerClass::get(
		LLVMState& state,
		llvm::Value* Ptr		// The smart pointer structure
	)
	{
		// get the actual pointer value (first element of SmartPointerStruct)
		return state.Builder->CreateExtractValue(Ptr, SmartPointerValue, "actualptr");
		// return state.Builder->CreateLoad(llvm::PointerType::get(*state.Context, 0), actualPtr, "actualval");
	}

	//
	// store the actual pointer into the smart pointer
	//
	/*static*/
	void SmartPointerClass::set(
		LLVMState& state,
		llvm::Value* Ptr,		// The smart pointer
		llvm::Value* actualPtr	// the actual pointer to store
	)
	{
		llvm::Type* elementType = std::get<0>(state.smartPointerTypeMap[Ptr->getType()]);
		llvm::Value* memberPtr = state.Builder->CreateGEP(elementType, actualPtr, llvm::ConstantInt::get(*state.Context, llvm::APInt(32, SmartPointerValue, true)), "memberptr");
		state.Builder->CreateStore(actualPtr, memberPtr);
	}

	/*static*/
	void SmartPointerClass::setValue(LLVMState& state, llvm::Value* Ptr, llvm::Value* index, llvm::Value* value)
	{
		//
		// given a smart pointer representing an array or a single value, set the value at a specific index
		//
		ASSERT(index->getType() == llvm::IntegerType::get(*state.Context, 32), "Only 32-bit indices are supported!");
		llvm::Value* actualPtr = get(state, Ptr);
		llvm::Type* elementType = std::get<0>(state.smartPointerTypeMap[Ptr->getType()]);
		llvm::Value* memberPtr = state.Builder->CreateGEP(elementType, actualPtr, index, "memberptr");

		state.Builder->CreateStore(value, memberPtr);
	}

	/*static*/
	llvm::Value* SmartPointerClass::getValue(LLVMState& state, llvm::Value* Ptr, llvm::Value*& MemberPtr, llvm::Value* index)
	{
		//
		// given a smart pointer representing an array or a single value, get the value at a specific index
		//
		ASSERT(index->getType() == llvm::IntegerType::get(*state.Context, 32), "Only 32-bit indices are supported!");
		llvm::Value* value = nullptr;
		llvm::Value* actualPtr = get(state, Ptr);
		llvm::Type* elementType = std::get<0>(state.smartPointerTypeMap[Ptr->getType()]);
		MemberPtr = state.Builder->CreateGEP(elementType, actualPtr, index, "memberptr");
		value = state.Builder->CreateLoad(elementType, MemberPtr);
		return value;
	}
	
	/*static*/
	llvm::Type* SmartPointerClass::GetSmartPointerStruct(LLVMState& state, llvm::Type* ElementType, bool IsArray)
	{
		// 
		// SmartPointerStruct is a structure that contains the pointer, contained element type, number of elements and reference count of a pointer
		// Because the contained type can be of any type, each time a new type is requested we check if we already have such a structure defined, if not define and return the structure
		// The structures are stored in the NamedValues::Types map the same way all other types are stored. The name of the structure type is _SmartPointerStruct_[ElementType]
		// 

		std::string elementTypeName;
		llvm::raw_string_ostream rso(elementTypeName);
		ElementType->print(rso);
		std::string pointerStructName = _SmartPointerStruct_ + elementTypeName + (IsArray ? "_1" : "_0");

		// check if the same structure has already been defined, otherwise define it
		llvm::Type* SmartPointerStruct = state.NamedValues.GetType(pointerStructName);
		if (nullptr == SmartPointerStruct) {
			std::vector<llvm::Type*> memberTypes;
			memberTypes.push_back(llvm::PointerType::get(*state.Context, 0));	// pointer value
			memberTypes.push_back(llvm::Type::getInt64Ty(*state.Context));		// element count
			memberTypes.push_back(llvm::Type::getInt64Ty(*state.Context));		// ref count (for future use)
			SmartPointerStruct = llvm::StructType::create(*state.Context, memberTypes, pointerStructName, true);

			state.NamedValues.InsertType(pointerStructName, SmartPointerStruct, NamedValues::UserDefinedType::structure);
			state.smartPointerTypeMap[SmartPointerStruct] = std::make_tuple(ElementType, IsArray);
		}

		return SmartPointerStruct;
	}

}