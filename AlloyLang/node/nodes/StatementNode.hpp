#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct EXPRESSION;

	struct FOR_LOOP : STATEMENT
	{
		const EXPRESSION* pCondition;
		const EXPRESSION* pIncrement;
		const EXPRESSION* pEnd;
		const STATEMENT* pStatement;
	};

	struct WHILE_LOOP : STATEMENT
	{
		const EXPRESSION* pCondition;
		const STATEMENT* pStatement;
	};

	struct IF_STATEMENT : STATEMENT
	{
		const EXPRESSION* pCondition;
		const STATEMENT* pStatement;
		const STATEMENT* pElseStatement;
	};

	struct STATEMENT_BLOCK : STATEMENT
	{
		const std::vector<STATEMENT*> Statements;
	};

	struct RETURN : STATEMENT
	{
		const EXPRESSION* pValue;
	};
}