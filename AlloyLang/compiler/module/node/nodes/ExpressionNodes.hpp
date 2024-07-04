#pragma once

#include "NodeCommon.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct TYPE_NAME;
	struct VARIABLE;

	struct LITERAL
	{
		LiteralType Type;
		Token* pValueToken;
	};

	struct CONSTRUCTOR 
	{
		TYPE_NAME* pType;
		std::vector<std::pair<Token*, EXPRESSION*>> Arguments;	// Token* contains name of member
	};

	struct POINTER_INIT
	{
		EXPRESSION* pValue;
		EXPRESSION* pSize;	// nullptr if not an array
	};

	struct POINTER_MOVE
	{
		VARIABLE* pVariable;
	};

	struct INITIALIZER_LIST
	{
		std::vector<EXPRESSION*> Values;
	};

	struct ENCLOSED_EXPRESSION
	{
		EXPRESSION* pExpression;
	};

	struct ENUM_VALUE
	{
		TYPE_NAME* pEnumName;
		Token* pEnumValueNameToken;
		EXPRESSION* pPayloadValue;	// optional, only for enum values which can hold a payload
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