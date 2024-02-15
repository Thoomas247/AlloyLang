#pragma once

#include <map>
#include <vector>
#include <string>

//
// This class holds a map of structure names to a map of struct member names with their indices
// LLVM does not store the names of the struct members in the StructType class so there is no way to access a structure member by its name
//
class NamedStructs
{
public:
	static void setMemberNames(const std::string& StructName, const std::map<std::string, int>& StructMemberNames) {
		structMembersMap[StructName] = StructMemberNames;
	}

private:
	// map from structure names to structure member names
	static std::map<std::string, std::map<std::string, int>> structMembersMap;
};

