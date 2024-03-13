#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <deque>

namespace llvm
{
	class LLVMContext;
	class ConstantFolder;
	class IRBuilderDefaultInserter;
	template <typename, typename>
		class IRBuilder;
	class AllocaInst;
	class Type;
}

namespace AlloyCompiler
{

	// for pointer types, llvm does not store the type pointed to by the pointer
	// we have to kee track of it ourselves
	// type is null if this is not a pointer
	struct ValueTypePair
	{
		llvm::AllocaInst* value = nullptr;
		llvm::Type* type = nullptr;
	};

	class NamedValues
	{
	public:
		NamedValues();
		~NamedValues();

		void RegisterDefaultTypes(llvm::LLVMContext& llvmContext);

		void PushScope(const std::string_view& name);
		void PopScope();

		ValueTypePair* GetValue(const std::string_view& name);
		void InsertValue(const std::string_view& name, llvm::AllocaInst* value, llvm::Type* type);

		llvm::Type* GetType(const std::string_view& name);
		/// <summary>
		/// Returns -1 if the struct does not exist, -2 if the member does not exist.
		/// </summary>
		int GetMemberIndex(const std::string_view& structName, const std::string_view& memberName);
		std::string_view GetTypeName(llvm::Type* type);
		void InsertType(const std::string_view& name, llvm::Type* type, bool isStruct, std::unordered_map<std::string_view, int> structMembers = {});

		// should be called before PopScope in order to free memory allocated on the heap
		void FreeHeapPointers(llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>& builder);

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
			std::unordered_map<std::string_view, ValueTypePair> Values;
			std::unordered_map<std::string_view, TypeInfo> Types;
			std::unordered_map<llvm::Type*, std::string_view> TypeNames; // used for error messages

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
