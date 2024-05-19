#pragma once

#include "../Node.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct NAMED_TYPE;
	struct NAMED_VARIABLE;

	struct LITERAL
	{
		LiteralType Type;
		Token* pValueToken;
	};

	struct CONSTRUCTOR 
	{
		NAMED_TYPE* pType;
		std::vector<std::pair<Token*, EXPRESSION*>> Arguments;	// Token* contains name of member
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
		Token* pMemberNameToken;
	};

	struct UNARY
	{
		Token* pOpToken;
		EXPRESSION* pExpression;
	};

	struct BINARY
	{
		Token* pOpToken;
		EXPRESSION* pLeft;
		EXPRESSION* pRight;
	};

	struct ASSIGNMENT
	{
		EXPRESSION* pVariable;
		EXPRESSION* pValue;
	};
}