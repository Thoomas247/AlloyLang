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

	llvm::AllocaInst* NamedValues::Get(const std::string_view& name)
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

	void NamedValues::Insert(const std::string_view& name, llvm::AllocaInst* value)
	{
		ASSERT(!m_ScopeStack.back().Values.contains(name), "Named value already exists! Should check if it exists first with NamedValues::Get(const std::string& name).");

		m_ScopeStack.back().Values[name] = value;
	}

}