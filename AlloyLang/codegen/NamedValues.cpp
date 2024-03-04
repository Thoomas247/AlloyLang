#include "NamedValues.hpp"

#include "../log/Log.hpp"

namespace AlloyCompiler
{

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
		// look for the type starting from the current scope and going up
		for (auto it = m_ScopeStack.rbegin(); it != m_ScopeStack.rend(); ++it)
		{
			auto& scope = *it;
			auto found = scope.Types.find(name);
			if (found != scope.Types.end())
			{
				return found->second;
			}
		}

		return nullptr;
	}

	void NamedValues::InsertType(const std::string_view& name, llvm::Type* type, std::unordered_map<std::string_view, size_t> structMembers)
	{
		ASSERT(!m_ScopeStack.back().Types.contains(name), "Named type already exists! Should check if it exists first with NamedValues::GetType(const std::string& name).");

		TypeInfo typeInfo;
		typeInfo.Type = type;
		typeInfo.Name = name;
		typeInfo.IsStruct = structMembers.size() > 0;
		typeInfo.StructMembers = structMembers;


		m_ScopeStack.back().Types[name] = typeInfo;
	}

}