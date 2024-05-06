#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct TYPE;

	struct NAMED_TYPE
	{
		const std::string_view Name;
	};

	struct STRUCT_MEMBER
	{
		const std::string_view Name;
		const TYPE* pType;
	};

	struct STRUCT_TYPE
	{
		const std::vector<STRUCT_MEMBER*> Members;
	};

	struct ARRAY_TYPE
	{
		const TYPE* pElementType;
		const size_t Size;
	};

	struct NAMED_TYPE_DEFINITION
	{
		const std::string_view Name;
		const TYPE* pType;
	};
}