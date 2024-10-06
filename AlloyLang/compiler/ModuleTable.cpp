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
		static const std::unordered_set<std::string_view> s_BuiltInTypes
		{
			"String",

			"bool",

			"i8", "i16", "i32", "i64",

			"u8", "u16", "u32", "u64",

			"f32", "f64",
		};

		if (s_BuiltInTypes.contains(name))
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

		auto result = getDefinition(fn, name);

		// do not include module name if extern
		if (result.pDefiniton != nullptr && result.pDefiniton->pBody == nullptr)
		{
			result.MangledName = result.pDefiniton->pFunctionNameToken->Value;
			result.ModuleName = "";
		}

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
}