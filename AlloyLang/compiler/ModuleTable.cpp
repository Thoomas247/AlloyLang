#include "ModuleTable.hpp"

namespace AlloyCompiler
{
	std::string getCastFunctionDefinitionString(const std::string& type, const std::string& functionName)
	{
		return std::format("exp fn {0}:{1}<type T>(const num: &Self) -> T {2}\n", type, functionName, "{}");
	}

	ModuleTable::ModuleTable(std::unordered_map<std::string, Module>& modules, const std::string& mainModuleName)
		: m_GlobalModule(""), m_Modules(modules), m_ContextStack({ mainModuleName })
	{
		std::string builtInModule;

		for (auto& typeName : NUMERIC_TYPE_NAMES)
		{
			builtInModule.append(getCastFunctionDefinitionString(typeName, "cast"));
			builtInModule.append(getCastFunctionDefinitionString(typeName, "bit_cast"));
		}

		m_GlobalModule.GenerateFromString(builtInModule);
	}

	const std::string& ModuleTable::GetCurrentContext() const
	{
		return m_ContextStack.back();
	}

	const Module& ModuleTable::GetCurrentModule() const
	{
		return m_Modules.at(m_ContextStack.back());
	}

	void ModuleTable::PushContext(const std::string& moduleName)
	{
		m_ContextStack.push_back(moduleName);
	}

	void ModuleTable::PopContext()
	{
		ASSERT(m_ContextStack.size() > 1, "Cannot pop main module!");

		m_ContextStack.pop_back();
	}

	SearchResult<MACRO_DEFINITION> ModuleTable::GetMacroDefinition(const std::string_view& name) const
	{
		GetDefinitionFn<MACRO_DEFINITION> fn = [](const Module& module, const std::string_view& name) -> Definition<MACRO_DEFINITION>
			{
				return module.GetMacroDefinition(name);
			};

		return getDefinition(fn, name);
	}

	SearchResult<TYPE_DEFINITION> ModuleTable::GetTypeDefinition(const std::string_view& name) const
	{

		if (std::find(BUILT_IN_TYPE_NAMES.begin(), BUILT_IN_TYPE_NAMES.end(), name) != BUILT_IN_TYPE_NAMES.end())
		{
			SearchResult<TYPE_DEFINITION> result;
			result.Code = SearchResultCode::Found;
			result.MangledName = name;
			result.ModuleName = "";
			result.pDefiniton = nullptr;

			return result;
		}

		GetDefinitionFn<TYPE_DEFINITION> fn = [](const Module& module, const std::string_view& name) -> Definition<TYPE_DEFINITION>
			{
				return module.GetTypeDefinition(name);
			};

		return getDefinition(fn, name);
	}

	SearchResult<FUNCTION_DEFINITION> ModuleTable::GetFunctionDefinition(const std::string_view& name) const
	{
		GetDefinitionFn<FUNCTION_DEFINITION> fn = [](const Module& module, const std::string_view& name) -> Definition<FUNCTION_DEFINITION>
			{
				return module.GetFunctionDefinition(name);
			};

		SearchResult<FUNCTION_DEFINITION> result;

#if 0
		// if this is a member function, we need to check at what level the function is defined
		// eg: given the type graph A->B->i32->any, we must check at every level if the function exists
		size_t memberSeparatorIndex = name.find('@');
		if (memberSeparatorIndex != std::string_view::npos)
		{
			std::string_view fullTypeName = name.substr(0, memberSeparatorIndex);
			std::string_view functionName = name.substr(memberSeparatorIndex + 1);

			for (auto typeName : getTypeGraph(fullTypeName))
			{
				std::string fullName = std::string(typeName) + "@" + std::string(functionName);
				result = getDefinition(fn, fullName);

				if (result.Code != SearchResultCode::NotFound)
				{
					break;
				}
			}
		}
		else
		{
			result = getDefinition(fn, name);

			// do not include module name if extern
			if (result.pDefiniton != nullptr && result.pDefiniton->pBody == nullptr)
			{
				result.MangledName = result.pDefiniton->pFunctionNameToken->Value;
				result.ModuleName = "";
			}
		}

#else

		result = getDefinition(fn, name);

		// do not include module name if extern
		if (result.pDefiniton != nullptr && result.pDefiniton->pBody == nullptr)
		{
			result.MangledName = result.pDefiniton->pFunctionNameToken->Value;
			result.ModuleName = "";
		}

#endif

		return result;
	}

	SearchResult<VARIABLE_DEFINITION> ModuleTable::GetGlobalVariableDefinition(const std::string_view& name) const
	{
		GetDefinitionFn<VARIABLE_DEFINITION> fn = [](const Module& module, const std::string_view& name) -> Definition<VARIABLE_DEFINITION>
			{
				return module.GetGlobalVariableDefinition(name);
			};

		return getDefinition(fn, name);
	}

	FUNCTION_DEFINITION* ModuleTable::GetMainFunction()
	{
		Module& mainModule = m_Modules.at(m_ContextStack[0]);

		return mainModule.GetFunctionDefinition("main").pDefinition;
	}

	const std::unordered_map<std::string, Module>& ModuleTable::GetModules()
	{
		return m_Modules;
	}

	bool ModuleTable::ResolveTypeGraph()
	{
		bool success = true;

		m_TypeGraph.clear();

		// add all the built-in types
		for (auto& builtInTypeName : BUILT_IN_TYPE_NAMES)
		{
			m_TypeGraph.emplace(builtInTypeName, "any");
		}

		// iterate through every type in every module
		for (auto& [moduleName, module] : m_Modules)
		{
			PushContext(moduleName);

			for (auto& [typeName, typeDefinition] : module.GetNodeBuffer().GetTypeDefinitions())
			{
				ASSERT(!typeDefinition.IsNull(), "An error occurred!");

				std::string fullTypeName = moduleName + "::" + typeName;
				std::string_view& nextTypeName = m_TypeGraph.emplace(fullTypeName, "any").first->second;	// by default, all types point to the "any" type

				auto& rightType = typeDefinition.pDefinition->pType->Type;

				if (rightType.Is<TYPE_NAME>())
				{
					SearchResult<TYPE_DEFINITION> result = GetTypeDefinition(rightType.Get<TYPE_NAME>()->pNameToken->Value);

					if (result.Code == SearchResultCode::NotFound)
					{
						// TODO: error not found
						success = false;
					}

					else if (result.Code == SearchResultCode::Inaccessible)
					{
						// TODO: error inaccessible
						success = false;
					}

					else
					{
						nextTypeName = result.MangledName;
					}
				}
			}

			PopContext();
		}

		return success;
	}

	ModuleTable::ModuleAndSymbolName ModuleTable::splitName(const std::string_view& name) const
	{
		ModuleAndSymbolName result;

		// find the last double colon
		bool found = false;
		for (size_t i = name.length() - 1; i > 0; i--)
		{
			if (name[i - 1] == ':' && name[i] == ':')
			{
				result.ModuleName = name.substr(0, i - 1);
				result.SymbolName = name.substr(i + 1);

				found = true;
				break;
			}
		}

		if (!found)
		{
			result.ModuleName = "";
			result.SymbolName = name;
		}

		return result;
	}

	std::string ModuleTable::getRelativePath(const std::string_view& rootName, const std::string_view& moduleName) const
	{
		return std::string(rootName) + "::" + std::string(moduleName);
	}

	std::vector<std::string_view> ModuleTable::getTypeGraph(const std::string_view& fullTypeName) const
	{
		std::vector<std::string_view> result;

		result.push_back(fullTypeName);

		std::string_view currentName = fullTypeName;

		bool reachedEnd = false;
		while (!reachedEnd)
		{
			auto it = m_TypeGraph.find(currentName);

			if (it != m_TypeGraph.end())
			{
				result.push_back(it->second);
				currentName = it->second;
			}

			else
			{
				reachedEnd = true;
			}
		}

		return result;
	}
}