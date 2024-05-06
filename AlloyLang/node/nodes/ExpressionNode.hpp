#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct NAMED_TYPE;
	struct NAMED_VARIABLE;
	struct EXPRESSION;
	struct ASSIGNABLE;

	struct LITERAL
	{

	};

	struct CONSTRUCTOR 
	{
		const NAMED_TYPE* pType;
		const std::vector<std::pair<std::string_view, EXPRESSION*>> Arguments;
	};

	struct POINTER_INIT
	{
		const EXPRESSION* pValue;
		const EXPRESSION* pSize;	// nullptr if not an array
	};

	struct POINTER_MOVE
	{
		const NAMED_VARIABLE* pVariable;
	};

	struct INITIALIZER_LIST
	{
		const std::vector<EXPRESSION*> Values;
	};

	struct ENCLOSED_EXPRESSION
	{
		const EXPRESSION* pExpression;
	};

	struct ARRAY_ACCESS
	{
		const EXPRESSION* pArray;
		const EXPRESSION* pIndex;
	};

	struct MEMBER_ACCESS
	{
		const EXPRESSION* pObject;
		const std::string_view MemberName;
	};

	struct UNARY
	{
		const EXPRESSION* pValue;
	};

	struct BINARY
	{
		const TokenID OperatorTokenID;
		const EXPRESSION* pLeft;
		const EXPRESSION* pRight;
	};

	struct ASSIGNMENT
	{
		const ASSIGNABLE* pAssignable;
		const EXPRESSION* pValue;
	};
}