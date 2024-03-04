#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <deque>

namespace llvm
{
	class AllocaInst;
	class Type;
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

		llvm::AllocaInst* GetValue(const std::string_view& name);
		void InsertValue(const std::string_view& name, llvm::AllocaInst* value);

		llvm::Type* GetType(const std::string_view& name);
		void InsertType(const std::string_view& name, llvm::Type* type, std::unordered_map<std::string_view, size_t> structMembers = {});

	private:
		struct TypeInfo
		{
			llvm::Type* Type;
			std::string_view Name;
			bool IsStruct;
			std::unordered_map<std::string_view, size_t> StructMembers;
		};

		struct Scope
		{
			std::string_view Name;
			std::unordered_map<std::string_view, llvm::AllocaInst*> Values;
			std::unordered_map<std::string_view, TypeInfo> Types;

			Scope(const std::string_view& name)
				: Name(name)
			{}
		};

	private:
		std::deque<Scope> m_ScopeStack;
	};

}
