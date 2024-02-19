#include "llvm.hpp"
#include "LLVMCodeGenerator.hpp"
#include "NamedStructs.hpp"
#include "NamedValues.hpp"

using namespace llvm;

namespace AlloyCompiler
{

	std::map<std::string, std::map<std::string, int>> NamedStructs::structMembersMap;

	/*static*/
	llvm::Value* NamedStructs::getConstantStruct(LLVMCodeGenerator& codeGen,
		NodeID id,
		const std::vector<NodeID>& memberIds,
		const std::vector<NodeID>& valueIds) {

		Value* result = nullptr;

		// get the name of the structure
		const IDENTIFIER& identifier = codeGen.NodeBuffers.GetNode(id).Identifier;
		std::string structName(codeGen.TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

		// get the type of the structure
		llvm::StructType* structType = static_cast<llvm::StructType * >(CGNamedValues::AlloyToLLVMType(*codeGen.TheContext, codeGen.NodeBuffers, codeGen.TokenBuffers, id));
		if (nullptr == structType ||
			structType->getTypeID() != Type::StructTyID) {
			// TBD: undefined structure
			assert(false);
		}
		else {
			std::vector<Constant*> structMembers(memberIds.size());

			// go through the list of initializers and initialize all members
			for (int i = 0; i < memberIds.size(); i++) {

				// get the name of the member to initiliaze
				const IDENTIFIER& identifier = codeGen.NodeBuffers.GetNode(memberIds[i]).Identifier;
				std::string memberName(codeGen.TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

				int memberIndex = NamedStructs::getMemberIndex(structName, memberName);
				if (memberIndex != -1) {

					// evaluate the corresponding expression
					Value* value = codeGen.codegen(valueIds[i]);

					// add to the vector of values
					if (isa<Constant>(value)) {
						structMembers[i] = static_cast<Constant*>(value);
					}
					else {
						// TBD: implement case where initializer is another variable and not a constant
						assert(false);
					}

				}
				else {
					assert(false);	// TBD: referencing a non-existing member
				}
			}
			result = ConstantStruct::get(structType, ArrayRef<Constant *>(structMembers));

		}

		return result;	// result contains the whole initialized structure

	}

	/*static*/
	bool NamedStructs::loadValueOfLocalOrGlobalStructMember(LLVMCodeGenerator& codeGen,
		const std::string& Name, 
		const std::string& MemberName, 
		Value*& Value) {
		//
		// read the value of one member of a local or global structure variable
		// 

		bool ret = true;

		// check if the structure exists and load it
		llvm::Value* ptr = codeGen.loadValueOfLocalOrGlobalVariable(Name, Value);

		if (!ptr) {
			// TBD: Unknown variable name
			assert(false);
			return false;
		}
		
		// this is a structure and we are requesting the value of one member
		StructType* structType = static_cast<StructType*>(Value->getType());
		assert(structType->getTypeID() == Type::StructTyID);

		int index = NamedStructs::getMemberIndex(structType->getStructName().str(), MemberName);
		if (index != -1) {

			// convert our index to an llvm 32-bit Int
			llvm::Value* llvm_index = llvm::ConstantInt::get(*codeGen.TheContext, llvm::APInt(32, index, true));

			// struct members are accessed through 2 indices, this is described in details here:
			// https://llvm.org/docs/GetElementPtr.html
			std::vector<llvm::Value*> indices(2);
			indices[0] = llvm::ConstantInt::get(*codeGen.TheContext, llvm::APInt(32, 0, true));
			indices[1] = llvm_index;

			// case of a local structure variable
			llvm::Value* member = codeGen.Builder->CreateGEP(structType, ptr, indices, "memberptr");
			Value = codeGen.Builder->CreateLoad(structType->getTypeAtIndex(index), member, "loadtmp");
		}
		else {
			ret = false;
			assert(false);	// TBD: referencing a non-existing member
		}

		return ret;
	}

	/*static*/
	bool NamedStructs::updateValueOfLocalOrGlobalStructMember(LLVMCodeGenerator& codeGen,
		const std::string& VariableName,
		const std::string& MemberName,
		Value* Value) {
		//
		// update the value of one member a local or global structure
		//

		bool result = false;
		llvm::Value* structValue = nullptr;

		// Lookup this variable in the current function and load the previous value
		// loading the previous value (which is a structure) gives us the details about the structure
		llvm::Value* ptr = codeGen.loadValueOfLocalOrGlobalVariable(VariableName, structValue);
		if (!ptr) {
			// TBD: Unknown variable name
			assert(false);
		}

		// this is a structure and we are requesting the value of one member
		StructType* structType = static_cast<StructType*>(structValue->getType());
		assert(structType->getTypeID() == Type::StructTyID);

		int index = NamedStructs::getMemberIndex(structType->getStructName().str(), MemberName);
		if (index != -1) {
			// convert our index to an llvm 32-bit Int
			llvm::Value* llvm_index = llvm::ConstantInt::get(*codeGen.TheContext, llvm::APInt(32, index, true));

			// struct members are accessed through 2 indices, this is described in details here:
			// https://llvm.org/docs/GetElementPtr.html
			std::vector<llvm::Value*> indices(2);
			indices[0] = llvm::ConstantInt::get(*codeGen.TheContext, llvm::APInt(32, 0, true));
			indices[1] = llvm_index;

			llvm::Value* member = codeGen.Builder->CreateGEP(structType, ptr, indices, "memberptr");
			codeGen.Builder->CreateStore(Value, member, "savetmp");

			result = true;
		}
		return result;
	}

}