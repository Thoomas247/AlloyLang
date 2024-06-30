#pragma once
#include <deque>

#include "module/Module.hpp"

namespace AlloyCompiler
{
	enum class SearchResultCode : uint8_t
	{
		Found,
		NotFound,
		Inaccessible
	};

	template <typename T>
	struct SearchResult
	{
		SearchResultCode Code;
		T* pDefiniton;

		SearchResult(SearchResultCode code)
			: Code(code), pDefiniton(nullptr)
		{}

		SearchResult(SearchResultCode code, T* pDefinition)
			: Code(SearchResultCode::NotFound), pDefiniton(pDefinition)
		{}
	};

	class ModuleTable
	{
	public:
		ModuleTable(std::unordered_map<std::string, Module>& modules, const std::string& mainModuleName);

		void PushContext(const std::string& moduleName);
		void PopContext();

		SearchResult<MACRO_DEFINITION> GetMacroDefinition(const std::string_view& name) const;
		SearchResult<TYPE_DEFINITION> GetTypeDefinition(const std::string_view& name) const;
		SearchResult<FUNCTION_DEFINITION> GetFunctionDefinition(const std::string_view& name) const;
		SearchResult<EXTERN_DEFINITION> GetExternDefinition(const std::string_view& name) const;

		FUNCTION_DEFINITION* GetMainFunction();

		const std::unordered_map<std::string, Module>& GetModules();

	private:
		template <typename T>
		using GetDefinitionFn = Definition<T>(*)(Module&, const std::string_view&);

		struct ModuleAndSymbolName
		{
			std::string_view ModuleName;
			std::string_view SymbolName;
		};

	private:
		ModuleAndSymbolName splitName(const std::string_view& name) const;
		std::string getRelativePath(const std::string_view& rootName, const std::string_view& moduleName) const;

		template <typename T>
		Definition<T> getDefinitionInModule(GetDefinitionFn<T> getDefinitionFn, const std::string_view& moduleName, const std::string_view& symbolName) const;

		template <typename T>
		SearchResult<T> getDefinition(GetDefinitionFn<T> getDefinitionFn, const std::string_view& name) const;

	private:
		std::unordered_map<std::string, Module>& m_Modules;
		std::deque<std::string> m_ContextStack;
	};

	template<typename T>
	inline Definition<T> ModuleTable::getDefinitionInModule(GetDefinitionFn<T> getDefinitionFn, const std::string_view& moduleName, const std::string_view& symbolName) const
	{
		ASSERT(m_Modules.contains(std::string(moduleName)), "Module does not exist!");

		Module& givenModule = m_Modules.at(std::string(moduleName));

		// look in given module
		Definition<T> definition = getDefinitionFn(givenModule, symbolName);

		if (definition.IsNull())
		{
			// look in all the modules the given module imports
			for (const std::string_view& importName : givenModule.GetNodeBuffer().GetImportedModules())
			{
				const std::string relativeModuleName = getRelativePath(moduleName, importName);

				if (m_Modules.contains(relativeModuleName))
				{
					Module& relativeModule = m_Modules.at(relativeModuleName);

					definition = getDefinitionFn(relativeModule, symbolName);
				}
				else if (m_Modules.contains(std::string(importName)))
				{
					Module& absoluteModule = m_Modules.at(std::string(importName));

					definition = getDefinitionFn(absoluteModule, symbolName);
				}
				else
				{
					ASSERT(false, "Import does not exist! This should be unreachable.");
				}

				if (!definition.IsNull())
				{
					break;
				}
			}
		}

		return definition;
	}

	template<typename T>
	inline SearchResult<T> ModuleTable::getDefinition(GetDefinitionFn<T> getDefinitionFn, const std::string_view& name) const
	{
		const Module& currentModule = m_Modules.at(std::string(m_ContextStack.back()));
		auto moduleAndSymbolName = splitName(name);

		SearchResult<T> result(SearchResultCode::NotFound);

		if (moduleAndSymbolName.ModuleName.empty())
		{
			// look in current module
			Definition<T> definition = getDefinitionInModule(getDefinitionFn, m_ContextStack.back(), moduleAndSymbolName.SymbolName);

			if (definition.pDefinition != nullptr)
			{
				result.Code = SearchResultCode::Found;
				result.pDefiniton = definition.pDefinition;
			}
		}
		else
		{
			// look in the module which the symbol name refers to
			Definition<T> definition = getDefinitionInModule(getDefinitionFn, moduleAndSymbolName.ModuleName, moduleAndSymbolName.SymbolName);

			if (definition.Access == Visibility::Private)
			{
				result.Code = SearchResultCode::Inaccessible;
			}
			else if (definition.pDefinition != nullptr)
			{
				result.Code = SearchResultCode::Found;
				result.pDefiniton = definition.pDefinition;
			}
		}

		return result;
	}
}