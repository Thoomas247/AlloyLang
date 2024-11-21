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
	/*static*/
	bool AlloyValue::convertValueToType(LLVMState& state, llvm::Value*& value, const AlloyType* newType)
	{
		//
		// Create an llvm instruction to convert input value from its current type to another type
		// TODO: 
		//		- Extend to include other needed types
		//		- Add a warning in case the conversion loses precision 
		//
		bool result = false;
		if (value != nullptr)
		{
			llvm::Type* oldType = value->getType();

			if (*newType == oldType) {
				return true;	// nothing to do
			}

			bool isArray;
			const AlloyType* containedType = SmartPointerClass::isSmartPointer(state, AlloyType::get(value->getType()), isArray);
			if (containedType != nullptr) {
				// case of smart pointers, load the underlying value and convert to requested type
				llvm::Value* Ptr = SmartPointerClass::get(state, value);
				oldType = containedType->llvmType;
				value = state.Builder->CreateLoad(oldType, Ptr);
				if (*newType == oldType) {
					return true;	// nothing to do
				}
				// fall through, now try to convert the loaded type into the requested type
			}

			switch (oldType->getTypeID()) {
			case llvm::Type::IntegerTyID:
			{
				switch (newType->getTypeID()) {
					//
					// int to float or double
					//
				case llvm::Type::FloatTyID:
				case llvm::Type::DoubleTyID:
					value = state.Builder->CreateSIToFP(value, newType->llvmType);
					result = true;
					break;

					//
					// int to int of another bitsize
					//
				case llvm::Type::IntegerTyID:
					value = state.Builder->CreateIntCast(value, newType->llvmType, true);
					result = true;
					break;

					//
					// int to pointer (e.g. null pointer)
					//
				case llvm::Type::PointerTyID:
					value = state.Builder->CreateIntToPtr(value, newType->llvmType);
					result = true;
					break;

				default:
					ASSERT(false, "Conversion to type is not implemented");
					break;
				}
				break;
			}

			case llvm::Type::FloatTyID:
			{
				switch (newType->getTypeID()) {
					//
					// float to double
					// 
				case llvm::Type::DoubleTyID:
					value = state.Builder->CreateFPCast(value, newType->llvmType);
					result = true;
					break;
				default:
					ASSERT(false, "Conversion to type is not implemented");
					break;
				}
				break;
			}

			case llvm::Type::DoubleTyID:
			{
				switch (newType->getTypeID()) {
					//
					// double to float
					//
				case llvm::Type::FloatTyID:
					value = state.Builder->CreateFPCast(value, newType->llvmType);
					result = true;
					break;

				default:
					ASSERT(false, "Conversion to type is not implemented");
					break;
				}
				break;
			}

			case llvm::Type::PointerTyID:
			{
				// if we have a pointer or a ref, load the underlying value
				value = state.Builder->CreateLoad(newType->llvmType, value);
				result = true;
				break;
			}

			default:
				/// ASSERT(false, "Conversion from type is not implemented");
				break;
			}
		}
		return result;
	}

	/*static*/
	bool AlloyValue::convertValueToType(LLVMState& state, AlloyValue& value, const AlloyType* newType)
	{
		bool result = convertValueToType(state, value.Value, newType);
		if (result) {
			// if the type has changed, update that in the AlloyValue
			value.Type = newType;
		}
		return result;
	}

}