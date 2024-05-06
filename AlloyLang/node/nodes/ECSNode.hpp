#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct STATEMENT_BLOCK;
	struct TYPE;
	struct QUERYABLE;

	struct NAMED_COMPONENT
	{
		const std::string_view Name;
	};

	struct NAMED_COMPONENT_DEFINITION
	{
		const std::string_view Name;
		const TYPE* pType;
	};

	struct NAMED_RESOURCE
	{
		const std::string_view Name;
	};

	struct NAMED_RESOURCE_DEFINITION
	{
		const std::string_view Name;
		const TYPE* pType;
	};

	struct NAMED_QUERY
	{
		const std::string_view Name;
	};

	struct NAMED_QUERY_DEFINITION
	{
		const std::string_view Name;
		const std::vector<QUERYABLE*> Members;
	};

	struct NAMED_SYSTEM
	{
		const std::string_view Name;
	};

	struct NAMED_SYSTEM_DEFINITION
	{
		const std::string_view Name;
		const std::vector<NAMED_QUERY*> Queries;
		const STATEMENT_BLOCK* pBody;
	};

	struct NAMED_GROUP
	{
		const std::string_view Name;
	};

	struct NAMED_GROUP_DEFINITION
	{
		const std::string_view Name;
		const std::vector<NAMED_SYSTEM*> Systems;
	};

	struct NAMED_GROUP_LIST
	{
		const std::vector<NAMED_GROUP*> Groups;
	};

	struct APPLICATION_DEFINITION
	{
		const std::string_view Name;
		const std::vector<NAMED_GROUP_LIST*> GroupLists;
	};
}