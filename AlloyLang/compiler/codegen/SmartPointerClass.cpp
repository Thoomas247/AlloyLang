#include "llvm/llvm.hpp"
#include "NamedValues.hpp"
#include "CodeGenerator.hpp"
#include "AlloyType.hpp"
#include "AlloyValue.hpp"
#include "LibraryFunctions.hpp"
#include "Inlines.hpp"
#include "SmartPointerClass.hpp"
#include "../../log/Log.hpp"

namespace AlloyCompiler
{
	std::vector<llvm::Value*> getGEPIndex(LLVMState& state, int index);
	
	/*static*/
	AlloyValue SmartPointerClass::create(
		LLVMState& state,
		AlloyType* alloyType,	// Type as created by GetSmartPointerStruct
		llvm::Value* Ptr,		// The actual pointer that we are storing
		llvm::Value* Count		// The number of elements pointed to by the pointer
		)
	{
		llvm::Type* PtrType = alloyType->llvmType;
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

		return AlloyValue(state.Builder->CreateLoad(PtrType, alloca), alloyType, alloca);
	}
	
	/*static*/
	llvm::Value* SmartPointerClass::get(
		LLVMState& state,
		llvm::Value* Ptr		// The smart pointer structure
	)
	{
		// get the actual pointer value (first element of SmartPointerStruct)
		return state.Builder->CreateExtractValue(Ptr, SmartPointerValue, "actualptr");
		// return state.Builder->CreateLoad(AlloyType::getPointerType("i8"), actualPtr, "actualval");
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
		llvm::Value* memberPtr = state.Builder->CreateGEP(elementType, actualPtr, AlloyValue::getConstantInt(*state.Context, 32, SmartPointerValue), "memberptr");
		state.Builder->CreateStore(actualPtr, memberPtr);
	}

	/*static*/
	void SmartPointerClass::setValue(LLVMState& state, llvm::Value* Ptr, llvm::Value* index, llvm::Value* value)
	{
		//
		// given a smart pointer representing an array or a single value, set the value at a specific index
		//
		ASSERT(index->getType() == AlloyType::get("i32")->llvmType, "Only 32-bit indices are supported!");
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
		ASSERT(index->getType() == AlloyType::get("i32")->llvmType, "Only 32-bit indices are supported!");
		llvm::Value* value = nullptr;
		llvm::Value* actualPtr = get(state, Ptr);
		llvm::Type* elementType = std::get<0>(state.smartPointerTypeMap[Ptr->getType()]);
		MemberPtr = state.Builder->CreateGEP(elementType, actualPtr, index, "memberptr");
		value = state.Builder->CreateLoad(elementType, MemberPtr);
		return value;
	}
	
	/*static*/
	AlloyType* SmartPointerClass::GetSmartPointerStruct(LLVMState& state, AlloyType* ElementType, bool IsArray)
	{
		// 
		// SmartPointerStruct is a structure that contains the pointer, contained element type, number of elements and reference count of a pointer
		// Because the contained type can be of any type, each time a new type is requested we check if we already have such a structure defined, if not define and return the structure
		// The structures are stored in the NamedValues::Types map the same way all other types are stored. The name of the structure type is _SmartPointerStruct_[ElementType]
		// 

		std::string elementTypeName = ElementType->name();
		ASSERT(!elementTypeName.empty(), "Element typename cannot be empty!");
		std::string pointerStructName = _SmartPointerStruct_ + elementTypeName + (IsArray ? "_1" : "_0");

		// check if the same structure has already been defined, otherwise define it
		AlloyType* SmartPointerStruct = state.NamedValues.GetType(pointerStructName);
		if (nullptr == SmartPointerStruct) {
			std::vector<llvm::Type*> memberTypes;
			memberTypes.push_back(AlloyType::getPointerType("i8")->llvmType);	// pointer value
			memberTypes.push_back(AlloyType::get("i64")->llvmType);		// element count
			memberTypes.push_back(AlloyType::get("i64")->llvmType);		// ref count (for future use)
			SmartPointerStruct = AlloyType::get(llvm::StructType::create(*state.Context, memberTypes, pointerStructName, true));

			state.NamedValues.InsertType(pointerStructName, SmartPointerStruct, NamedValues::UserDefinedType::structure);
			state.smartPointerTypeMap[SmartPointerStruct->llvmType] = std::make_tuple(ElementType->llvmType, IsArray);
		}

		return SmartPointerStruct;
	}

}