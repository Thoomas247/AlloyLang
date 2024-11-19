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
	class AlloyType;
	class AlloyValue;

	// GenericTypeMap is a map from a generic type T to the actual type name (e.g. i32) and the generated AlloyType
	// Note that llvm can generate a type that is different from the one requested, e.g. generate u32 when we request i32
	typedef std::unordered_map<std::string, AlloyType*> GenericTypeMap;
	typedef std::vector<AlloyType*> GenericArgumentTypes;

#define _EnumPayloadStruct_	"_EnumPayloadStruct_"
#define EnumPayloadIndex	0
#define EnumPayloadValue	1
#define EnumPayloadEnumID	2

	class NamedValues
	{
	public:
		NamedValues();
		virtual ~NamedValues();

		void RegisterDefaultTypes(llvm::LLVMContext& llvmContext);

		void PushScope(const std::string_view& name);
		void PopScope();

		const AlloyValue* GetValue(const std::string& name, bool searchInParents = true);
		void InsertValue(const std::string& name, llvm::AllocaInst* ptr, AlloyType* type, AlloyType* parentType, bool isConst, bool freeOnExit);
		void InsertValue(const std::string& name, const AlloyValue& value);
		bool UpdateValue(const std::string& name, const AlloyValue& value);
		bool RemoveValue(const std::string& name);

		//
		// this structure contains additional member information for structure and enum types
		//
		struct TypeMemberInfo
		{
			int memberIndex;
			AlloyType* Type;	// contains the structure element type
								// for enums : contains the type of the optional payload associated to the element
		};

		AlloyType* GetType(const std::string_view& name);
		
		/// <summary>
		/// Returns -1 if the struct or enum does not exist, -2 if the member does not exist.
		/// </summary>
		TypeMemberInfo GetStructMemberInfo(const std::string_view& structName, const std::string_view& memberName);
		TypeMemberInfo GetEnumMemberInfo(const std::string_view& structName, const std::string_view& memberName);

		// Note that llvm can generate a type that is different from the one requested, e.g. generate u32 when we request i32
		// this is why we cannot rely on NamedValues::GetTypeName to retrieve the real type name
		std::string_view GetTypeName(const AlloyType* type);

		typedef enum { basic = 0, structure = 1, enumeration = 2, array = 3 } UserDefinedType;
		void InsertType(const std::string& name, AlloyType* type, UserDefinedType userDefinedType, 
			const std::unordered_map<std::string_view, TypeMemberInfo>& memberInfo = {},
			const GenericTypeMap& genericTypeMap = {}
			);
		bool GetStructMembers(const std::string_view& structName, std::unordered_map<std::string_view, TypeMemberInfo>& structMembers);
		bool GetEnumMembers(const std::string_view& enumName, std::unordered_map<std::string_view, TypeMemberInfo>& enumMembers);

		// support for generic function parameters
		AlloyType* GetGenericType(const std::string& typeName);
		void SetGenericType(const std::string& typeName, AlloyType* realType);

		// returns the current function's generic type map
		GenericTypeMap GetGenericTypeMap();
		// returns the generic type map for a specific type
		bool GetGenericTypeMap(const std::string_view& structName, GenericTypeMap& genericTypeMap);

		llvm::Value*& GetEnumCapturedValue() { return m_ScopeStack.back().EnumCapturedValue; }

		// should be called before PopScope in order to free memory allocated on the heap
		void FreeHeapPointers(llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>& builder);

		// check if specific type is an enumeration
		bool IsEnumType(AlloyType* type);

		// return the unique ID for that type
		unsigned int GetID(AlloyType* type);

		// return the structure type associated with a specific payload type
		AlloyType* GetEnumPayloadStruct(llvm::LLVMContext& llvmContext, AlloyType* PayloadType);

		// dump all registered type names for debugging purposes
		void DumpTypeNames();

	private:
		struct TypeInfo
		{
			AlloyType* Type;
			std::string_view Name;
			UserDefinedType userDefinedType;
			std::unordered_map<std::string_view, TypeMemberInfo> memberInfo;
			GenericTypeMap genericTypeMap;
			unsigned int ID;		// a unique internal ID that identifies each type
		};

		struct Scope
		{
			std::string_view Name;
			std::unordered_map<std::string, AlloyValue> Values;
			std::unordered_map<std::string, TypeInfo> Types;
			std::unordered_map<const AlloyType*, std::string> TypeNames; // used for error messages
			GenericTypeMap GenericTypeMap;	// this is a map from the generic types to the actual function parameter types
			llvm::Value* EnumCapturedValue;									// captured payload value in if or switch statements

			Scope(const std::string_view& name)
				: Name(name), EnumCapturedValue(nullptr)
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
