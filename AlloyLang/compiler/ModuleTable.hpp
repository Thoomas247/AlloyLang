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
		std::string MangledName;
		std::string ModuleName;
	};

	class ModuleTable
	{
	public:
		ModuleTable(std::unordered_map<std::string, Module>& modules, const std::string& mainModuleName);

		const std::string& GetCurrentContext() const;

		void PushContext(const std::string& moduleName);
		void PopContext();

		SearchResult<MACRO_DEFINITION> GetMacroDefinition(const std::string_view& name) const;
		SearchResult<TYPE_DEFINITION> GetTypeDefinition(const std::string_view& name) const;
		SearchResult<FUNCTION_DEFINITION> GetFunctionDefinition(const std::string_view& name) const;
		SearchResult<VARIABLE_DEFINITION> GetGlobalVariableDefinition(const std::string_view& name) const;

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

		template <typename T>
		struct ModuleDefinitionPair
		{
			std::string ModuleName;
			Definition<T> Definition;
		};

	private:
		ModuleAndSymbolName splitName(const std::string_view& name) const;
		std::string getRelativePath(const std::string_view& rootName, const std::string_view& moduleName) const;

		template <typename T>
		ModuleDefinitionPair<T> getDefinitionInModule(GetDefinitionFn<T> getDefinitionFn, const std::string_view& moduleName, const std::string_view& symbolName) const;

		template <typename T>
		SearchResult<T> getDefinition(GetDefinitionFn<T> getDefinitionFn, const std::string_view& name) const;

	private:
		std::unordered_map<std::string, Module>& m_Modules;
		std::deque<std::string> m_ContextStack;
	};

	template<typename T>
	ModuleTable::ModuleDefinitionPair<T> ModuleTable::getDefinitionInModule(GetDefinitionFn<T> getDefinitionFn, const std::string_view& moduleName, const std::string_view& symbolName) const
	{
		std::string realModuleName = std::string(moduleName);

		if (realModuleName.empty())
		{
			realModuleName = m_ContextStack.back();
		}

		ModuleDefinitionPair<T> result;

		if (m_Modules.contains(realModuleName))
		{
			Module& givenModule = m_Modules.at(realModuleName);

			result.Definition = getDefinitionFn(givenModule, symbolName);
			result.ModuleName = moduleName;
		}

		return result;
	}

	template<typename T>
	SearchResult<T> ModuleTable::getDefinition(GetDefinitionFn<T> getDefinitionFn, const std::string_view& name) const
	{
		Module& currentModule = m_Modules.at(std::string(m_ContextStack.back()));

		std::string moduleName;
		std::string symbolName;
		{
			auto moduleAndSymbolName = splitName(name);
			moduleName = moduleAndSymbolName.ModuleName;
			symbolName = moduleAndSymbolName.SymbolName;
		}

		// check whatever the given module is for the symbol
		ModuleDefinitionPair<T> moduleAndDefinition = getDefinitionInModule(getDefinitionFn, moduleName, symbolName);

		// if not found in given module, check if any alias matches the given module name, and check in those aliased modules
		const auto& importedModules = currentModule.GetNodeBuffer().GetImportedModules();
		if (moduleAndDefinition.Definition.IsNull() && importedModules.contains(moduleName))
		{
			// get every real module name which was renamed to the given module name
			for (const std::string_view& importName : importedModules.at(moduleName))
			{
				const std::string relativeModuleName = getRelativePath(moduleName, importName);
				const std::string absoluteModuleName = std::string(importName);

				// check at path relative to current context
				if (m_Modules.contains(relativeModuleName))
				{
					moduleAndDefinition = getDefinitionInModule(getDefinitionFn, relativeModuleName, symbolName);
				}

				// check at path relative to project root
				else if (m_Modules.contains(absoluteModuleName))
				{
					moduleAndDefinition = getDefinitionInModule(getDefinitionFn, absoluteModuleName, symbolName);
				}

				else
				{
					ASSERT(false, "Import does not exist! This should be unreachable.");
				}

				// stop looking if found
				if (!moduleAndDefinition.Definition.IsNull())
				{
					break;
				}
			}
		}

		SearchResult<T> result{};

		if (moduleAndDefinition.Definition.IsNull())
		{
			result.Code = SearchResultCode::NotFound;
			result.MangledName = "";
			result.ModuleName = "";
			result.pDefiniton = nullptr;
		}

		else if (moduleAndDefinition.Definition.Access == Visibility::Private
			&& !moduleAndDefinition.ModuleName.empty()
			&& moduleAndDefinition.ModuleName != m_ContextStack.back())
		{
			result.Code = SearchResultCode::Inaccessible;
			result.MangledName = moduleAndDefinition.ModuleName + "::" + symbolName;
			result.ModuleName = moduleAndDefinition.ModuleName;
			result.pDefiniton = moduleAndDefinition.Definition.pDefinition;
		}
		
		else
		{
			result.Code = SearchResultCode::Found;
			result.MangledName = moduleAndDefinition.ModuleName + "::" + symbolName;
			result.ModuleName = moduleAndDefinition.ModuleName;
			result.pDefiniton = moduleAndDefinition.Definition.pDefinition;
		}

		return result;
	}
}