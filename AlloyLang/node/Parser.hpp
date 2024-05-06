#pragma once

#include "NodeAllocator.hpp"
#include "Nodes.hpp"

namespace AlloyCompiler
{
	struct NamedNodes
	{
		std::unordered_map<std::string_view, NAMED_GROUP_DEFINITION*> GroupDefinitions;
		std::unordered_map<std::string_view, NAMED_SYSTEM_DEFINITION*> SystemDefinitions;
		std::unordered_map<std::string_view, NAMED_QUERY_DEFINITION*> QueryDefinitions;
		std::unordered_map<std::string_view, NAMED_TYPE_DEFINITION*> TypeDefinitions;
		std::unordered_map<std::string_view, NAMED_FUNCTION_DEFINITION*> FunctionDefinitions;
		std::unordered_map<std::string_view, EXTERN_FUNCTION_DEFINITION*> ExternDefinitions;
	};

	NamedNodes Parse(const TokenBuffers& tokenBuffers);
}