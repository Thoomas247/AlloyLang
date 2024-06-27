#include "Program.hpp"

namespace AlloyCompiler
{

	Program::Program(const std::string& mainModuleName)
		: m_CurrentContextName(mainModuleName)
	{}

	MACRO_DEFINITION* Program::GetMacroDefinition(Token* pNameToken)
	{
		GetNodeMapFn<MACRO_DEFINITION> fn = [](Module& module) -> NamedNodes::NodeMap<MACRO_DEFINITION>&
			{
				return module.Nodes.MacroDefinitions;
			};

		return getDefinition<MACRO_DEFINITION>(fn, pNameToken);
	}

	TYPE_DEFINITION* Program::GetTypeDefinition(Token* pNameToken)
	{
		GetNodeMapFn<TYPE_DEFINITION> fn = [](Module& module) -> NamedNodes::NodeMap<TYPE_DEFINITION>&
			{
				return module.Nodes.TypeDefinitions;
			};

		return getDefinition<TYPE_DEFINITION>(fn, pNameToken);
	}

	FUNCTION_DEFINITION* Program::GetFunctionDefinition(Token* pNameToken)
	{
		GetNodeMapFn<FUNCTION_DEFINITION> fn = [](Module& module) -> NamedNodes::NodeMap<FUNCTION_DEFINITION>&
			{
				return module.Nodes.FunctionDefinitions;
			};

		return getDefinition<FUNCTION_DEFINITION>(fn, pNameToken);
	}

	FUNCTION_DEFINITION* Program::GetMemberFunctionDefinition(Token* pTypeNameToken, Token* pFunctionNameToken)
	{
		// TODO
	}

	EXTERN_DEFINITION* Program::GetExternDefinition(Token* pNameToken)
	{
		GetNodeMapFn<EXTERN_DEFINITION> fn = [](Module& module) -> NamedNodes::NodeMap<EXTERN_DEFINITION>&
			{
				return module.Nodes.ExternDefinitions;
			};

		return getDefinition<EXTERN_DEFINITION>(fn, pNameToken);
	}

	NameContextPair Program::getNameAndContext(const std::string_view& fullName)
	{
		NameContextPair pair;

		size_t separatorIndex = fullName.find_last_of(':');

		pair.Context = fullName.substr(0, separatorIndex - 1);	// -1 to exclude the ':' preceding the one we found
		pair.Name = fullName.substr(separatorIndex + 1);		// +1 to exclude the ':' we found

		return pair;
	}

	std::string Program::getRelativeModuleName(const std::string_view& name) const
	{
		return m_CurrentContextName + "::" + std::string(name);
	}

}
