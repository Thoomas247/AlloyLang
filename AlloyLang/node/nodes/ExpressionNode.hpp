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
		NAMED_TYPE* pType;
		std::vector<std::pair<std::string_view, EXPRESSION*>> Arguments;
	};

	struct POINTER_INIT
	{
		EXPRESSION* pValue;
		EXPRESSION* pSize;	// nullptr if not an array
	};

	struct POINTER_MOVE
	{
		NAMED_VARIABLE* pVariable;
	};

	struct INITIALIZER_LIST
	{
		std::vector<EXPRESSION*> Values;
	};

	struct ENCLOSED_EXPRESSION
	{
		EXPRESSION* pExpression;
	};

	struct ARRAY_ACCESS
	{
		EXPRESSION* pArray;
		EXPRESSION* pIndex;
	};

	struct MEMBER_ACCESS
	{
		EXPRESSION* pObject;
		std::string_view MemberName;
	};

	struct UNARY
	{
		EXPRESSION* pValue;
	};

	struct BINARY
	{
		TokenID OperatorTokenID;
		EXPRESSION* pLeft;
		EXPRESSION* pRight;
	};

	struct ASSIGNMENT
	{
		ASSIGNABLE* pAssignable;
		EXPRESSION* pValue;
	};
}