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
	#define _EnumPayloadStruct_	"_EnumPayloadStruct_"

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

		ValueTypePair* GetValue(const std::string& name, bool searchInParents = true);
		void InsertValue(const std::string& name, llvm::AllocaInst* value, llvm::Type* type, bool isConst, bool freeOnExit);
		bool RemoveValue(const std::string& name);

		//
		// this structure contains additional member information for structure and enum types
		//
		struct TypeMemberInfo
		{
			int memberIndex;
			llvm::Type* containedType;	// for structures : contains the underlying type for pointer types which are are store by llvm
										// for enums : contains the type of the optional payload associated to the element
		};

		llvm::Type* GetType(const std::string_view& name);
		
		/// <summary>
		/// Returns -1 if the struct or enum does not exist, -2 if the member does not exist.
		/// </summary>
		NamedValues::TypeMemberInfo GetStructMemberIndex(const std::string_view& structName, const std::string_view& memberName);
		NamedValues::TypeMemberInfo GetEnumMemberIndex(const std::string_view& structName, const std::string_view& memberName);

		std::string_view GetTypeName(const llvm::Type* type);

		typedef enum { basic = 0, structure = 1, enumeration = 2 } UserDefinedType;
		void InsertType(const std::string& name, llvm::Type* type, UserDefinedType userDefinedType, 
			const std::unordered_map<std::string_view, TypeMemberInfo>& memberInfo = {},
			const std::unordered_map<std::string, std::string>& genericTypeMap = {}
			);
		bool GetStructMembers(const std::string_view& structName, std::unordered_map<std::string_view, TypeMemberInfo>& structMembers);
		bool GetEnumMembers(const std::string_view& enumName, std::unordered_map<std::string_view, TypeMemberInfo>& enumMembers);

		// support for generic function parameters
		std::string GetGenericType(const std::string& typeName);
		void SetGenericType(const std::string& typeName, const std::string& realType);

		// returns the current function's generic type map
		std::unordered_map<std::string, std::string> GetGenericTypeMap();
		// returns the generic type map for a specific type
		bool GetGenericTypeMap(const std::string_view& structName, std::unordered_map<std::string, std::string>& genericTypeMap);

		llvm::Value*& GetEnumCapturedValue() { return m_ScopeStack.back().EnumCapturedValue; }

		// should be called before PopScope in order to free memory allocated on the heap
		void FreeHeapPointers(llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>& builder);

		// check if specific type is an enumeration
		bool IsEnumType(llvm::Type* type);

		// return the unique ID for that type
		unsigned int GetID(llvm::Type* type);

		// return the structure type associated with a specific payload type
		llvm::Type* GetEnumPayloadStruct(llvm::LLVMContext& llvmContext, llvm::Type* PayloadType);

	private:
		struct TypeInfo
		{
			llvm::Type* Type;
			std::string_view Name;
			UserDefinedType userDefinedType;
			std::unordered_map<std::string_view, TypeMemberInfo> memberInfo;
			std::unordered_map<std::string, std::string> genericTypeMap;
			unsigned int ID;		// a unique internal ID that identifies each type
		};

		struct Scope
		{
			std::string_view Name;
			std::unordered_map<std::string, ValueTypePair> Values;
			std::unordered_map<std::string, TypeInfo> Types;
			std::unordered_map<const llvm::Type*, std::string> TypeNames; // used for error messages
			std::unordered_map<std::string, std::string> GenericTypeMap;	// this is a map from the generic types to the actual function parameter types
			llvm::Value* EnumCapturedValue;									// captured payload value in if or switch statements

			Scope(const std::string_view& name)
				: Name(name)
			{}
		};

	private:
		TypeInfo* findType(const std::string_view& name);

	private:
		std::deque<Scope> m_ScopeStack;

		// this is incremented each time a new type is added and allows us to give each type a unique ID
		unsigned int NextTypeID;
	};

}
