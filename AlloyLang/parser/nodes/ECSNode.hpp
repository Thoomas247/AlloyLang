#pragma once

#include "../Node.hpp"
#include "../Annotation.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct STATEMENT_BLOCK;
	struct TYPE;

	struct NAMED_COMPONENT_DEFINITION
	{
		Token* pNameToken;
		TYPE* pType;

		AnnotationArgs Excludes;
	};

	struct NAMED_RESOURCE_DEFINITION
	{
		Token* pNameToken;
		TYPE* pType;
	};

	struct NAMED_QUERY_DEFINITION
	{
		Token* pNameToken;
		std::vector<Token*> ComponentReadNames, ComponentWriteNames;
	};

	struct NAMED_SYSTEM_DEFINITION
	{
		Token* pNameToken;
		std::vector<Token*> ResourceReads, ResourceWrites, QueryNames;
		STATEMENT_BLOCK* pBody;

		bool IsInline;
	};

	struct NAMED_GROUP_DEFINITION
	{
		Token* pNameToken;
		std::vector<Token*> SystemNames;
	};

	struct APPLICATION_DEFINITION
	{
		Token* pNameToken;
		std::vector<Token*> StartGroupNames, UpdateGroupNames, EndGroupNames;
	};
}