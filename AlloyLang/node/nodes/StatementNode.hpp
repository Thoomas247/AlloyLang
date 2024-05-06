#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct EXPRESSION;
	struct STATEMENT;

	struct FOR_LOOP
	{
		const EXPRESSION* pCondition;
		const EXPRESSION* pIncrement;
		const EXPRESSION* pEnd;
		const STATEMENT* pStatement;
	};

	struct WHILE_LOOP
	{
		const EXPRESSION* pCondition;
		const STATEMENT* pStatement;
	};

	struct IF_STATEMENT 
	{
		const EXPRESSION* pCondition;
		const STATEMENT* pStatement;
		const STATEMENT* pElseStatement;
	};

	struct STATEMENT_BLOCK
	{
		const std::vector<STATEMENT*> Statements;
	};

	struct RETURN
	{
		const EXPRESSION* pValue;
	};
}