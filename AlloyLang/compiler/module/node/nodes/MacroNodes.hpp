#pragma once

#include "NodeCommon.hpp"
#include "TypeNodes.hpp"
#include "FunctionNodes.hpp"
#include "VariantNodes.hpp"

namespace AlloyCompiler
{
	enum class MacroVariableType : uint8_t
	{
		None,
		Type,
		Fn
	};

	struct MACRO_DEFINITION;
	struct MACRO_CALL;
	struct MACRO_VARIABLE_IDENTIFIER;

	using MACRO_RETURN = VariantNode<MACRO_VARIABLE_IDENTIFIER, MACRO_CALL, TYPE>;
	using MACRO_STATEMENT = VariantNode<MACRO_DEFINITION, MACRO_CALL, TYPE_DEFINITION, FUNCTION_DEFINITION, MACRO_RETURN>;

	struct MACRO_DEFINITION
	{
		Token* pNameToken;
		std::vector<std::pair<MacroVariableType, Token*>> Parameters;
		MacroVariableType ReturnType;

		std::vector<MACRO_STATEMENT*> Body;
	};

	struct MACRO_VARIABLE_IDENTIFIER
	{
		Token* pNameToken;
	};

	struct MACRO_CALL
	{
		Token* pMacroNameToken;
		std::vector<VariantNode<MACRO_VARIABLE_IDENTIFIER, MACRO_CALL>> Arguments;
	};

}