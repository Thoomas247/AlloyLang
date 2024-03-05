#include "NamedValues.hpp"

#include "../log/Log.hpp"

namespace AlloyCompiler
{

	/* -- PUBLIC -- */

	NamedValues::NamedValues()
	{
		m_ScopeStack.emplace_back("");
	}

	NamedValues::~NamedValues()
	{
	}

	void NamedValues::PushScope(const std::string_view& name)
	{
		m_ScopeStack.emplace_back(name);
	}

	void NamedValues::PopScope()
	{
		ASSERT(m_ScopeStack.size() > 1, "Cannot pop the root scope!");

		m_ScopeStack.pop_back();
	}

	llvm::AllocaInst* NamedValues::GetValue(const std::string_view& name)
	{
		// look for the value starting from the current scope and going up
		for (auto it = m_ScopeStack.rbegin(); it != m_ScopeStack.rend(); ++it)
		{
			auto& scope = *it;
			auto found = scope.Values.find(name);
			if (found != scope.Values.end())
			{
				return found->second;
			}
		}

		return nullptr;
	}

	void NamedValues::InsertValue(const std::string_view& name, llvm::AllocaInst* value)
	{
		ASSERT(!m_ScopeStack.back().Values.contains(name), "Named value already exists! Should check if it exists first with NamedValues::GetValue(const std::string& name).");

		m_ScopeStack.back().Values[name] = value;
	}

	llvm::Type* NamedValues::GetType(const std::string_view& name)
	{
		TypeInfo* typeInfo = findType(name);

		if (typeInfo)
		{
			return typeInfo->Type;
		}

		return nullptr;
	}

	int NamedValues::GetMemberIndex(const std::string_view& structName, const std::string_view& memberName)
	{
		TypeInfo* typeInfo = findType(structName);

		if (!typeInfo || !typeInfo->IsStruct)
		{
			return -1;
		}

		auto it = typeInfo->StructMembers.find(memberName);

		if (it == typeInfo->StructMembers.end())
		{
			return -2;
		}

		return it->second;
	}

	void NamedValues::InsertType(const std::string_view& name, llvm::Type* type, bool isStruct, std::unordered_map<std::string_view, int> structMembers)
	{
		ASSERT(!m_ScopeStack.back().Types.contains(name), "Named type already exists! Should check if it exists first with NamedValues::GetType(const std::string& name).");
		ASSERT(structMembers.empty() || isStruct, "Struct members can only be added to a struct type.");

		TypeInfo typeInfo;
		typeInfo.Type = type;
		typeInfo.Name = name;
		typeInfo.IsStruct = isStruct;
		typeInfo.StructMembers = structMembers;

		m_ScopeStack.back().Types[name] = typeInfo;
	}



	/* -- PRIVATE -- */

	NamedValues::TypeInfo* NamedValues::findType(const std::string_view& name)
	{
		// look for the type starting from the current scope and going up
		for (auto it = m_ScopeStack.rbegin(); it != m_ScopeStack.rend(); ++it)
		{
			auto& scope = *it;
			auto found = scope.Types.find(name);
			if (found != scope.Types.end())
			{
				return &found->second;
			}
		}

		return nullptr;
	}

}