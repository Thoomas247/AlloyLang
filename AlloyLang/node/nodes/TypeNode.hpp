#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{

	struct NAMED_TYPE : TYPE
	{
		const std::string_view Name;
	};

	struct STRUCT_MEMBER : NODE
	{
		const std::string_view Name;
		const TYPE* pType;
	};

	struct STRUCT_TYPE : TYPE
	{
		const std::vector<STRUCT_MEMBER*> Members;
	};

	struct ARRAY_TYPE : TYPE
	{
		const TYPE* pElementType;
		const size_t Size;
	};

	struct NAMED_TYPE_DEFINITION : NODE
	{
		const std::string_view Name;
		const TYPE* pType;
	};
}