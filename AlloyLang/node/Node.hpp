#pragma once
#include "../log/Log.hpp"
#include "../tokenizer/Token.hpp"
#include "NodeEnums.hpp"

namespace AlloyCompiler
{
	struct NODE
	{
		virtual ~NODE() = 0;

		const TokenID RefTokenID;
	};

	struct TYPE : NODE
	{
		virtual ~TYPE() = 0;

		const TypeModifier Modifier;
	};

	struct QUERYABLE : NODE
	{
		virtual ~QUERYABLE() = 0;
	};

	struct DEFINITION : NODE
	{
		virtual ~DEFINITION() = 0;
	};

	struct EXPRESSION : NODE
	{
		virtual ~EXPRESSION() = 0;
	};

	struct PRIMARY : EXPRESSION
	{
		virtual ~PRIMARY() = 0;
	};

	struct ASSIGNABLE : NODE
	{
		virtual ~ASSIGNABLE() = 0;
	};

	struct POSTFIX : EXPRESSION, ASSIGNABLE
	{
		virtual ~POSTFIX() = 0;
	};

	struct STATEMENT : NODE
	{
		virtual ~STATEMENT() = 0;
	};
}