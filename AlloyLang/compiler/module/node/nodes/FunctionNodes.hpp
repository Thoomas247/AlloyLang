#pragma once

#include "NodeCommon.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	struct VARIABLE_DECLARATION;
	struct STATEMENT_BLOCK;
	struct TYPE;
	struct TYPE_IDENTIFIER;
	struct TYPE_NAME;
	struct VARIABLE;

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
		TYPE_IDENTIFIER* pTypeIdentifier;	// optional type identifier for member functions
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
		TYPE_NAME* pTypeOrVariableName;	// optional, can contain name of variable OR name of type, TYPE_NAME is used because it can handle both cases
		Token* pFunctionNameToken;
		std::vector<EXPRESSION*> Arguments;
	};

}