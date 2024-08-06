#include "ModuleTable.hpp"

namespace AlloyCompiler
{
	ModuleTable::ModuleTable(std::unordered_map<std::string, Module>& modules, const std::string& mainModuleName)
		: m_Modules(modules), m_ContextStack({ mainModuleName })
	{
	}

	const std::string& ModuleTable::GetCurrentContext() const
	{
		return m_ContextStack.back();
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
		GetDefinitionFn<MACRO_DEFINITION> fn = [](Module& module, const std::string_view& name) -> Definition<MACRO_DEFINITION>
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

		GetDefinitionFn<TYPE_DEFINITION> fn = [](Module& module, const std::string_view& name) -> Definition<TYPE_DEFINITION>
			{
				return module.GetTypeDefinition(name);
			};

		return getDefinition(fn, name);
	}

	SearchResult<FUNCTION_DEFINITION> ModuleTable::GetFunctionDefinition(const std::string_view& name) const
	{
		GetDefinitionFn<FUNCTION_DEFINITION> fn = [](Module& module, const std::string_view& name) -> Definition<FUNCTION_DEFINITION>
			{
				return module.GetFunctionDefinition(name);
			};

		return getDefinition(fn, name);
	}

	SearchResult<EXTERN_DEFINITION> ModuleTable::GetExternDefinition(const std::string_view& name) const
	{
		GetDefinitionFn<EXTERN_DEFINITION> fn = [](Module& module, const std::string_view& name) -> Definition<EXTERN_DEFINITION>
			{
				return module.GetExternDefinition(name);
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