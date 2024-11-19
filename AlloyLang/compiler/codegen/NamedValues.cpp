#include "llvm/llvm.hpp"
#include "NamedValues.hpp"
#include "CodeGenerator.hpp"
#include "AlloyType.hpp"
#include "AlloyValue.hpp"
#include "LibraryFunctions.hpp"
#include "Inlines.hpp"
#include "SmartPointerClass.hpp"
#include "../../log/Log.hpp"

namespace AlloyCompiler
{

	/* -- PUBLIC -- */

	NamedValues::NamedValues() :
		NextTypeID(0)
	{
		m_ScopeStack.emplace_back("");
	}

	NamedValues::~NamedValues()
	{
	}

	void NamedValues::RegisterDefaultTypes(llvm::LLVMContext& llvmContext)
	{
		// TODO: remove
		InsertType("String", AlloyType::getPointerType(llvm::IntegerType::get(llvmContext, 8)), UserDefinedType::basic); // convert string to u8*

		// insert the default types
		InsertType("bool", AlloyType::getIntType(llvmContext, 1, false, "bool"), UserDefinedType::basic);

		InsertType("i8", AlloyType::getIntType(llvmContext, 8, true, "i8"), UserDefinedType::basic);
		InsertType("i16", AlloyType::getIntType(llvmContext, 16, true, "i16"), UserDefinedType::basic);
		InsertType("i32", AlloyType::getIntType(llvmContext, 32, true, "i32"), UserDefinedType::basic);
		InsertType("i64", AlloyType::getIntType(llvmContext, 64, true, "i64"), UserDefinedType::basic);

		InsertType("u8", AlloyType::getIntType(llvmContext, 8, false, "u8"), UserDefinedType::basic);
		InsertType("u16", AlloyType::getIntType(llvmContext, 16, false, "u16"), UserDefinedType::basic);
		InsertType("u32", AlloyType::getIntType(llvmContext, 32, false, "u32"), UserDefinedType::basic);
		InsertType("u64", AlloyType::getIntType(llvmContext, 64, false, "u64"), UserDefinedType::basic);

		InsertType("f32", AlloyType::get(llvm::Type::getFloatTy(llvmContext)), UserDefinedType::basic);
		InsertType("f64", AlloyType::get(llvm::Type::getDoubleTy(llvmContext)), UserDefinedType::basic);
	}

	AlloyType* NamedValues::GetEnumPayloadStruct(llvm::LLVMContext& llvmContext, AlloyType* PayloadType)
	{
		// 
		// EnumPayloadStruct is a structure that contains the index, payload and enum ID for an enum value
		// Because the payload can be of any type, each time a new type is requested we check if we already have such a structure defined, if not define and return the structure
		// The structures are stored in the NamedValues::Types map the same way all other types are stored. The name of the structure type is _EnumPayloadStruct_[PayloadType]
		// 
		std::string_view payloadTypeName = (PayloadType == nullptr ? "" : GetTypeName(PayloadType));
		std::string payloadStructName = _EnumPayloadStruct_ + std::string(payloadTypeName);
		
		// check if the same structure has already been defined, otherwise define it
		AlloyType* EnumPayloadStruct = GetType(payloadStructName);
		if (nullptr == EnumPayloadStruct) {
			std::vector<llvm::Type*> memberTypes;
			memberTypes.push_back(AlloyType::get("i64")->llvmType);		// index into the enum
			memberTypes.push_back(PayloadType != nullptr ? PayloadType->llvmType : AlloyType::get("i8")->llvmType);			// payload
			memberTypes.push_back(AlloyType::get("i64")->llvmType);		// the actual enum ID, needed to compare enum values
			EnumPayloadStruct = AlloyType::get(llvm::StructType::create(llvmContext, memberTypes, payloadStructName, true));

			InsertType(payloadStructName, EnumPayloadStruct, UserDefinedType::structure);
		}

		return EnumPayloadStruct;
	}

	void NamedValues::PushScope(const std::string_view& name)
	{
		//
		// for the generic type maps, we copy the previous map into the current one, new types will then either overwrite or be added to the map
		// e.g. we can start with:
		// T -> i32 
		// and end up with :
		// T -> i64
		// T1 ->f32
		//
		const GenericTypeMap& previousTypeMap = m_ScopeStack.back().GenericTypeMap;

		m_ScopeStack.emplace_back(name);

		m_ScopeStack.back().GenericTypeMap.insert(previousTypeMap.begin(), previousTypeMap.end());
	}

	void NamedValues::PopScope()
	{
		ASSERT(m_ScopeStack.size() > 1, "Cannot pop the root scope!");

		m_ScopeStack.pop_back();
	}

	void NamedValues::FreeHeapPointers(llvm::IRBuilder<>& builder)
	{
		//
		// Free memory allocated on the heap
		// When a pointer variable goes out of scope, we should free its memory
		//
		for (auto& it : m_ScopeStack.back().Values) {

			// we have to make sure that we do not attempt to free references which are also pointers
			// pointers and references have their type set explicitely
			if (it.second.freeOnExit && it.second.Type->containedType != nullptr) {
				// load the underlying heap pointer created using Malloc and free it
				// llvm::Value* Ptr = builder.CreateLoad(it.second.Type, it.second.value);
				// builder.CreateFree(Ptr);
			}
		}
	}


	const AlloyValue* NamedValues::GetValue(const std::string& name, bool searchInParents /*= true*/)
	{
		//
		// searchInParents can be set to false in situations where we need to search the current level only and not go through the parent tree
		//
		AlloyValue* result = nullptr;

		// look for the value starting from the current scope and going up
		for (auto it = m_ScopeStack.rbegin(); it != m_ScopeStack.rend(); ++it)
		{
			auto& scope = *it;
			auto found = scope.Values.find(name);
			if (found != scope.Values.end())
			{
				result = &found->second;
				break;
			}
			if (!searchInParents)
				break;
		}

		return result;
	}

	bool NamedValues::RemoveValue(const std::string& name)
	{
		//
		// remove a value from map, returns true if value was found, false otherwise
		//
		bool result = false;

		// look for the value starting from the current scope and going up
		for (auto it = m_ScopeStack.rbegin(); it != m_ScopeStack.rend(); ++it)
		{
			auto& scope = *it;
			auto found = scope.Values.find(name);
			if (found != scope.Values.end())
			{
				scope.Values.erase(name);
				result = true;
				break;
			}
		}

		return result;
	}

	void NamedValues::InsertValue(const std::string& name, llvm::AllocaInst* ptr, AlloyType* type, AlloyType* parentType, bool isConst, bool freeOnExit)
	{
		ASSERT(!m_ScopeStack.back().Values.contains(name), "Named value already exists! Should check if it exists first with NamedValues::GetValue(const std::string& name).");
		AlloyValue alloyValue(nullptr, type, ptr, isConst);
		alloyValue.freeOnExit = freeOnExit;
		alloyValue.parentType = parentType;
		m_ScopeStack.back().Values[name] = alloyValue;
	}

	bool NamedValues::UpdateValue(const std::string& name, const AlloyValue& value)
	{
		bool result = false;

		// look for the value starting from the current scope and going up
		for (auto it = m_ScopeStack.rbegin(); it != m_ScopeStack.rend(); ++it)
		{
			auto& scope = *it;
			auto found = scope.Values.find(name);
			if (found != scope.Values.end())
			{
				found->second = value;
				result = true;
				break;
			}
		}

		ASSERT(result, "Named value not found and cannot be updated.");
		return result;
	}

	void NamedValues::InsertValue(const std::string& name, const AlloyValue& value)
	{
		ASSERT(!m_ScopeStack.back().Values.contains(name), "Named value already exists! Should check if it exists first with NamedValues::GetValue(const std::string& name).");
		m_ScopeStack.back().Values[name] = value;
	}

	AlloyType* NamedValues::GetType(const std::string_view& name)
	{
		TypeInfo* typeInfo = findType(name);

		if (typeInfo)
		{
			return typeInfo->Type;
		}

		return nullptr;
	}

	bool NamedValues::GetStructMembers(const std::string_view& structName, std::unordered_map<std::string_view, NamedValues::TypeMemberInfo>& structMembers)
	{
		TypeInfo* typeInfo = findType(structName);

		if (typeInfo && typeInfo->userDefinedType == structure)
		{
			structMembers = typeInfo->memberInfo;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool NamedValues::GetEnumMembers(const std::string_view& enumName, std::unordered_map<std::string_view, NamedValues::TypeMemberInfo>& enumPayloadMap)
	{
		TypeInfo* typeInfo = findType(enumName);

		if (typeInfo && typeInfo->userDefinedType == enumeration)
		{
			enumPayloadMap = typeInfo->memberInfo;
			return true;
		}
		else
		{
			return false;
		}
	}

	// check if specific type is an enumeration
	bool NamedValues::IsEnumType(AlloyType* type)
	{
		// check if specific type is an enumeration
		TypeInfo* typeInfo = findType(GetTypeName(type));

		if (typeInfo && typeInfo->userDefinedType == enumeration)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	// check if specific type is an enumeration
	unsigned int NamedValues::GetID(AlloyType* type)
	{
		// check if specific type is an enumeration
		TypeInfo* typeInfo = findType(GetTypeName(type));

		if (typeInfo)
		{
			return typeInfo->ID;
		}
		else
		{
			return 0;
		}
	}

	bool NamedValues::GetGenericTypeMap(const std::string_view& typeName, GenericTypeMap& genericTypeMap)
	{
		TypeInfo* typeInfo = findType(typeName);

		if (typeInfo)
		{
			genericTypeMap = typeInfo->genericTypeMap;
			return true;
		}
		else
		{
			return false;
		}
	}

	NamedValues::TypeMemberInfo NamedValues::GetStructMemberInfo(const std::string_view& structName, const std::string_view& memberName)
	{
		TypeInfo* typeInfo = findType(structName);

		if (!typeInfo || typeInfo->userDefinedType != UserDefinedType::structure)
		{
			return { -1, nullptr };
		}

		auto it = typeInfo->memberInfo.find(memberName);

		if (it == typeInfo->memberInfo.end())
		{
			return { -2, nullptr };
		}

		return it->second;
	}

	NamedValues::TypeMemberInfo NamedValues::GetEnumMemberInfo(const std::string_view& enumName, const std::string_view& memberName)
	{
		TypeInfo* typeInfo = findType(enumName);

		if (!typeInfo || typeInfo->userDefinedType != UserDefinedType::enumeration)
		{
			return { -1, nullptr };
		}

		auto it = typeInfo->memberInfo.find(memberName);

		if (it == typeInfo->memberInfo.end())
		{
			return { -2, nullptr };
		}

		return it->second;
	}

	std::string_view NamedValues::GetTypeName(const AlloyType* type)
	{
		// look for the type starting from the current scope and going up
		for (auto it = m_ScopeStack.rbegin(); it != m_ScopeStack.rend(); ++it)
		{
			auto& scope = *it;
			auto found = scope.TypeNames.find(type);
			if (found != scope.TypeNames.end())
			{
				return found->second;
			}
		}

#ifdef _DEBUG
		std::cout << "llvm Type not found: ";
		type->llvmType->dump();
		DumpTypeNames();
#endif
		ASSERT(false, "Type name not found! This only happens if type wasn't added to NamedValues when it should.");
		return "";
	}

	void NamedValues::InsertType(const std::string& name, AlloyType* type, UserDefinedType userDefinedType,
		const std::unordered_map<std::string_view, TypeMemberInfo>& memberInfo,
		const GenericTypeMap& genericTypeMap
		)
	{
		ASSERT(!m_ScopeStack.back().Types.contains(name), "Named type already exists! Should check if it exists first with NamedValues::GetType(const std::string& name).");
		ASSERT(memberInfo.empty() || userDefinedType == UserDefinedType::structure || userDefinedType == UserDefinedType::enumeration, "Struct and enum members can only be added to a struct or enum type.");

		TypeInfo typeInfo;
		typeInfo.Type = type;
		typeInfo.Name = name;
		typeInfo.userDefinedType = userDefinedType;
		typeInfo.memberInfo = memberInfo;
		typeInfo.genericTypeMap = genericTypeMap;
		typeInfo.ID = ++NextTypeID;		// unique ID for this type

		m_ScopeStack.back().Types[name] = typeInfo;
		m_ScopeStack.back().TypeNames[type] = name;
	}

	void NamedValues::DumpTypeNames()
	{
		//
		// dump all registered type names for debugging purposes
		//
		std::string tabs = "";

		for (auto it = m_ScopeStack.rbegin(); it != m_ScopeStack.rend(); ++it)
		{
			auto& scope = *it;
			std::cout << std::vformat("{0}Scope {1}:", std::make_format_args(tabs, scope.Name)) << std::endl;

			for (auto typeName = scope.TypeNames.begin(); typeName != scope.TypeNames.end(); typeName++)
			{
				std::cout << std::vformat("{0}{1} ==> ", std::make_format_args(tabs, typeName->second));
				typeName->first->llvmType->dump();
			}
			tabs = tabs + "\t";
		}
	}


	/* -- PRIVATE -- */

	NamedValues::TypeInfo* NamedValues::findType(const std::string_view& name)
	{
		// look for the type name starting from the current scope and going up
		for (auto it = m_ScopeStack.rbegin(); it != m_ScopeStack.rend(); ++it)
		{
			auto& scope = *it;
			auto found = scope.Types.find(std::string(name));
			if (found != scope.Types.end())
			{
				return &found->second;
			}
		}

		return nullptr;
	}

	// support for generic function parameters
	AlloyType* NamedValues::GetGenericType(const std::string& typeName)
	{
		auto found = m_ScopeStack.back().GenericTypeMap.find(typeName);
		if (found == m_ScopeStack.back().GenericTypeMap.end()) {
			return nullptr;
		}
		else {
			return found->second;
		}
	}

	void NamedValues::SetGenericType(const std::string& typeName, AlloyType* Type)
	{
		m_ScopeStack.back().GenericTypeMap[typeName] = Type;
	}

	GenericTypeMap NamedValues::GetGenericTypeMap()
	{
		return m_ScopeStack.back().GenericTypeMap;
	}
}