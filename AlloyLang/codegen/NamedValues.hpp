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
		/// <summary>
		/// Returns -1 if the struct does not exist, -2 if the member does not exist.
		/// </summary>
		int GetMemberIndex(const std::string_view& structName, const std::string_view& memberName);
		void InsertType(const std::string_view& name, llvm::Type* type, bool isStruct, std::unordered_map<std::string_view, int> structMembers = {});

	private:
		struct TypeInfo
		{
			llvm::Type* Type;
			std::string_view Name;
			bool IsStruct;
			std::unordered_map<std::string_view, int> StructMembers;
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
		TypeInfo* findType(const std::string_view& name);

	private:
		std::deque<Scope> m_ScopeStack;
	};

}
