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
			result.Code = SearchResultCode::BuiltIn;
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

		// if this is a member function, we need to check at what level the function is defined
		// eg: given the type graph A->B->i32->any, we must check at every level if the function exists
		size_t memberSeparatorIndex = name.find(NodeBuffer::TYPE_SEPARATOR);
		if (memberSeparatorIndex != std::string_view::npos)
		{
			std::string_view fullTypeName = name.substr(0, memberSeparatorIndex);
			std::string_view functionName = name.substr(memberSeparatorIndex + 1);

			result = getMemberFunctionDefinition(fn, fullTypeName, functionName);
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

	bool ModuleTable::AllowConversion(TYPE* from, TYPE* to) const
	{
		bool result = false;

		if (to->Type.Is<TYPE_NAME>())
		{
			// check if 'from' contains the name of 'to' anywhere down its type graph

			// get the full mangled name of 'to'
			// to handle the case where a type is imported, ie can be used as 'module::A' or just 'A'
			// we have to check the full mangled name, as otherwise we cannot see that these types are equal
			SearchResult<TYPE_DEFINITION> toTypeResult = GetTypeDefinition(to->Type.Get<TYPE_NAME>()->pNameToken->Value);
			ASSERT(toTypeResult.Code == SearchResultCode::Found || toTypeResult.Code == SearchResultCode::BuiltIn, "Invalid type!");
			const std::string& targetName = toTypeResult.MangledName;

			SearchResult<TYPE_DEFINITION> fromTypeResult = GetTypeDefinition(from->Type.Get<TYPE_NAME>()->pNameToken->Value);
			ASSERT(fromTypeResult.Code == SearchResultCode::Found || fromTypeResult.Code == SearchResultCode::BuiltIn, "Invalid type!");

			do
			{
				const std::string& currentName = toTypeResult.MangledName;

				if (currentName == targetName)
				{
					result = true;
				}

				else
				{
					fromTypeResult = getNextType(fromTypeResult.pDefiniton);
				}
			} while (result == false && fromTypeResult.Code == SearchResultCode::Found);
		}

		else
		{
			TYPE* fromBaseType = getBaseType(from);

			if (fromBaseType->Type.Is<STRUCT_TYPE>() && to->Type.Is<STRUCT_TYPE>())
			{
				STRUCT_TYPE* fromStruct = fromBaseType->Type.Get<STRUCT_TYPE>();
				STRUCT_TYPE* toStruct = to->Type.Get<STRUCT_TYPE>();

				if (fromStruct->Members.size() == toStruct->Members.size())
				{
					bool compatible = true;

					for (size_t i = 0; i < fromStruct->Members.size(); i++)
					{
						auto& [fromNameToken, fromType] = fromStruct->Members[i];
						auto& [toNameToken, toType] = toStruct->Members[i];

						if (!AllowConversion(fromType, toType))
						{
							compatible = false;
							break;
						}
					}

					if (compatible)
					{
						result = true;
					}
				}
			}

			else if (fromBaseType->Type.Is<ENUM_TYPE>() && to->Type.Is<ENUM_TYPE>())
			{
				ENUM_TYPE* fromEnum = fromBaseType->Type.Get<ENUM_TYPE>();
				ENUM_TYPE* toEnum = to->Type.Get<ENUM_TYPE>();

				if (fromEnum->Members.size() == toEnum->Members.size())
				{
					bool compatible = true;

					for (size_t i = 0; i < fromEnum->Members.size(); i++)
					{
						auto& [fromNameToken, fromType] = fromEnum->Members[i];
						auto& [toNameToken, toType] = toEnum->Members[i];

						if (fromType != nullptr && toType != nullptr)
						{
							// check if payload types are compatible
							if (!AllowConversion(fromType, toType))
							{
								compatible = false;
								break;
							}
						}

						else if (fromType == nullptr && toType == nullptr)
						{
							// do nothing, both members do not have payloads and are therefore compatible
						}

						else
						{
							// one has a payload but the other doesn't, incompatible
							compatible = false;
							break;
						}

					}

					if (compatible)
					{
						result = true;
					}
				}
			}

			else if (fromBaseType->Type.Is<ARRAY_TYPE>() && to->Type.Is<ARRAY_TYPE>())
			{
				ARRAY_TYPE* fromArray = fromBaseType->Type.Get<ARRAY_TYPE>();
				ARRAY_TYPE* toArray = to->Type.Get<ARRAY_TYPE>();

				bool compatibleSizes = false;
				if (fromArray->pSizeLiteral->Type == LiteralType::Integer && toArray->pSizeLiteral->Type == LiteralType::Integer)
				{
					const std::string_view& fromLiteral = fromArray->pSizeLiteral->pValueToken->Value;
					const std::string_view& toLiteral = toArray->pSizeLiteral->pValueToken->Value;

					uint64_t fromSize = 0;
					std::from_chars(fromLiteral.data(), fromLiteral.data() + fromLiteral.size(), fromSize);

					uint64_t toSize = 0;
					std::from_chars(toLiteral.data(), toLiteral.data() + toLiteral.size(), toSize);

					if (fromSize == toSize)
					{
						compatibleSizes = true;
					}
				}

				if (compatibleSizes && AllowConversion(fromArray->pElementType, toArray->pElementType))
				{
					result = true;
				}
			}
		}

		return result;
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

	TYPE* ModuleTable::getBaseType(TYPE* type) const
	{
		TYPE* baseType = type;

		if (baseType->Type.Is<TYPE_NAME>())
		{
			SearchResult<TYPE_DEFINITION> currentSearchResult = GetTypeDefinition(baseType->Type.Get<TYPE_NAME>()->pNameToken->Value);
			ASSERT(currentSearchResult.Code == SearchResultCode::Found || currentSearchResult.Code == SearchResultCode::BuiltIn, "Invalid type!");

			while (currentSearchResult.Code == SearchResultCode::Found && currentSearchResult.pDefiniton->pType->Type.Is<TYPE_NAME>())
			{
				currentSearchResult = getNextType(currentSearchResult.pDefiniton);
				ASSERT(currentSearchResult.Code == SearchResultCode::Found || currentSearchResult.Code == SearchResultCode::BuiltIn, "Invalid type!");
			}

			baseType = currentSearchResult.pDefiniton->pType;
		}

		return baseType;
	}

	SearchResult<FUNCTION_DEFINITION> ModuleTable::getMemberFunctionDefinition(GetDefinitionFn<FUNCTION_DEFINITION> getDefinitionFn, const std::string_view& typeName, const std::string_view& fnName) const
	{
		std::string currentType = std::string(typeName);
		std::string currentFn = std::string(fnName);

		std::string fullName = currentType + NodeBuffer::TYPE_SEPARATOR + currentFn;

		SearchResult<TYPE_DEFINITION> typeResult = GetTypeDefinition(currentType);
		SearchResult<FUNCTION_DEFINITION> funcResult = getDefinition(getDefinitionFn, fullName);

		while (funcResult.Code == SearchResultCode::NotFound)
		{
			typeResult = getNextType(typeResult.pDefiniton);
			if (typeResult.Code == SearchResultCode::NotFound)
			{
				break;
			}

			fullName = typeResult.MangledName + NodeBuffer::TYPE_SEPARATOR + currentFn;
			funcResult = getDefinition(getDefinitionFn, fullName);
		}

		return funcResult;
	}

	SearchResult<TYPE_DEFINITION> ModuleTable::getNextType(TYPE_DEFINITION* currentType) const
	{
		SearchResult<TYPE_DEFINITION> result;

		if (currentType != nullptr && currentType->pType->Type.Is<TYPE_NAME>())
		{
			result = GetTypeDefinition(currentType->pType->Type.Get<TYPE_NAME>()->pNameToken->Value);
		}

		return result;
	}
}