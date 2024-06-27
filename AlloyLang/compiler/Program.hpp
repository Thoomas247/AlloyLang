#pragma once
#include "../tokenizer/Tokenizer.hpp"
#include "../parser/Parser.hpp"
#include "../codegen/CodeGenerator.hpp"

namespace AlloyCompiler
{
	template <typename T>
	using GetNodeMapFn = NamedNodes::NodeMap<T>& (*) (Module&);

	struct NameContextPair
	{
		std::string_view Name;
		std::string_view Context;
	};

	class Program
	{
	public:
		Program(const std::string& mainModuleName);

		MACRO_DEFINITION* GetMacroDefinition(Token* pNameToken);
		TYPE_DEFINITION* GetTypeDefinition(Token* pNameToken);
		FUNCTION_DEFINITION* GetFunctionDefinition(Token* pNameToken);
		FUNCTION_DEFINITION* GetMemberFunctionDefinition(Token* pTypeNameToken, Token* pFunctionNameToken);
		EXTERN_DEFINITION* GetExternDefinition(Token* pNameToken);

	private:
		NameContextPair getNameAndContext(const std::string_view& fullName);
		std::string getRelativeModuleName(const std::string_view& name) const;

		template <typename T>
		Definition<T> findSymbolInModule(GetNodeMapFn<T> getNodeMap, Token* pErrorToken, const std::string_view& moduleName, const std::string_view& symbolName);

		template <typename T>
		T* getDefinition(GetNodeMapFn<T> getNodeMap, Token* pNameToken);

	private:
		std::string m_CurrentContextName;
		std::vector<std::string> m_SourceStrings;
		std::unordered_map<std::string, Module> m_Modules;
	};

	template<typename T>
	inline Definition<T> Program::findSymbolInModule(GetNodeMapFn<T> getNodeMap, Token* pErrorToken, const std::string_view& moduleName, const std::string_view& symbolName)
	{
		if (!m_Modules.contains(moduleName))
		{
			return Definition<T>(Visibility::Private, nullptr);
		}

		Module& module = m_Modules[moduleName];

		if (!getNodeMap(module).contains(symbolName))
		{
			return Definition<T>(Visibility::Private, nullptr);
		}

		return getNodeMap(module)[symbolName];
	}

	template<typename T>
	inline T* Program::getDefinition(GetNodeMapFn<T> getNodeMap, Token* pNameToken)
	{
		NameContextPair nameAndContext = getNameAndContext(pNameToken->Value);
		Module& currentContext = m_Modules.at(m_CurrentContextName);

		if (nameAndContext.Context.empty())
		{
			// check current module
			Definition<T> definition = findSymbolInModule<T>(getNodeMap, pNameToken, m_CurrentContextName, nameAndContext.Name);

			if (definition.IsNull())
			{
				// otherwise check imported modules
				for (auto& moduleName : currentContext.ImportedModules)
				{
					// check relative module
					definition = findSymbolInModule<T>(getNodeMap, pNameToken, getRelativeModuleName(moduleName), nameAndContext.Name);

					if (definition.IsNull())
					{
						// otherwise look in absolute path
						definition = findSymbolInModule<T>(getNodeMap, pNameToken, moduleName, nameAndContext.Name);
					}

					if (definition.IsNull())
					{
						continue;
					}

					if (definition.Access == Visibility::Private)
					{
						// symbol is private
						currentContext.LogErrorAtToken(pNameToken, "Symbol '{0}' is private and cannot be accessed here.", pNameToken->Value);
						return nullptr;
					}

					break;
				}
			}

			if (definition.IsNull())
			{
				// symbol not found anywhere
				currentContext.LogErrorAtToken(pNameToken, "Symbol '{0}' does not exist.", pNameToken->Value);
				return nullptr;
			}

			return definition.pDefinition;
		}

		else
		{
			// check relative module
			Definition<T> definition = findSymbolInModule<T>(getNodeMap, pNameToken, getRelativeModuleName(nameAndContext.Context), nameAndContext.Name);

			if (definition.IsNull())
			{
				// otherwise look in absolute path
				definition = findSymbolInModule<T>(getNodeMap, pNameToken, nameAndContext.Context, nameAndContext.Name);
			}

			if (definition.IsNull())
			{
				// symbol does not exist
				currentContext.LogErrorAtToken(pNameToken, "Symbol '{0}' does not exist.", pNameToken->Value);
				return nullptr;
			}

			if (definition.Access == Visibility::Private)
			{
				// symbol is private
				currentContext.LogErrorAtToken(pNameToken, "Symbol '{0}' is private and cannot be accessed here.", pNameToken->Value);
				return nullptr;
			}

			return definition.pDefinition;
		}
	}
}