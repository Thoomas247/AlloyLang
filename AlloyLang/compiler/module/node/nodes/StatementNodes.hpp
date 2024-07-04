#pragma once
#include "NodeCommon.hpp"
#include "VariantNodes.hpp"

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
		Token* pCaptureNameToken;	// optional, can only be used with enums
	};

	struct SWITCH_STATEMENT
	{
		EXPRESSION* pSwitchValue;
		std::vector<std::tuple<EXPRESSION*, Token*, STATEMENT*>> Cases;	// Token* is optional and represents the name of the capture
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