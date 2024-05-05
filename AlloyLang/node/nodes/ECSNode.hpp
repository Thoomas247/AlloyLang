#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct STATEMENT_BLOCK;

	struct NAMED_COMPONENT : QUERYABLE
	{
		const std::string_view Name;
	};

	struct NAMED_COMPONENT_DEFINITION : DEFINITION
	{
		const std::string_view Name;
		const TYPE* pType;
	};

	struct NAMED_RESOURCE : QUERYABLE
	{
		const std::string_view Name;
	};

	struct NAMED_RESOURCE_DEFINITION : DEFINITION
	{
		const std::string_view Name;
		const TYPE* pType;
	};

	struct NAMED_QUERY : NODE
	{
		const std::string_view Name;
	};

	struct NAMED_QUERY_DEFINITION : DEFINITION
	{
		const std::string_view Name;
		const std::vector<QUERYABLE*> Members;
	};

	struct NAMED_SYSTEM : NODE
	{
		const std::string_view Name;
	};

	struct NAMED_SYSTEM_DEFINITION : DEFINITION
	{
		const std::string_view Name;
		const std::vector<NAMED_QUERY*> Queries;
		const STATEMENT_BLOCK* pBody;
	};

	struct NAMED_GROUP : NODE
	{
		const std::string_view Name;
	};

	struct NAMED_GROUP_DEFINITION : DEFINITION
	{
		const std::string_view Name;
		const std::vector<NAMED_SYSTEM*> Systems;
	};

	struct NAMED_GROUP_LIST : NODE
	{
		const std::vector<NAMED_GROUP*> Groups;
	};

	struct APPLICATION_DEFINITION : DEFINITION
	{
		const std::string_view Name;
		const std::vector<NAMED_GROUP_LIST*> GroupLists;
	};
}