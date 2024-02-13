#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <deque>

namespace llvm
{
	class AllocaInst;
}

namespace AlloyCompiler
{
	class NamedValues
	{
	public:
		NamedValues();
		~NamedValues();

		void PushScope(const std::string_view& name);
		void PopScope();

		llvm::AllocaInst* Get(const std::string_view& name);
		void Insert(const std::string_view& name, llvm::AllocaInst* value);

	private:
		struct Scope
		{
			std::string_view Name;
			std::unordered_map<std::string_view, llvm::AllocaInst*> Values;

			Scope(const std::string_view& name)
				: Name(name)
			{}
		};

	private:
		std::deque<Scope> m_ScopeStack;
	};

}
