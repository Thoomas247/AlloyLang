#pragma once

#include "../Node.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct STATEMENT_BLOCK;
	struct TYPE;

	struct NAMED_COMPONENT_DEFINITION
	{
		std::string_view Name;
		TYPE* pType;

		std::vector<std::string_view> Excludes;
	};

	struct NAMED_RESOURCE_DEFINITION
	{
		std::string_view Name;
		TYPE* pType;
	};

	struct NAMED_QUERY_DEFINITION
	{
		std::string_view Name;
		std::vector<std::string_view> ComponentReadNames;
		std::vector<std::string_view> ComponentWriteNames;
	};

	struct NAMED_SYSTEM_DEFINITION
	{
		std::string_view Name;
		std::vector<std::string_view> ResourceReads;
		std::vector<std::string_view> ResourceWrites;
		std::vector<std::string_view> QueryNames;
		STATEMENT_BLOCK* pBody;

		bool IsInline;
	};

	struct NAMED_GROUP_DEFINITION
	{
		std::string_view Name;
		std::vector<std::string_view> SystemNames;
	};

	struct APPLICATION_DEFINITION
	{
		std::string_view Name;
		std::vector<std::string_view> StartGroupNames;
		std::vector<std::string_view> UpdateGroupNames;
		std::vector<std::string_view> EndGroupNames;
	};
}