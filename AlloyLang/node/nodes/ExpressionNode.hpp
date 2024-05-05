#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct NAMED_TYPE;
	struct NAMED_VARIABLE;

	struct LITERAL : PRIMARY
	{

	};

	struct CONSTRUCTOR : PRIMARY
	{
		const NAMED_TYPE* pType;
		const std::vector<std::pair<std::string_view, EXPRESSION*>> Arguments;
	};

	struct POINTER_INIT : PRIMARY
	{
		const EXPRESSION* pValue;
		const EXPRESSION* pSize;	// nullptr if not an array
	};

	struct POINTER_MOVE : PRIMARY
	{
		const NAMED_VARIABLE* pVariable;
	};

	struct INITIALIZER_LIST : PRIMARY
	{
		const std::vector<EXPRESSION*> Values;
	};

	struct ENCLOSED_EXPRESSION : PRIMARY
	{
		const EXPRESSION* pExpression;
	};

	struct ARRAY_ACCESS : POSTFIX
	{
		const EXPRESSION* pArray;
		const EXPRESSION* pIndex;
	};

	struct MEMBER_ACCESS : POSTFIX
	{
		const EXPRESSION* pObject;
		const std::string_view MemberName;
	};

	struct UNARY : EXPRESSION
	{
		const EXPRESSION* pValue;
	};

	struct BINARY : EXPRESSION
	{
		const TokenID OperatorTokenID;
		const EXPRESSION* pLeft;
		const EXPRESSION* pRight;
	};

	struct ASSIGNMENT : EXPRESSION, STATEMENT
	{
		const ASSIGNABLE* pAssignable;
		const EXPRESSION* pValue;
	};
}