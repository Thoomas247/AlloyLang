#pragma once
#include "../parser/Parser.hpp"

namespace AlloyCompiler
{
	using MacroVariable = VariantNode<TYPE, FUNCTION_DEFINITION>;
	using LocalVariableMap = std::unordered_map<std::string_view, MacroVariable>;
	using LocalMacroMap = std::unordered_map<std::string_view, MACRO_DEFINITION*>;
	using MacroResult = std::optional<MacroVariable>;

	class MacroEngine
	{
	public:
		MacroEngine(NamedNodes& namedNodes)
			: m_NamedNodes(namedNodes)
		{}

		MacroResult RunMacro(const MACRO_CALL& macroCall);

	private:
		MacroResult getVariableInPreviousScope(const std::string_view& name);
		MACRO_DEFINITION* getMacroInPreviousScope(const std::string_view& name);

		void pushScope();
		void popScope();

		bool addVariable(const std::string_view& name, const MacroVariable& variable);
		bool addMacro(const std::string_view& name, MACRO_DEFINITION* pMacro);

		MacroResult getVariable(const std::string_view& name);

	private:
		NamedNodes& m_NamedNodes;
		std::deque<LocalVariableMap> m_LocalVariables;
		std::deque<LocalMacroMap> m_LocalMacros;
	};
}