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

	// for pointer and reference types, llvm does not store the type pointed to by the pointer
	// we have to keep track of it ourselves
	// type is null if this is not a pointer nor a reference
	struct ValueTypePair
	{
		llvm::AllocaInst* value = nullptr;
		llvm::Type* containedType = nullptr;
		bool isConst = false;					// whether we are pointing to a constant value
		bool freeOnExit = false;				// whether we should free the pointer
	};

	class NamedValues
	{
	public:
		NamedValues();
		virtual ~NamedValues();

		void RegisterDefaultTypes(llvm::LLVMContext& llvmContext);

		void PushScope(const std::string_view& name);
		void PopScope();

		ValueTypePair* GetValue(const std::string& name);
		void InsertValue(const std::string& name, llvm::AllocaInst* value, llvm::Type* type, bool isConst, bool freeOnExit);
		bool RemoveValue(const std::string& name);

		struct StructMemberInfo
		{
			int memberIndex;
			llvm::Type* containedType;	// for pointer types only
		};

		llvm::Type* GetType(const std::string_view& name);
		/// <summary>
		/// Returns -1 if the struct does not exist, -2 if the member does not exist.
		/// </summary>
		NamedValues::StructMemberInfo GetMemberIndex(const std::string_view& structName, const std::string_view& memberName);
		std::string_view GetTypeName(llvm::Type* type);

		void InsertType(const std::string& name, llvm::Type* type, bool isStruct, std::unordered_map<std::string_view, StructMemberInfo> structMembers = {});
		bool GetStructMembers(const std::string_view& structName, std::unordered_map<std::string_view, StructMemberInfo>& structMembers);

		// should be called before PopScope in order to free memory allocated on the heap
		void FreeHeapPointers(llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>& builder);

	private:
		struct TypeInfo
		{
			llvm::Type* Type;
			std::string_view Name;
			bool IsStruct;
			std::unordered_map < std::string_view, StructMemberInfo > StructMembers;
		};

		struct Scope
		{
			std::string_view Name;
			std::unordered_map<std::string, ValueTypePair> Values;
			std::unordered_map<std::string, TypeInfo> Types;
			std::unordered_map<llvm::Type*, std::string> TypeNames; // used for error messages

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
