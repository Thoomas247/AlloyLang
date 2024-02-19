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
	Value* NamedStructs::loadValueOfLocalOrGlobalStructMember(LLVMCodeGenerator& codeGen,
		const std::string& Name, 
		const std::string& MemberName, 
		Value*& Value) {
		//
		// read the value of one member of a local or global structure variable
		// returns a pointer to the structure member or nullptr
		// 

		llvm::Value* ret = nullptr;

		// check if the structure exists and load it
		llvm::Value* ptr = codeGen.loadValueOfLocalOrGlobalVariable(Name, Value);

		if (!ptr) {
			// TBD: Unknown variable name
			assert(false);
			return ret;
		}
		
		return loadValueOfLocalOrGlobalStructMember(codeGen, ptr, MemberName, Value);
	}

	/*static*/
	Value* NamedStructs::loadValueOfLocalOrGlobalStructMember(LLVMCodeGenerator& codeGen,
		Value* VariablePtr,
		const std::string& MemberName,
		Value*& Value) {
		//
		// Read the value of one member of a local or global structure variable
		// Returns a pointer to the structure member or nullptr
		// On entry: The parameter Value should contain the structure's value, it is needed in addition to the pointer to the structure in VariablePtr
		// On exit: Value contains the returned value
		// 

		llvm::Value* ret = nullptr;

		// check if the structure exists and load it
		llvm::Value* ptr = VariablePtr;

		if (!ptr) {
			// TBD: Unknown variable name
			assert(false);
			return ret;
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
			ret = codeGen.Builder->CreateGEP(structType, ptr, indices, "memberptr");
			Value = codeGen.Builder->CreateLoad(structType->getTypeAtIndex(index), ret, "loadtmp");
		}
		else {
			ret = nullptr;
			assert(false);	// TBD: referencing a non-existing member
		}

		return ret;
	}

	/*static*/
	bool NamedStructs::updateValueOfLocalOrGlobalStructMember(LLVMCodeGenerator& codeGen,
		Value* StructVariable,		// this is a pointer to the structure variable
		Value* StructValue,			// this is the structure variable
		const std::string& MemberName,
		Value* Value) {
		//
		// update the value of one member a local or global structure
		//

		bool result = false;

		// make sure we are receiving a variable of type Structure
		if (!isa<StructType>(StructValue->getType())) {
			// TBD: not a structure
			assert(false);
		}
		else {
			// this is a structure and we are requesting the value of one member
			StructType* structType = static_cast<StructType*>(StructValue->getType());
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

				llvm::Value* member = codeGen.Builder->CreateGEP(structType, StructVariable, indices, "memberptr");
				codeGen.Builder->CreateStore(Value, member, "savetmp");

				result = true;
			}
		}
		return result;
	}

}