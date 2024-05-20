#pragma once

#include "../Node.hpp"
#include "../Annotation.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct STATEMENT_BLOCK;
	struct TYPE;

	struct COMPONENT_DEFINITION
	{
		Token* pNameToken;
		TYPE* pType;

		AnnotationArgs Excludes;
	};

	struct RESOURCE_DEFINITION
	{
		Token* pNameToken;
		TYPE* pType;
	};

	struct QUERY_DEFINITION
	{
		Token* pNameToken;
		std::vector<Token*> ComponentReadNames, ComponentWriteNames;
	};

	struct SYSTEM_DEFINITION
	{
		Token* pNameToken;
		std::vector<Token*> ResourceReads, ResourceWrites, QueryNames;
		STATEMENT_BLOCK* pBody;

		bool IsInline;
	};

	struct GROUP_DEFINITION
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