#include "llvm/llvm.hpp"
#include "NamedValues.hpp"

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
		InsertType("String", llvm::PointerType::get(llvm::IntegerType::get(llvmContext, 8), 0), UserDefinedType::basic); // convert string to u8*

		// insert the default types
		InsertType("bool", llvm::Type::getInt1Ty(llvmContext), UserDefinedType::basic);

		InsertType("i8", llvm::Type::getInt8Ty(llvmContext), UserDefinedType::basic);
		InsertType("i16", llvm::Type::getInt16Ty(llvmContext), UserDefinedType::basic);
		InsertType("i32", llvm::Type::getInt32Ty(llvmContext), UserDefinedType::basic);
		InsertType("i64", llvm::Type::getInt64Ty(llvmContext), UserDefinedType::basic);

		InsertType("u8", llvm::Type::getInt8Ty(llvmContext), UserDefinedType::basic);
		InsertType("u16", llvm::Type::getInt16Ty(llvmContext), UserDefinedType::basic);
		InsertType("u32", llvm::Type::getInt32Ty(llvmContext), UserDefinedType::basic);
		InsertType("u64", llvm::Type::getInt64Ty(llvmContext), UserDefinedType::basic);

		InsertType("f32", llvm::Type::getFloatTy(llvmContext), UserDefinedType::basic);
		InsertType("f64", llvm::Type::getDoubleTy(llvmContext), UserDefinedType::basic);
	}

	llvm::Type* NamedValues::GetEnumPayloadStruct(llvm::LLVMContext& llvmContext, llvm::Type* PayloadType)
	{
		// 
		// EnumPayloadStruct is a structure that contains the index, payload and enum ID for an enum value
		// Because the payload can be of any type, each time a new type is requested we check if we already have such a structure defined, if not define and return the structure
		// The structures are stored in the NamedValues::Types map the same way all other types are stored. The name of the structure type is _EnumPayloadStruct_[PayloadType]
		// 
		std::string_view payloadTypeName = (PayloadType == nullptr ? "" : GetTypeName(PayloadType));
		std::string payloadStructName = _EnumPayloadStruct_ + std::string(payloadTypeName);
		
		// check if the same structure has already been defined, otherwise define it
		llvm::Type* EnumPayloadStruct = GetType(payloadStructName);
		if (nullptr == EnumPayloadStruct) {
			std::vector<llvm::Type*> memberTypes;
			memberTypes.push_back(llvm::Type::getInt64Ty(llvmContext));		// index into the enum
			memberTypes.push_back(PayloadType != nullptr ? PayloadType : llvm::Type::getInt8Ty(llvmContext));			// payload
			memberTypes.push_back(llvm::Type::getInt64Ty(llvmContext));		// the actual enum ID, needed to compare enum values
			EnumPayloadStruct = llvm::StructType::create(llvmContext, memberTypes, payloadStructName, true);

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
		const std::unordered_map<std::string, std::string>& previousTypeMap = m_ScopeStack.back().GenericTypeMap;

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
			if (it.second.freeOnExit && it.second.containedType != nullptr) {
				// load the underlying hear pointer created using Malloc and free it
				llvm::Value* Ptr = builder.CreateLoad(it.second.value->getType(), it.second.value);
				builder.CreateFree(Ptr);
			}
		}
	}


	ValueTypePair* NamedValues::GetValue(const std::string& name, bool searchInParents /*= true*/)
	{
		//
		// searchInParents can be set to false in situations where we need to search the current level only and not go through the parent tree
		//
		ValueTypePair* result = nullptr;

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

	void NamedValues::InsertValue(const std::string& name, llvm::AllocaInst* value, llvm::Type* type, llvm::Type* parentType, bool isConst, bool freeOnExit)
	{
		ASSERT(!m_ScopeStack.back().Values.contains(name), "Named value already exists! Should check if it exists first with NamedValues::GetValue(const std::string& name).");
		ValueTypePair valueTypePair = { value, type, parentType, isConst, freeOnExit };
		m_ScopeStack.back().Values[name] = valueTypePair;
	}

	llvm::Type* NamedValues::GetType(const std::string_view& name)
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
	bool NamedValues::IsEnumType(llvm::Type* type)
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
	unsigned int NamedValues::GetID(llvm::Type* type)
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

	bool NamedValues::GetGenericTypeMap(const std::string_view& typeName, std::unordered_map<std::string, std::string>& genericTypeMap)
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

	NamedValues::TypeMemberInfo NamedValues::GetStructMemberIndex(const std::string_view& structName, const std::string_view& memberName)
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

	NamedValues::TypeMemberInfo NamedValues::GetEnumMemberIndex(const std::string_view& enumName, const std::string_view& memberName)
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

	std::string_view NamedValues::GetTypeName(const llvm::Type* type)
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

		ASSERT(false, "Type name not found! This only happens if type wasn't added to NamedValues when it should.");
		return "";
	}

	void NamedValues::InsertType(const std::string& name, llvm::Type* type, UserDefinedType userDefinedType,
		const std::unordered_map<std::string_view, TypeMemberInfo>& memberInfo,
		const std::unordered_map<std::string, std::string>& genericTypeMap
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
	std::string NamedValues::GetGenericType(const std::string& typeName)
	{
		auto found = m_ScopeStack.back().GenericTypeMap.find(typeName);
		if (found == m_ScopeStack.back().GenericTypeMap.end()) {
			return typeName;
		}
		else {
			return found->second;
		}
	}

	void NamedValues::SetGenericType(const std::string& typeName, const std::string& Type)
	{
		m_ScopeStack.back().GenericTypeMap[typeName] = Type;
	}

	std::unordered_map<std::string, std::string> NamedValues::GetGenericTypeMap()
	{
		return m_ScopeStack.back().GenericTypeMap;
	}
}