#include "MacroEngine.hpp"

namespace AlloyCompiler
{

	MacroResult MacroEngine::RunMacro(const MACRO_CALL& macroCall)
	{
		auto it = m_NamedNodes.MacroDefinitions.find(macroCall.pMacroNameToken->Value);
		if (it == m_NamedNodes.MacroDefinitions.end())
		{
			ASSERT(false, "Macro '{0}' does not exist!", macroCall.pMacroNameToken->Value);	// TODO: proper error handling
		}
		const MACRO_DEFINITION& macroDefinition = *it->second;

		// push a new variable scope
		pushScope();

		// check that the parameters match
		if (macroDefinition.Parameters.size() != macroCall.Arguments.size())
		{
			ASSERT(false, "Macro '{0}' takes in {1} arguments. {2} were provided.", 
				macroCall.pMacroNameToken->Value, macroDefinition.Parameters.size(), macroCall.Arguments.size());	// TODO: proper error handling)
		}

		// initialize the parameter variables
		for (size_t i = 0; i < macroCall.Arguments.size(); i++)
		{
			VariantNode<MACRO_VARIABLE_IDENTIFIER, MACRO_CALL> argument = macroCall.Arguments[i];

			MacroResult result;

			if (argument.Is<MACRO_VARIABLE_IDENTIFIER>())
			{
				const MACRO_VARIABLE_IDENTIFIER& macroVariableIdentifier = *argument.Get<MACRO_VARIABLE_IDENTIFIER>();
				result = getVariableInPreviousScope(macroVariableIdentifier.pNameToken->Value);
			}
			else if (argument.Is<MACRO_CALL>())
			{
				result = RunMacro(*argument.Get<MACRO_CALL>());
			}
			else
			{
				ASSERT(false, "Invalid argument node!");
			}

			if (!result.has_value())
			{
				ASSERT(false, "Macro argument must be a type name or a function name.");
			}

			if (!addVariable(macroDefinition.Parameters[i].second->Value, result.value()))
			{
				ASSERT(false, "");
			}

			MacroVariableType argumentType = MacroVariableType::None;
			if (result.value().Is<TYPE>())
			{
				argumentType = MacroVariableType::Type;
			}
			else if (result.value().Is<FUNCTION_DEFINITION>())
			{
				argumentType = MacroVariableType::Fn;
			}

			MacroVariableType expectedType = macroDefinition.Parameters[i].first;
			if (argumentType != expectedType)
			{
				const std::string argumentTypeName = (argumentType == MacroVariableType::Type) ? "type" : "function";
				const std::string expectedTypeName = (expectedType == MacroVariableType::Type) ? "type" : "function";

				ASSERT(false, "Expected parameter of type '{0}' but got '{1}'.", expectedTypeName, argumentTypeName);
			}
		}

		// process the macro
		MacroVariable returnValue;

		for (MACRO_STATEMENT* pStatement : macroDefinition.Body)
		{
			if (pStatement->Is<MACRO_DEFINITION>())
			{
				MACRO_DEFINITION* pLocalMacroDef = pStatement->Get<MACRO_DEFINITION>();

				if (!addMacro(pLocalMacroDef->pNameToken->Value, pLocalMacroDef))
				{
					ASSERT(false, "");
				}
			}
			else if (pStatement->Is<MACRO_CALL>())
			{
				MacroResult result = RunMacro(*pStatement->Get<MACRO_CALL>());

				if (!result.has_value())
				{
					ASSERT(false, "");
				}
			}
			else if (pStatement->Is<TYPE_DEFINITION>())
			{
				TYPE_DEFINITION* pTypeDefinition = pStatement->Get<TYPE_DEFINITION>();

				// if type needs to be evaluated first through another macro, run it
				if (pTypeDefinition->pType->Type.Is<MACRO_CALL>())
				{
					MacroResult result = RunMacro(*pTypeDefinition->pType->Type.Get<MACRO_CALL>());

					if (!result.has_value())
					{
						ASSERT(false, "");
					}

					// set the TYPE node to point to the resolved type, rather than MACRO_TYPE
					if (result.value().Is<TYPE>())
					{
						pTypeDefinition->pType = result.value().Get<TYPE>();
					}

					else
					{
						ASSERT(false, "Expected expression of type 'type' but got 'function'.");
					}
				}

				if (!addVariable(pTypeDefinition->pNameToken->Value, MacroVariable(pTypeDefinition->pType)))
				{
					ASSERT(false, "");
				}
			}
			else if (pStatement->Is<FUNCTION_DEFINITION>())
			{
				FUNCTION_DEFINITION* pFunctionDefinition = pStatement->Get<FUNCTION_DEFINITION>();

				// TODO: add all current local variables to a map accessible only to this function definition.
				// the types and functions previously created in this macro should be available when compiling this function.
				ASSERT(false, "Not yet implemented.");
			}
			else if (pStatement->Is<MACRO_RETURN>())
			{
				MACRO_RETURN* pMacroReturn = pStatement->Get<MACRO_RETURN>();

				if (pMacroReturn->Is<MACRO_VARIABLE_IDENTIFIER>())
				{
					MACRO_VARIABLE_IDENTIFIER* pMacroVariableIdentifier = pMacroReturn->Get<MACRO_VARIABLE_IDENTIFIER>();

					// retrieve the variable by name and return it
					MacroResult variable = getVariable(pMacroVariableIdentifier->pNameToken->Value);
					if (!variable.has_value())
					{
						ASSERT(false, "");
					}

					returnValue = variable.value();
				}
				else if (pMacroReturn->Is<MACRO_CALL>())
				{
					MACRO_CALL* pMacroCall = pMacroReturn->Get<MACRO_CALL>();

					MacroResult result = RunMacro(*pMacroCall);
					if (!result.has_value())
					{
						ASSERT(false, "");
					}

					returnValue = result.value();
				}
				else if (pMacroReturn->Is<TYPE>())
				{
					TYPE* pType = pMacroReturn->Get<TYPE>();

					returnValue.Set(pType);
				}
				else
				{
					ASSERT(false, "Invalid MACRO_RETURN node!");
				}
			}
			else
			{
				ASSERT(false, "Invalid MACRO_STATEMENT node!");
			}
		}

		// check that the return value type matches the macro's return type
		switch (macroDefinition.ReturnType)
		{
		case MacroVariableType::None:
			ASSERT(returnValue.IsEmpty(), "Expected a return value of type 'type'.");
			break;

		case MacroVariableType::Type:
			ASSERT(returnValue.Is<TYPE>(), "Expected a return value of type 'fn'.");
			break;

		case MacroVariableType::Fn:
			ASSERT(returnValue.Is<FUNCTION_DEFINITION>(), "Expected no return value.");
			break;

		default:
			break;
		}

		// pop the variable scope
		popScope();

		return MacroResult(returnValue);
	}

	MacroResult MacroEngine::getVariableInPreviousScope(const std::string_view& name)
	{
		MacroResult result;

		// look in the previous scope if there is one
		if (m_LocalVariables.size() > 1)
		{
			const size_t previousScopeIndex = m_LocalVariables.size() - 2;

			auto it = m_LocalVariables[previousScopeIndex].find(name);
			if (it != m_LocalVariables[previousScopeIndex].end())
			{
				result.emplace(it->second);
			}
		}

		// if variable wasn't found, look in global scope
		if (!result.has_value())
		{
			if (m_NamedNodes.TypeDefinitions.contains(name))
			{
				result.emplace(MacroVariable(m_NamedNodes.TypeDefinitions[name]->pType));
			}

			else if (m_NamedNodes.FunctionDefinitions.contains(name))
			{
				result.emplace(MacroVariable(m_NamedNodes.FunctionDefinitions[name]));
			}
		}

		return result;
	}

	MACRO_DEFINITION* MacroEngine::getMacroInPreviousScope(const std::string_view& name)
	{
		MACRO_DEFINITION* pMacroDefinition = nullptr;

		// look in the previous scope if there is one
		if (m_LocalMacros.size() > 1)
		{
			const size_t previousScopeIndex = m_LocalMacros.size() - 2;

			auto it = m_LocalMacros[previousScopeIndex].find(name);
			if (it != m_LocalMacros[previousScopeIndex].end())
			{
				pMacroDefinition = it->second;
			}
		}

		// if macro wasn't found, look in global scope
		if (pMacroDefinition == nullptr)
		{
			auto it = m_NamedNodes.MacroDefinitions.find(name);
			if (it != m_NamedNodes.MacroDefinitions.end())
			{
				pMacroDefinition = it->second;
			}
		}

		return pMacroDefinition;
	}

	void MacroEngine::pushScope()
	{
		m_LocalVariables.push_back(LocalVariableMap());
		m_LocalMacros.push_back(LocalMacroMap());
	}

	void MacroEngine::popScope()
	{
		m_LocalVariables.pop_back();
		m_LocalMacros.pop_back();
	}

	bool MacroEngine::addVariable(const std::string_view& name, const MacroVariable& variable)
	{
		if (m_LocalVariables.back().contains(name))
		{
			ASSERT(false, "A variable with the name '{0}' already exists in this scope.", name); // TODO: proper error handling
			return false;
		}

		m_LocalVariables.back()[name] = variable;
		return true;
	}

	bool MacroEngine::addMacro(const std::string_view& name, MACRO_DEFINITION* pMacro)
	{
		if (m_LocalMacros.back().contains(name))
		{
			ASSERT(false, "A macro with the name '{0}' already exists in this scope.", name); // TODO: proper error handling
			return false;
		}

		m_LocalMacros.back()[name] = pMacro;
		return true;
	}

	MacroResult MacroEngine::getVariable(const std::string_view& name)
	{
		MacroResult result;

		auto it = m_LocalVariables.back().find(name);
		if (it != m_LocalVariables.back().end())
		{
			result.emplace(it->second);
		}

		if (!result.has_value())
		{
			if (m_NamedNodes.TypeDefinitions.contains(name))
			{
				result.emplace(m_NamedNodes.TypeDefinitions[name]);
			}
			else if (m_NamedNodes.FunctionDefinitions.contains(name))
			{
				result.emplace(m_NamedNodes.FunctionDefinitions[name]);
			}
		}

		return result;
	}

}
