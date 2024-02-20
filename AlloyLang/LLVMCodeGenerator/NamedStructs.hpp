#pragma once

#include <map>
#include <vector>
#include <string>

namespace AlloyCompiler
{
	//
	// This class holds a map of structure names to a map of struct member names with their indices
	// LLVM does not store the names of the struct members in the StructType class so there is no way to access a structure member by its name
	//
	// 	NamedStructs also contains helper methods to create mutable and unmutable structures
	//
	class NamedStructs
	{
	public:
		// create a constant structure that can only contain constant values (no assignment from other variables)
		// e.g. Vector3 { x = 0.0, y = 0.0, z = 0.0 }
		static llvm::Value* getConstantStruct(LLVMCodeGenerator& codeGen,
			NodeID id,
			const std::vector<NodeID>& memberIds,
			const std::vector<NodeID>& valueIds);

		// create a mutable structure with elements assigned by either constants or other variables
		// e.g. Vector3 { x = a, y = 0.0, z = c }
		static llvm::Value* getMutableStruct(LLVMCodeGenerator& codeGen,
			NodeID id,
			const std::vector<NodeID>& memberIds,
			const std::vector<NodeID>& valueIds);

		// load the value of one member of a structure given the structure name and member name
		static Value* loadValueOfLocalOrGlobalStructMember(LLVMCodeGenerator& codeGen,
			const std::string& VariableName, 
			const std::string& MemberName, 
			Value*& Value);

		// load the value of one member of a structure given the structure pointer and member name
		static Value* loadValueOfLocalOrGlobalStructMember(LLVMCodeGenerator& codeGen,
			Value* VariablePtr,
			const std::string& MemberName,
			Value*& Value);

		// load the value of one member of a structure given the structure pointer and member index
		static Value* loadValueOfLocalOrGlobalStructMember(LLVMCodeGenerator& codeGen,
			Value* VariablePtr,
			int MemberIndex,
			Value*& Value);

		// update the value of a structure member given the structure pointer, structure value and member name
		static bool updateValueOfLocalOrGlobalStructMember(LLVMCodeGenerator& codeGen,
			Value* StructVariable,		// this is a pointer to the structure variable
			Value *StructValue,			// this is the structure variable
			const std::string& MemberName, 
			Value* Value);

		// set the member names of a structure in our internal map as these are not stored by LLVM
		static void setMemberNames(const std::string& StructName, const std::map<std::string, int>& StructMemberNames) {
			structMembersMap[StructName] = StructMemberNames;
		}

		// retrieve the index of a member given a structure name and member name
		static int getMemberIndex(const std::string& StructName, const std::string& MemberName) {
			//
			// returns the index to the member of the structure or -1 if either the structure or member are not found
			//
			int result = -1;
			if (structMembersMap.contains(StructName)) {
				std::map<std::string, int>& StructMemberNames = structMembersMap[StructName];
				if (StructMemberNames.contains(MemberName)) {
					result = StructMemberNames[MemberName];
				}
			}

			return result;
		}

	private:
		// map from structure names to structure member names
		static std::map<std::string, std::map<std::string, int>> structMembersMap;
	};
}
