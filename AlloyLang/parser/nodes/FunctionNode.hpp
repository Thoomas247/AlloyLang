#pragma once

#include "../Node.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct VARIABLE_DECLARATION;
	struct STATEMENT_BLOCK;
	struct TYPE;

	struct RETURN_TYPE
	{
		ReturnType RetType;
		TYPE* pType;
	};

	struct FUNCTION_TYPE
	{
		bool IsVarArg;
		bool IsGeneric;
		bool IsMember;
		std::vector<FUNCTION_PARAMETER*> Parameters;
		RETURN_TYPE* pReturnType;	// optional
	};

	struct FUNCTION_DEFINITION
	{
		Token* pStructNameToken;	// optional name of a struct in the case of member functions
		Token* pFunctionNameToken;
		FUNCTION_TYPE* pFunctionType;
		STATEMENT_BLOCK* pBody;
	};

	struct EXTERN_DEFINITION
	{
		Token* pNameToken;
		FUNCTION_TYPE* pFunctionType;
	};

	struct FUNCTION_CALL
	{
		Token* pStructOrVariableNameToken;	// optional name of a struct or variable in the case of member functions
		Token* pFunctionNameToken;
		std::vector<EXPRESSION*> Arguments;
	};

}