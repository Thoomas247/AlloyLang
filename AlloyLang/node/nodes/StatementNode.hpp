#pragma once

#include "../Node.hpp"

namespace AlloyCompiler
{
	struct FOR_LOOP
	{
		EXPRESSION* pInitialization;
		EXPRESSION* pCondition;
		EXPRESSION* pIncrement;
		STATEMENT* pBody;
	};

	struct WHILE_LOOP
	{
		EXPRESSION* pCondition;
		STATEMENT* pStatement;
	};

	struct IF_STATEMENT 
	{
		EXPRESSION* pCondition;
		STATEMENT* pStatement;
		STATEMENT* pElseStatement;
	};

	struct STATEMENT_BLOCK
	{
		std::vector<STATEMENT*> Statements;
	};

	struct RETURN
	{
		EXPRESSION* pValue;
	};
}