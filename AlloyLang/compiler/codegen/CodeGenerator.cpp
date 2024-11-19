#include "llvm/llvm.hpp"
#include "NamedValues.hpp"
#include "CodeGenerator.hpp"
#include "AlloyType.hpp"
#include "AlloyValue.hpp"
#include "LibraryFunctions.hpp"
#include "Inlines.hpp"
#include "SmartPointerClass.hpp"

namespace AlloyCompiler
{

#pragma region Util

	// forward declarations
	AlloyValue generateExpression(ModuleTable& moduleTable, LLVMState& state, const EXPRESSION& expressionNode,
		AlloyType*& expectedType);
	AlloyValue generateVariableDefinition(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DEFINITION& variableDefinition);
	llvm::Value* generateStatement(ModuleTable& moduleTable, LLVMState& state, const STATEMENT& statement);
	llvm::Value* generateStatementBlock(ModuleTable& moduleTable, LLVMState& state, const STATEMENT_BLOCK& statementBlock);
	llvm::Function* generateFunctionDefinition(ModuleTable& moduleTable, LLVMState& state, AlloyType* parentType, const FUNCTION_DEFINITION& functionDefinition, const std::string& moduleName,
		const std::vector<TYPE*>& functionArguments, const std::vector<AlloyValue>& argumentValues);
	AlloyType* generateTypeDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_DEFINITION& typeDefinition, const std::string& moduleName,
		const GenericArgumentTypes& genericArguments);
	AlloyType* generateStructDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
		const STRUCT_TYPE& structDefinition, const GenericArgumentTypes& genericArguments);
	AlloyType* generateEnumDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
		const ENUM_TYPE& enumDefinition, const GenericArgumentTypes& genericArguments);
	AlloyType* generateArrayDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
		const ARRAY_TYPE& arrayDefinition, const GenericArgumentTypes& genericArguments,
		const GenericTypeMap& parentTypeMap);
	llvm::Function* generateExternDefinition(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_DEFINITION& externDefinition, const std::string& moduleName);
	AlloyType* getTypeFromTypeName(ModuleTable& moduleTable, LLVMState& state, Token* pNameToken,
		const GenericArgumentTypes& genericArguments,
		const GenericTypeMap& genericTypeMap);
	AlloyType* generateTypeIdentifier(ModuleTable& moduleTable, LLVMState& state, const TYPE& typeIdentifier,
		Token* pParentTypeNameToken, const GenericTypeMap& genericTypeMap);

	std::string GetMangledName(LLVMState& state, const std::string_view& moduleName, const std::string_view& typeName, const GenericArgumentTypes& genericArguments,
		const GenericTypeMap& genericTypeMap)
	{
		//
		// return the mangled name for a type with generic arguments
		//
		std::string mangledName(moduleName);
		if (!mangledName.empty())
			mangledName += "::";
		mangledName += std::string(typeName);

		if (!genericArguments.empty()) {
			for (auto& t : genericArguments) {
				if (genericTypeMap.contains(t->name()))
					mangledName = mangledName + "@" + genericTypeMap.at(t->name())->name();
				else
					mangledName = mangledName + "@" + t->name();
			}
		}

		return mangledName;
	}

	template<typename ...Args>
	constexpr void logInfoAtCurrentPosition(const ModuleTable& moduleTable, const Token* pToken, const std::string& format, Args && ...args)
	{
		if (pToken != nullptr)
		{
			const Module& currentModule = moduleTable.GetCurrentModule();

			const Location& location = pToken->Location;
			const size_t tokenSize = pToken->Value.size();
			const std::string_view line = currentModule.GetSource().GetLine(location.LineStart);

			Log::Print("({0}:{1}) INFO:", location.Line, location.Column);
			Log::Print("    {0}", line);
			Log::Print("    {0}{1}", std::string(location.Column - 1, ' '), std::string(tokenSize, '~'));
			Log::Print("    {0}{1}", std::string(location.Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
		}
		else
		{
			Log::Print("    {0}", std::vformat(format, std::make_format_args(args...)));
		}
	}

	template<typename ...Args>
	constexpr void logErrorAtCurrentPosition(const ModuleTable& moduleTable, const Token* pToken, const std::string& format, Args && ...args)
	{
		if (pToken != nullptr)
		{
			const Module& currentModule = moduleTable.GetCurrentModule();

			const Location& location = pToken->Location;
			const size_t tokenSize = pToken->Value.size();
			const std::string_view line = currentModule.GetSource().GetLine(location.LineStart);

			Log::Error("({0}:{1}) ERROR:", location.Line, location.Column);
			Log::Error("    {0}", line);
			Log::Error("    {0}{1}", std::string(location.Column - 1, ' '), std::string(tokenSize, '~'));
			Log::Error("    {0}{1}", std::string(location.Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
		}
		else
		{
			Log::Error("    {0}", std::vformat(format, std::make_format_args(args...)));
		}

		ASSERT(false, "Check log file for errors!");	// all errors should assert in debug mode
	}

	std::vector<llvm::Value*> getGEPIndex(LLVMState& state, int index)
	{
		//
		// Given a index into an array or structure element, convert it to a the format required
		// by llvm GEP (GetElementPtr) operarions
		// Array and structure members are accessed through 2 indices, this is described in detail here:
		// https://llvm.org/docs/GetElementPtr.html
		// 
		// convert our index to an llvm 32-bit Int
		llvm::Value* llvmIndex = llvm::ConstantInt::get(*state.Context, llvm::APInt(32, index, true));

		std::vector<llvm::Value*> indices(2);
		indices[0] = llvm::ConstantInt::get(*state.Context, llvm::APInt(32, 0, true));
		indices[1] = llvmIndex;

		return indices;
	}

	void branchIfNotDuplicate(LLVMState& state, llvm::BasicBlock* destination)
	{
		//
		// check if the code block ends with a branch, if not insert a branch to the provided destination
		// this is needed because if the function ends with 2 consecutive branches, the code optimizers fail
		//
		llvm::Instruction* PTI = state.Builder->GetInsertBlock()->getTerminator();
		if (PTI == nullptr
			|| PTI->getOpcode() != llvm::Instruction::Br) {
			state.Builder->CreateBr(destination);
		}
	}

	llvm::Value* convertToBool(LLVMState& state, llvm::Value* value)
	{
		// convert condition to a bool by comparing non-equal to 0
		// TODO: extend to all possible data types (eg pointers)
		if (value->getType()->getTypeID() == llvm::Type::TypeID::IntegerTyID)
		{
			return state.Builder->CreateICmpNE(value,
				llvm::ConstantInt::get(*state.Context, llvm::APInt(value->getType()->getIntegerBitWidth(), 0)), "ifcond");
		}
		else
		{
			return state.Builder->CreateFCmpONE(value, llvm::ConstantFP::get(*state.Context, llvm::APFloat(0.0)), "ifcond");
		}
	}

	GenericArgumentTypes ProcessGenericArguments(ModuleTable& moduleTable, LLVMState& state,
		const std::vector<TYPE*>& genericArguments,
		const GenericTypeMap& typeMap)
	{
		//
		// Given a vector of arguments of type AlloyCompiler::TYPE, convert to a vector of AlloyType(s) for these arguments
		//
		GenericArgumentTypes arguments;

		for (TYPE* argument : genericArguments) {
			if (argument->Type.Is<TYPE_NAME>()) {
				TYPE_NAME* typeName = argument->Type.Get<TYPE_NAME>();
				std::string typeNameStr(typeName->pNameToken->Value);
				AlloyType* alloyType = nullptr;
				if (typeMap.contains(typeNameStr)) {
					alloyType = typeMap.at(typeNameStr);
				}
				else {
					// search for the type definition in the module table
					SearchResult<TYPE_DEFINITION> result = moduleTable.GetTypeDefinition(typeName->pNameToken->Value);

					if (result.Code == SearchResultCode::NotFound)
					{
						logErrorAtCurrentPosition(moduleTable, typeName->pNameToken, "Type '{0}' does not exist.", typeName->pNameToken->Value);
					}
					else if (result.Code == SearchResultCode::Inaccessible)
					{
						logErrorAtCurrentPosition(moduleTable, typeName->pNameToken, "Type '{0}' is private and cannot be accessed here.", typeName->pNameToken->Value);
					}
					else
					{
						alloyType = getTypeFromTypeName(moduleTable, state, typeName->pNameToken,
							ProcessGenericArguments(moduleTable, state, typeName->GenericArguments, typeMap),
							typeMap);
					}
				}
				arguments.push_back(alloyType);
			}
			else {
				ASSERT(false, "Expected TYPE_NAME");
				arguments.push_back(nullptr);
			}
		}

		return arguments;
	}

#if 0
	std::string unescapeString(const std::string_view& str)
	{
		static const std::unordered_map<char, char> escapeCharacterMap =
		{
			{ 'n',	'\n' },
			{ 'r',	'\r' },
			{ 't',	'\t' },
			{ 'v',	'\v' },
			{ '\\', '\\' },
			{ '\'', '\'' },
			{ '\"', '\"' },
			{ '0',	'\0' },
			{ 's',	' ' }
		};

		std::string result;
		result.reserve(str.Size());

		for (size_t i = 0; i < str.Size(); i++)
		{
			char c = str[i];

			if (c == '\\' && i + 1 < str.Size())
			{
				i++;

				auto it = escapeCharacterMap.find(str[i]);

				if (it != escapeCharacterMap.end())
				{
					c = it->second;
				}

				else
				{
					continue;	// TODO: log error?
				}
			}

			result.push_back(c);
		}

		return result;
	}
#endif

	llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function, const std::string_view& varName, AlloyType* type, int numElements = 0)
	{
		//
		// For function definitions, allocate memory for function parameters in the entry block of the function
		// numElements is the number of elements in an array, not used otherwise
		//
		llvm::AllocaInst* result;
		llvm::IRBuilder<> tempBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
		llvm::Value* elements = nullptr;
		if (numElements > 0) {
			elements = llvm::ConstantInt::get(function->getContext(), llvm::APInt(64, numElements));
		}
		result = tempBuilder.CreateAlloca(type->llvmType, elements, varName);
		return result;
	}

	AlloyType* getTypeFromTypeName(ModuleTable& moduleTable, LLVMState& state, Token* pNameToken,
		const GenericArgumentTypes& genericArguments,
		const GenericTypeMap& genericTypeMap)
	{
		//
		// Helper function to return an llvm type given its type name
		// Also searches the named nodes if the type has not been defined yet and define it
		// For generic types, genericArguments and genericTypeMap are used top map the generic type to an actual type (e.g. T to i32)
		// This function assumes that the current module has been set by the caling function
		//

		AlloyType* type = nullptr;
		std::string mangledName(pNameToken->Value);

		// if we have a type map that contains the requested type, retrieve the actual type, e.g. T might be an i32
		if (genericTypeMap.contains(mangledName))
		{
			type = genericTypeMap.at(mangledName);
		}

		if (nullptr == type)
		{
			// first we search for the basic hard-coded types
			type = state.NamedValues.GetType(mangledName);
			if (nullptr == type) {

				// search for the type definition in the module table
				SearchResult<TYPE_DEFINITION> result = moduleTable.GetTypeDefinition(mangledName);

				if (result.Code == SearchResultCode::NotFound)
				{
					logErrorAtCurrentPosition(moduleTable, pNameToken, "Type '{0}' does not exist.", mangledName != pNameToken->Value ? std::string(pNameToken->Value) + " (" + mangledName + ")" : mangledName);
				}
				else if (result.Code == SearchResultCode::Inaccessible)
				{
					logErrorAtCurrentPosition(moduleTable, pNameToken, "Type '{0}' is private and cannot be accessed here.", mangledName != pNameToken->Value ? std::string(pNameToken->Value) + " (" + mangledName + ")" : mangledName);
				}
				else
				{
					if (genericArguments.size() != result.pDefiniton->pTypeIdentifier->GenericParameters.size()) {
						logErrorAtCurrentPosition(moduleTable, pNameToken, "Invalid number of parameters for generic type {0}!", mangledName);
						goto failed;
					}

					mangledName = GetMangledName(state, "", result.MangledName, genericArguments, genericTypeMap);

					// now check if this type has already been defined
					type = state.NamedValues.GetType(mangledName);

					// if not already defined, try to define it now
					if (nullptr == type) {
						// if not already defined, try to define it now
						type = generateTypeDefinition(moduleTable, state, *result.pDefiniton, result.ModuleName, genericArguments);
						ASSERT(type == state.NamedValues.GetType(mangledName), "Type has been defined with a mangled name different from expected one!");
					}
				}
			}
		}

	failed:
		return type;
	}

	std::string getExtendedFunctionName(ModuleTable& moduleTable, LLVMState& state,
		const std::string_view& moduleName, const std::string_view& typeName,
		const std::string_view& functionName,
		const std::vector<GENERIC_PARAMETER*>& genericParameters,
		const std::vector<TYPE*>& genericArguments,
		const std::vector<VARIABLE_DECLARATION*> Parameters,
		const std::vector<AlloyValue>& argumentValues,
		const GenericTypeMap& parentTypeMap,
		const GenericTypeMap& typeMap
	)
	{
		//
		// Returns moduleName::typeName@functionName@type1@type2...
		// the types are retrieved from the function parameters in the case of generic functions
		// 
		// Support for generics:
		// The parameter list "parameters" is needed in order to determine if any parameter is a type
		// The functionArguments list is needed to know what is the type refered to by the corresponding parameter
		// On return, typeMap will contain a map from function parameter types to actual types
		//
		std::string mangled(moduleName);

		ASSERT(genericParameters.size() == genericArguments.size(), "The number of generic parameters and arguments do not match!");

		if (!mangled.empty())
			mangled += "::";
		if (!typeName.empty()) {
			mangled += typeName;
			mangled += "@";
		}
		mangled += std::string(functionName);

		int argument = 0;
		for (GENERIC_PARAMETER* genericParameter : genericParameters)
		{
			TYPE* type = genericArguments[argument];

			TYPE_NAME* typeName = type->Type.Get<TYPE_NAME>();
			if (typeName) {
				mangled += "@";
				if (typeMap.contains(std::string(typeName->pNameToken->Value)))
					mangled += typeMap.at(std::string(typeName->pNameToken->Value))->name();
				else
					mangled += typeName->pNameToken->Value;
			}
			argument++;
		}

		// after the generic arguments, we add all the regular arguments and take care of the "Any" arguments
		for (int i = 0; i < Parameters.size(); i++) {
			VARIABLE_DECLARATION* var = Parameters[i];
			TypeModifier modifier = var->pType->Modifier;
			AlloyType* result;
			TYPE_NAME* typeName_ = var->pType->Type.Get<TYPE_NAME>();
			if (typeName_
				&& typeMap.contains(std::string(typeName_->pNameToken->Value))
				) {
				mangled += "@";
				mangled += typeMap.at(std::string(typeName_->pNameToken->Value))->name();
			}
			else if (var->isAny) {
				if (modifier == TypeModifier::Reference || modifier == TypeModifier::Pointer)
					result = argumentValues[i].Type->containedType;
				else
					result = argumentValues[i].Type;
				mangled += "@";
				mangled += state.NamedValues.GetTypeName(result);
			}
			else {
				Token tok = { typeName, var->pNameToken->Location, TokenKind::string_literal };
				result = generateTypeIdentifier(moduleTable, state, *var->pType,
					&tok, parentTypeMap);
				mangled += "@";
				mangled += state.NamedValues.GetTypeName(result);
			}
			// References and pointers are marked with a special character
			if (modifier == TypeModifier::Reference) {
				mangled += "&";
			}
			else if (modifier == TypeModifier::Pointer) {
				mangled += "*";
			}
		}

		return mangled;
	}

	bool buildGenericTypeMap(ModuleTable& moduleTable, LLVMState& state,
		const std::vector<GENERIC_PARAMETER*>& genericParameters,
		const std::vector<TYPE*>& genericArguments,
		GenericTypeMap& typeMap
	)
	{
		// 
		// Support for generics:
		// Build a map from the generic types to the actual types and return the map in typeMap
		//

		ASSERT(genericParameters.size() == genericArguments.size(), "The number of generic parameters and arguments do not match!");
		bool result = false;
		int argument = 0;
		for (GENERIC_PARAMETER* genericParameter : genericParameters)
		{
			TYPE* type = genericArguments[argument];

			if (!type->Type.Is<TYPE_NAME>()) {
				// TODO: only TYPE_NAME is currently supported
				logErrorAtCurrentPosition(moduleTable, genericParameter->pIdentifierToken, "Expected a type name.");
				goto failed;
			}

			TYPE_NAME* typeName_ = type->Type.Get<TYPE_NAME>();

			// check that the generic parameter has not been encountered already
			if (typeMap.contains(std::string(typeName_->pNameToken->Value))) {
				logErrorAtCurrentPosition(moduleTable, genericParameter->pIdentifierToken, "Type {0} ({1}) cannot be defined more than once.", genericParameter->pIdentifierToken->Value, typeName_->pNameToken->Value);

				goto failed;
			}

			typeMap[std::string(genericParameter->pIdentifierToken->Value)] = getTypeFromTypeName(moduleTable, state, typeName_->pNameToken, {}, {});

			argument++;
		}
		result = true;

	failed:
		return result;
	}

	
	bool isFunctionParameterConst(const FUNCTION_TYPE& functionType, int index)
	{
		//
		// for functions with variable number of arguments, the index can exceed the size of the parameters,
		//		in this case consider the parameter as constant as we cannot modify it
		//
		return (index < functionType.Parameters.size() ? (functionType.Parameters[index]->VarType == VariableType::Constant) : true);
	}

	bool isFunctionParameterByRef(const FUNCTION_TYPE& functionType, int index)
	{
		//
		// for functions with variable number of arguments, the index can exceed the size of the parameters,
		//		in this case the parameter cannot be ByRef
		//
		return (index < functionType.Parameters.size() ? (functionType.Parameters[index]->pType->Modifier == TypeModifier::Reference) : false);
	}

	AlloyType* getFunctionParameterType(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_TYPE& functionType, 
											const GenericTypeMap& typeMap, 
											TypeModifier& modifier, 
											int index
											)
	{
		//
		// return the type of function parameter
		// the result can be null for functions with variable number of arguments or if the type is not yet know (i.e. Any)
		//
		AlloyType* result = nullptr;
		modifier = (index < functionType.Parameters.size() ? functionType.Parameters[index]->pType->Modifier : TypeModifier::None);

		if (index < functionType.Parameters.size()
			&& !functionType.Parameters[index]->isAny
			) {
			result = generateTypeIdentifier(moduleTable, state, *functionType.Parameters[index]->pType,
				nullptr,
				typeMap);
			// References should be passed as pointers
			if (modifier == TypeModifier::Reference || modifier == TypeModifier::Pointer)
			{
				result = AlloyType::getPointerType(result);
			}
		}

		return result;
	}
#pragma endregion

#pragma region Literals

	AlloyValue generateLiteral(ModuleTable& moduleTable, LLVMState& state, const LITERAL& literalNode,
		AlloyType* identifierType)
	{
		//
		// Convert a literal into an llvm value
		// identifierType.type can be null, if provided it should contain the expected type
		//
		const std::string_view literalStr = literalNode.pValueToken->Value;
		llvm::Value* result = nullptr;

		// get the correct type of value that we are generating
		AlloyType* thisType = identifierType;
		bool isArray;
		AlloyType* elementType = SmartPointerClass::isSmartPointer(state, thisType, isArray);
		if (elementType)
			thisType = elementType;

		// TODO: allow negative values for ints and floats
		// in the current implementation, an additional call to the unary "-" is made. This call is removed by the optimizers so there are no adverse effects

		switch (literalNode.Type)
		{
		case LiteralType::Integer:
		{
			int64_t intValue;
			int bits = 64;	// default conversion to 64-bit integers
			if (thisType && thisType->isIntegerTy()) {
				// if we are expecting a specific integer type, generate an integer with the right type
				bits = thisType->getIntegerBitWidth();
			}
			std::from_chars_result temp = std::from_chars(literalStr.data(), literalStr.data() + literalStr.size(), intValue);
			result = AlloyValue::getConstantInt(*state.Context, bits, intValue);
			break;
		}

		case LiteralType::Float:
		{
			double dblValue;
			std::from_chars_result temp = std::from_chars(literalStr.data(), literalStr.data() + literalStr.size(), dblValue);
			result = llvm::ConstantFP::get(*state.Context, llvm::APFloat(dblValue));
			break;
		}

		case LiteralType::Boolean:
		{
			if (literalStr == "true")
			{
				result = llvm::ConstantInt::getTrue(*state.Context);
				break;
			}

			if (literalStr == "false")
			{
				result = llvm::ConstantInt::getFalse(*state.Context);
				break;
			}

			ASSERT(false, "Unknown boolean literal '{0}'!", literalStr);
			break;
		}

		case LiteralType::String:
			result = state.Builder->CreateGlobalStringPtr(literalStr);
			break;

		case LiteralType::Character:
			result = AlloyValue::getConstantInt(*state.Context, 8, literalStr[0]);
			break;

		default:
			ASSERT(false, "Unknown literal type!");
			break;
		}

		// if we know the type that we are expecting, convert right away to that type
		if (thisType == nullptr) {
			thisType = AlloyType::get(result->getType());
		}
		else {
			if (result && thisType) {
				AlloyValue::convertValueToType(state, result, thisType);
			}
		}

		return AlloyValue(result, thisType);
	}

#pragma endregion

#pragma region Identifiers

	AlloyValue generateIdentifier(ModuleTable& moduleTable, LLVMState& state, const VARIABLE& variable,
		AlloyType*& identifierType)
	{
		const std::string_view name = variable.pNameToken->Value;

		// check if we have a local variable with this name
		const AlloyValue* alloyValue = state.NamedValues.GetValue(std::string(name));

		if (alloyValue)
		{
			bool isArray;
			AlloyValue result;

			// load the actual value pointed to by the AllocaInst
			AlloyValue valueOrPtr(state.Builder->CreateLoad(alloyValue->Type->llvmType, alloyValue->Ptr, name), alloyValue->Type);

			// set the current type and parentType (for structs and enums)
			result.parentType = alloyValue->parentType;
			identifierType = valueOrPtr.Type;

			if (alloyValue->Type->containedType
				&& llvm::isa<llvm::PointerType>(valueOrPtr.Type->llvmType)) {
				// the type is set in the case of pointers and references, we need to load the pointed to value
				result.Value = state.Builder->CreateLoad(alloyValue->Type->containedType->llvmType, valueOrPtr.Value, "loadtmp");
				identifierType = AlloyType::get(result.Value->getType());
			}
			else if (SmartPointerClass::isSmartPointer(state, identifierType, isArray)) {
				// this is the same as the last else condition, keeping it here for debugging purposes
				// the caller should dereference the smart pointer if needed
				result.Value = valueOrPtr.Value;
				return AlloyValue(valueOrPtr.Value, identifierType, alloyValue->Ptr, alloyValue->isConst);
			}
			else {
				result.Value = valueOrPtr.Value;
			}

			result.Type = identifierType;
			result.Ptr = alloyValue->Ptr;
			result.isConst = alloyValue->isConst;
			state.NamedValues.UpdateValue(std::string(name), result);
			return result;
		}

		// check if we have a global
		llvm::GlobalVariable* globalVar = state.Module->getGlobalVariable(name, true);

		if (globalVar)
		{
			llvm::Value* value = state.Builder->CreateLoad(globalVar->getValueType(), globalVar, name);
			// TODO: we need to handle const globals
			return AlloyValue(value, AlloyType::get(globalVar->getValueType()), globalVar);
		}

		// check if the identifier is a function and return a pointer to that function
		SearchResult<FUNCTION_DEFINITION> funcResult = moduleTable.GetFunctionDefinition(name);

		if (funcResult.Code == SearchResultCode::Found)
		{
			// look up the function in the global module table
			llvm::Function* calleeFunc = state.Module->getFunction(funcResult.MangledName);
			if (calleeFunc == nullptr) {
				calleeFunc = generateFunctionDefinition(moduleTable, state, nullptr, *funcResult.pDefiniton, funcResult.ModuleName, {}, {});
			}

			if (calleeFunc != nullptr) {
				identifierType = AlloyType::getPointerType("i8");
				return AlloyValue(calleeFunc, identifierType);
			}
		}

		logErrorAtCurrentPosition(moduleTable, variable.pNameToken, "Unknown variable name '{0}'!", name);
		return AlloyValue();
	}

	AlloyType* generateTypeIdentifier(ModuleTable& moduleTable, LLVMState& state, const TYPE& typeIdentifier,
		Token* pParentTypeNameToken, const GenericTypeMap& genericTypeMap)
	{
		//
		// genericTypeMap maps from the generic type names to the actual type names, can be empty
		//
		AlloyType* identifierType = nullptr;
		GenericArgumentTypes genericArgumentTypes;

		// handle simple types
		if (typeIdentifier.Type.Is<TYPE_NAME>())
		{
			const TYPE_NAME& typeName = *typeIdentifier.Type.Get<TYPE_NAME>();

			Token* pNameToken = typeName.pNameToken;
			//
			// If the type name is Self, we take the parentTypeName as Self is not a real type
			//
			if (pNameToken->Value == "Self")
			{
				if (pParentTypeNameToken == nullptr)
				{
					logErrorAtCurrentPosition(moduleTable, typeName.pNameToken,
						"Self used with non member function!");
					goto exit;
				}
				pNameToken = pParentTypeNameToken;
			}

			// convert TYPE* arguments into AlloyType*
			genericArgumentTypes = ProcessGenericArguments(moduleTable, state, typeName.GenericArguments, genericTypeMap);
			identifierType = getTypeFromTypeName(moduleTable, state, pNameToken,
				genericArgumentTypes,
				genericTypeMap);

			if (!identifierType)
			{
				logErrorAtCurrentPosition(moduleTable, typeName.pNameToken,
					"Unknown type name '{0}'!", pNameToken->Value);
				goto exit;
			}
		}
		// handle array types
		else if (typeIdentifier.Type.Is<ARRAY_TYPE>())
		{
			const ARRAY_TYPE& type = *typeIdentifier.Type.Get<ARRAY_TYPE>();
			TYPE_IDENTIFIER ti = { nullptr, {} };
			identifierType = generateArrayDefinition(moduleTable, state, ti, type, genericArgumentTypes, genericTypeMap);
		}
		// handle struct types
		else if (typeIdentifier.Type.Is<STRUCT_TYPE>())
		{
			const STRUCT_TYPE& type = *typeIdentifier.Type.Get<STRUCT_TYPE>();
			TYPE_IDENTIFIER ti = { nullptr, {} };
			identifierType = generateStructDefinition(moduleTable, state, ti, type, genericArgumentTypes);
		}
		// handle enum types
		else if (typeIdentifier.Type.Is<ENUM_TYPE>())
		{
			const ENUM_TYPE& type = *typeIdentifier.Type.Get<ENUM_TYPE>();
			TYPE_IDENTIFIER ti = { nullptr, {} };
			identifierType = generateEnumDefinition(moduleTable, state, ti, type, genericArgumentTypes);
		}
		else {
			ASSERT(false, "Unknown type definition!");
		}

	exit:
		return identifierType;
	}

#pragma endregion

#pragma region Declarations

	AlloyType* generateTypeDeclaration(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_TYPE& typeDeclarationNode)
	{
		// TODO: var and const

		if (typeDeclarationNode.pReturnType->pType == nullptr) {
			return {};
		}
		else {
			return generateTypeIdentifier(moduleTable, state, *typeDeclarationNode.pReturnType->pType, nullptr, state.NamedValues.GetGenericTypeMap());
		}
	}

	AlloyValue generateVariableDeclaration(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DECLARATION& variableDeclarationNode,
		AlloyType*& identifierType)
	{
		const std::string_view name = variableDeclarationNode.pNameToken->Value;
		TypeModifier modifier = variableDeclarationNode.pType ? variableDeclarationNode.pType->Modifier : TypeModifier::None;
		GenericTypeMap genericTypeMap;

		// if the type is already known, i.e. inferred from the expression's value, we do not try get the type again
		if (nullptr == identifierType) {
			identifierType = generateTypeIdentifier(moduleTable, state, *variableDeclarationNode.pType, nullptr, genericTypeMap);

			if (!identifierType)
			{
				logErrorAtCurrentPosition(moduleTable, variableDeclarationNode.pNameToken, "Variable '{0}' type error!", name);
				return AlloyValue();
			}

			switch (modifier) {
			case TypeModifier::None:
				break;

			case TypeModifier::Pointer:
			{
				// allocating a pointer to this type
				identifierType = SmartPointerClass::GetSmartPointerStruct(state, identifierType, false);
				break;
			}

			case TypeModifier::Reference:
				// TODO: implement reference value declaration
				assert(false && "Not Implemented!");
				break;

			default:
			{
				ASSERT(false, "Invalid type modifier!");
				break;
			}
			}
		}

		// create the alloca
		llvm::AllocaInst* allocaInst = createEntryBlockAlloca(
			state.Builder->GetInsertBlock()->getParent(),
			name,
			identifierType
		);

		// add the variable to the named values
		AlloyValue alloyValue(nullptr, identifierType, allocaInst, (variableDeclarationNode.VarType == VariableType::Constant));
		alloyValue.freeOnExit = (modifier == TypeModifier::Pointer);
		state.NamedValues.InsertValue(std::string(name), alloyValue);

		return alloyValue;
	}

	llvm::Function* generateFunctionDeclaration(ModuleTable& moduleTable, LLVMState& state,
		AlloyType* parentType,
		const FUNCTION_DEFINITION& functionDeclarationNode, const std::string& moduleName,
		const std::vector<TYPE*>& functionArguments,	// generic arguments, if any
		const std::vector<AlloyValue>& argumentValues,			// if an argument is of type Any, then get the type from the argument values
		GenericTypeMap& typeMap		// map from the generic type to the actual function parameter type
	)
	{
		//
		// If type is not nullptr, we are generating a member function in the form of Type@Name
		// If the parameter list contains types (generic functions), also add the type names and function arguments to the mangled name
		//
		buildGenericTypeMap(moduleTable, state,
			functionDeclarationNode.pFunctionType->GenericParameters,
			functionArguments,
			typeMap
			);
		std::vector<VARIABLE_DECLARATION*> empty;
		std::string name = getExtendedFunctionName(
			moduleTable, state,
			((parentType != nullptr) ? "" : moduleName),	// when parentType is provided, GetTypeName will already include the module name so don't repeat it here
			((parentType != nullptr) ? state.NamedValues.GetTypeName(parentType) : ""),
			functionDeclarationNode.pFunctionNameToken->Value,
			functionDeclarationNode.pFunctionType->GenericParameters,
			functionArguments,
			functionDeclarationNode.pBody ? functionDeclarationNode.pFunctionType->Parameters : empty,	// do not mangle external function definitions
			argumentValues,
			{},
			typeMap
		);

#ifdef TRACE_CODE_GENERATOR
		if (!moduleName.empty()) {
			logInfoAtCurrentPosition(moduleTable, functionDeclarationNode.pFunctionNameToken,
				"Processing function {0}\n", name);
		}
#endif

		if (name.empty()) {
			logErrorAtCurrentPosition(moduleTable, functionDeclarationNode.pFunctionNameToken, "Function '{0}' invalid declaration!", name);
			return nullptr;
		}

		// check if function already exists
		if (state.Module->getFunction(name))
		{
			// Replace '@' in mangled name by ':' for error messages
			std::string ename = name;
			std::replace(ename.begin(), ename.end(), '@', ':');
			logErrorAtCurrentPosition(moduleTable, functionDeclarationNode.pFunctionNameToken, "Function '{0}' already defined!", ename);
			return nullptr;
		}

		// retrieve the parameter types
		std::vector<llvm::Type*> paramTypes;
		std::vector<AlloyType*> paramSubTypes;
		std::vector<TypeModifier> paramModifiers;
		std::vector< VariableType> paramVarTypes;

		int argi = 0;
		for (VARIABLE_DECLARATION* pParameterVariableDeclaration : functionDeclarationNode.pFunctionType->Parameters)
		{
			TypeModifier modifier = pParameterVariableDeclaration->pType->Modifier;
			AlloyType* identifierType = nullptr;

			// if argument is of type Any, obtain the type from the argument value
			if (pParameterVariableDeclaration->isAny) {
				identifierType = argumentValues[argi].Type;
			}
			else {
				Location location(0, 0, 0);
				Token tok{ ((parentType != nullptr) ? parentType->name() : ""), location, TokenKind::string_literal};
				identifierType = generateTypeIdentifier(moduleTable, state, *pParameterVariableDeclaration->pType,
					((parentType != nullptr) ? &tok : nullptr),
					typeMap);
			}

			if (!identifierType)
			{
				logErrorAtCurrentPosition(moduleTable, pParameterVariableDeclaration->pNameToken, "Function '{0}' parameter type error!", name);
				return nullptr;
			}

			// References should be passed as pointers
			if (!pParameterVariableDeclaration->isAny		// parameters of type Any will already have the type set as pointer type
				&& (modifier == TypeModifier::Reference || modifier == TypeModifier::Pointer))
			{
				paramTypes.push_back(AlloyType::getPointerType(identifierType)->llvmType);
				paramSubTypes.push_back(identifierType);
			}
			else
			{
				paramTypes.push_back(identifierType->llvmType);
				paramSubTypes.push_back(identifierType->containedType);
			}
			paramModifiers.push_back(modifier);
			paramVarTypes.push_back(pParameterVariableDeclaration->VarType);
			argi++;
		}

		// retrieve the return types
		AlloyType* returnType = AlloyType::get(llvm::Type::getVoidTy(*state.Context));

		if (functionDeclarationNode.pFunctionType != nullptr
			&& functionDeclarationNode.pFunctionType->pReturnType != nullptr)
		{
			returnType = generateTypeDeclaration(moduleTable, state, *functionDeclarationNode.pFunctionType);
		}

		if (!returnType)
		{
			logErrorAtCurrentPosition(moduleTable, functionDeclarationNode.pFunctionNameToken, "Function '{0}' return type error!", name);
			return nullptr;
		}

		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType->llvmType, paramTypes, functionDeclarationNode.pFunctionType->IsVarArg);

		//
		// This call creates a crash when later executing the code using the JIT engine: 
		// llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, name, state.Module.get());
		// There is a TODO in the source-code of llvm that might explain that: 
		// TODO: remove this once all users have been updated to pass an AddrSpace
		// So when using Module.get() to pass a pointer rather than a reference (*Module) we should also pass 0 as address space
		//
		llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, name, *state.Module);

		// set names for all arguments
		// i is the index in the function parameters list
		// arg is the index of the argument of the llvm function
		for (size_t i = 0; i < functionDeclarationNode.pFunctionType->Parameters.size(); i++)
		{
			const std::string_view paramName = functionDeclarationNode.pFunctionType->Parameters[i]->pNameToken->Value;

			function->getArg(i)->setName(paramName);

			// set the ByRef attribute on parameters passed byref
			if (paramModifiers[i] == TypeModifier::Reference)
			{
				function->addAttributeAtIndex(i + 1, llvm::Attribute::getWithByRefType(*state.Context, paramSubTypes[i]->llvmType));
			}

			/* ReadOnly is not the same as const and LLVM does not accept ReadOnly on anything other than pointers
			// set the ReadOnly attribute on parameters passed as const
			if (paramVarTypes[arg] == VariableType::Constant)
			{
				function->addAttributeAtIndex(arg+1, llvm::Attribute::get(*state.Context, llvm::Attribute::AttrKind::ReadOnly));
			}
			*/
		}

		return function;
	}

#pragma endregion

#pragma region Expressions

	AlloyValue generateConstructorExpression(ModuleTable& moduleTable, LLVMState& state, const CONSTRUCTOR& constructorExpression)
	{
		// search for the struct definition in the module table
		std::string structName;
		GenericTypeMap genericTypeMap = state.NamedValues.GetGenericTypeMap();

		GenericArgumentTypes genericArgumentTypes = ProcessGenericArguments(moduleTable, state, constructorExpression.pType->GenericArguments, genericTypeMap);
		AlloyType* type = getTypeFromTypeName(moduleTable, state, constructorExpression.pType->pNameToken,
			genericArgumentTypes,
			genericTypeMap);

#ifdef TRACE_CODE_GENERATOR
		logInfoAtCurrentPosition(moduleTable, constructorExpression.pType->pNameToken,
			"Processing constructor {0}\n", GetMangledName(state, moduleTable.GetCurrentContext(), constructorExpression.pType->pNameToken->Value,
				genericArgumentTypes,
				genericTypeMap));
#endif
		if (!type || type->getTypeID() != llvm::Type::StructTyID)
		{
			SearchResult<TYPE_DEFINITION> result = moduleTable.GetTypeDefinition(constructorExpression.pType->pNameToken->Value);

			// create mangled structure name (if needed)
			structName = GetMangledName(state, "", result.MangledName,
				genericArgumentTypes,
				genericTypeMap);

			logErrorAtCurrentPosition(moduleTable, constructorExpression.pType->pNameToken,
				"Unknown struct type '{0}'!", structName);
			return AlloyValue();
		}

		llvm::StructType* structType = static_cast<llvm::StructType*>(type->llvmType);
		structName = std::string(state.NamedValues.GetTypeName(type));

		// create a mutable variable at the end of the insertion block
		llvm::IRBuilder<> tempBuilder(state.Builder->GetInsertBlock(), state.Builder->GetInsertBlock()->end());
		llvm::AllocaInst* structPtr = tempBuilder.CreateAlloca(structType, nullptr, constructorExpression.pType->pNameToken->Value);

		/* reset memory to 0
		* this call crashes the JIT compiler but only after optimizations are applied
		* the call can be removed as structures are made packed (byte aligned)
		state.Builder->CreateMemSetInline(
			structPtr,
			llvm::MaybeAlign(1),
			llvm::ConstantInt::get(*state.Context, llvm::APInt(8, 0)),
			llvm::ConstantInt::get(*state.Context, llvm::APInt(64, state.Module->getDataLayout().getTypeAllocSize(type)))
		);
		*/

		// go through the list of initializers and initialize all members
		for (int i = 0; i < constructorExpression.Arguments.size(); i++)
		{
			// get the name of the member to initialize
			const std::string_view memberName = constructorExpression.Arguments[i].first->Value;

			NamedValues::TypeMemberInfo memberInfo = state.NamedValues.GetStructMemberInfo(structName, memberName);

			if (memberInfo.memberIndex == -1)
			{
				logErrorAtCurrentPosition(moduleTable, constructorExpression.pType->pNameToken,
					"Type '{0}' is not a struct!", structName);
				return AlloyValue();
			}

			if (memberInfo.memberIndex == -2)
			{
				logErrorAtCurrentPosition(moduleTable, constructorExpression.pType->pNameToken,
					"Struct '{0}' does not have a member '{1}'!", structName, memberName);
				return AlloyValue();
			}

			// set the type that we are expecting for each structure element
			AlloyType* memberType = memberInfo.Type;
			AlloyValue expressionVal = generateExpression(moduleTable, state, *constructorExpression.Arguments[i].second, memberType);

			if (!expressionVal.isValid())
			{
				logErrorAtCurrentPosition(moduleTable, nullptr, // nodeID
					"Error evaluating '{0}.{1}'!", structName, memberName);
				return AlloyValue();
			}

			if (llvm::isa<llvm::PointerType>(memberType->llvmType)) {
				// TBD: check if special processing is needed in case of pointers (e.g. strings?)
			}

			llvm::Value* memberPtr = state.Builder->CreateStructGEP(structType, structPtr, memberInfo.memberIndex, memberName);

			AlloyValue::convertValueToType(state, expressionVal, memberType);

			state.Builder->CreateStore(expressionVal.Value, memberPtr, "savetmp");
		}

		// result contains the whole initialized structure
		return AlloyValue(state.Builder->CreateLoad(structPtr->getAllocatedType(), structPtr), AlloyType::get(structPtr->getAllocatedType()), structPtr);
	}

	AlloyValue generatePointerInitializerExpression(ModuleTable& moduleTable, LLVMState& state, const POINTER_INIT& pointerInitializerNode,
		AlloyType* identifierType)
	{
		//
		// new ( EXPRESSION ) ;
		//
		bool isArray;
		AlloyType* elementType = SmartPointerClass::isSmartPointer(state, identifierType, isArray);
		if (elementType == nullptr)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD *pointerInitializerNode... 
				"Pointer type required!");
			return AlloyValue();
		}

		// get the value to set for each element
		AlloyType* tempType = elementType;
		AlloyValue defaultValue = generateExpression(moduleTable, state, *pointerInitializerNode.pValue, tempType);
		llvm::AllocaInst* alloc = nullptr;
		llvm::Value* count;
		if (defaultValue.Ptr
			&& llvm::isa<llvm::AllocaInst>(defaultValue.Ptr)
			)
		{
			llvm::AllocaInst* alloca = static_cast<llvm::AllocaInst*>(defaultValue.Ptr);
			// get the number of allocated elements in case of an array
			count = state.Builder->CreateIntCast(alloca->getArraySize(), AlloyType::get("i64")->llvmType, true);
		}
		else
		{
			// creating a single object
			count = llvm::ConstantInt::get(*state.Context, llvm::APInt(64, 1, true));
		}

		// create a mutable variable on the heap
		AlloyType* PointerType = AlloyType::get("i64");	// pointers are 64-bit values
		llvm::Constant* AllocSize = llvm::ConstantExpr::getSizeOf(elementType->llvmType);
		AllocSize = llvm::ConstantExpr::getTruncOrBitCast(AllocSize, PointerType->llvmType);
		count = state.Builder->CreateMul(AllocSize, count);
		llvm::CallInst* Ptr = state.Builder->CreateMalloc(PointerType->llvmType, elementType->llvmType, count, nullptr, nullptr, "ptrinit");

		if (alloc) {
			// if the value is of type AlloaInst, copy the data using memcpy
			state.Builder->CreateMemCpy(Ptr, llvm::MaybeAlign(1), alloc, llvm::MaybeAlign(1), count);
		}
		else {
			state.Builder->CreateStore(defaultValue.Value, Ptr);
		}

		// create the smart pointer structure containing all the pointer information and return it to caller
		return SmartPointerClass::create(state, identifierType, Ptr, count);
	}

	AlloyValue generateArrayInitializerExpression(ModuleTable& moduleTable, LLVMState& state, const ARRAY_INIT& arrayInitializerNode,
		AlloyType* expectedType)
	{
		//
		// ( [ EXPRESSION; EXPRESSION ] ) ;
		//
		// Array initialization in the form of: [ VALUE, COUNT ]
		//

		// get the value to set for each element
		bool isArray;
		AlloyType* iteratorType = AlloyType::get("i32");	// integer type used to iterate through all elements
		AlloyType* elementType = SmartPointerClass::isSmartPointer(state, expectedType, isArray);
		AlloyType* temp = elementType;
		AlloyValue defaultValue = generateExpression(moduleTable, state, *arrayInitializerNode.pValue, temp);

		// now get the size of the vector to create
		temp = iteratorType;
		AlloyValue count = generateExpression(moduleTable, state, *arrayInitializerNode.pCount, temp);
		AlloyValue::convertValueToType(state, count, iteratorType);

		// create code to go through the list of members and initialize them to the given value
		// we need to create the loop within the llvm code because we don't know beforehand the size of the array
		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();
		ASSERT(func != nullptr, "No function to insert into!");

		llvm::IRBuilder<>& tempBuilder = *state.Builder;

		// create a mutable variable at the end of the insertion block
		llvm::AllocaInst* arrayPtr = tempBuilder.CreateAlloca(elementType->llvmType, count.Value, "array_init_temparray");

		// emit init code before the loop
		// start is the loop variable, initialize it to 0
		llvm::AllocaInst* start = tempBuilder.CreateAlloca(iteratorType->llvmType, nullptr, "array_init_start");
		tempBuilder.CreateStore(llvm::ConstantInt::get(iteratorType->llvmType, 0), start);
		llvm::Value* step = llvm::ConstantInt::get(iteratorType->llvmType, 1);

		// create a new basic block to start insertion into
		llvm::BasicBlock* loopBlock = llvm::BasicBlock::Create(*state.Context, "array_init_loop", func);
		// insert an explicit fall through from the current block to the loopBlock
		tempBuilder.CreateBr(loopBlock);

		// start insertion into the loopBlock
		tempBuilder.SetInsertPoint(loopBlock);

		// generate the body of the loop

		// create the instructions to store the value for each element
		llvm::Value* current = tempBuilder.CreateLoad(start->getAllocatedType(), start);
		llvm::Value* memberPtr = tempBuilder.CreateGEP(elementType->llvmType, arrayPtr, current, "array_init_gep");
		tempBuilder.CreateStore(defaultValue.Value, memberPtr);

		// increment loop variable (start) by step value
		current = tempBuilder.CreateAdd(current, step, "array_init_inc");
		tempBuilder.CreateStore(current, start);

		// emit the condition
		llvm::Value* conditionVal = tempBuilder.CreateICmpUGE(current, count.Value);
		conditionVal = convertToBool(state, conditionVal);

		// create the "after loop" block and insert it
		llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(*state.Context, "array_init_afterloop", func);

		// insert the conditional branch into the end of afterBlock
		tempBuilder.CreateCondBr(conditionVal, afterBlock, loopBlock);

		// any new code will be inserted in afterBlock
		tempBuilder.SetInsertPoint(afterBlock);

		return SmartPointerClass::create(state, expectedType, arrayPtr, count.Value); // result contains the whole initialized array as a smart pointer
	}

	AlloyValue generatePointerMoveExpression(ModuleTable& moduleTable, LLVMState& state, const POINTER_MOVE& pointerMoveNode,
		AlloyType*& expectedType)
	{
		//
		// move Identifier ;
		//
		ASSERT(pointerMoveNode.pVariable->Is<VARIABLE>(), "TODO: right side of pointer move ca now be any variable to support member/array accesses!");
		VARIABLE* pVariable = pointerMoveNode.pVariable->Get<VARIABLE>();

		AlloyValue ptrValue = generateIdentifier(moduleTable, state, *pVariable, expectedType);

		// retrieve the name of the identifier in the right-side node in order to remove it from the NamedValues map
		const Token& rightNode = *pVariable->pNameToken;
		const std::string name(rightNode.Value);
		state.NamedValues.RemoveValue(name);

		return ptrValue;
	}

	AlloyValue generateInitializerListExpression(ModuleTable& moduleTable, LLVMState& state, const INITIALIZER_LIST& initListExpressionNode,
		AlloyType*& expectedType)
	{
		//
		// Array initialization in the form of: { EXPRESSION, EXPRESSION, ... }
		// Arrays can be represented by either an llvm Vector or an llvm Pointer. Both cases are handled in this function
		//
		bool isArray;
		AlloyType* elementType = SmartPointerClass::isSmartPointer(state, expectedType, isArray);
		if (!expectedType ||
			(!elementType			// arrays are created as smart pointers which are structures
#if 0
				&& expectedType.type->getTypeID() != AlloyType::FixedVectorTyID
				&& expectedType.type->getTypeID() != AlloyType::ScalableVectorTyID
				&& expectedType.type->getTypeID() != AlloyType::PointerTyID
#endif
				)
			)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: initListExpressionNode...
				"Unknown vector type!");
			return AlloyValue();
		}

		llvm::Value* count = llvm::ConstantInt::get(*state.Context, llvm::APInt(64, initListExpressionNode.Values.size(), true));
		// create a mutable variable at the end of the insertion block
		llvm::IRBuilder<> tempBuilder(state.Builder->GetInsertBlock(), state.Builder->GetInsertBlock()->end());
		llvm::AllocaInst* arrayPtr = tempBuilder.CreateAlloca(elementType->llvmType, count, "initlist");

		// go through the list of initializers and initialize all members
		for (int i = 0; i < initListExpressionNode.Values.size(); i++)
		{
			// set the type that we are expecting for each array element
			AlloyType* identifierType = elementType;

			AlloyValue expressionVal = generateExpression(moduleTable, state, *initListExpressionNode.Values[i], identifierType);

			if (!expressionVal.isValid())
			{
				logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: initListExpressionNode.Values[i]...
					"Error evaluating expression!");
				return AlloyValue();
			}

			// set the value of this element into the array
			llvm::Value* memberPtr = state.Builder->CreateGEP(elementType->llvmType, arrayPtr, 
				AlloyValue::getConstantInt(*state.Context, 32, i));
			state.Builder->CreateStore(expressionVal.Value, memberPtr);
		}

		return SmartPointerClass::create(state, expectedType, arrayPtr, count);
	}

	AlloyValue generateVariableDefinitionExpression(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DEFINITION& variableDefinition)
	{
		//
		// ( var | const ) identifier:TYPE = expression;
		//
		AlloyType* identifierType = nullptr;
		AlloyValue declarationVal;
		AlloyValue result;

#ifdef TRACE_CODE_GENERATOR
		logInfoAtCurrentPosition(moduleTable, variableDefinition.pDeclaration->pNameToken,
			"Processing variable definition for {0}\n", variableDefinition.pDeclaration->pNameToken->Value);
#endif
		if (nullptr == variableDefinition.pDeclaration->pType)
		{
			// case where the type is not given but has to be inferred from the type of the expression

			// create the value expression
			result = generateExpression(moduleTable, state, *variableDefinition.pValue, identifierType);

			if (result.isValid())
			{
				// create the declaration of a given type
				identifierType = result.Type;
				declarationVal = generateVariableDeclaration(moduleTable, state, *variableDefinition.pDeclaration, identifierType);
			}
		}
		else
		{
			// case where the type of the variable is given, the expression should return a value of compatible type

			// create the declaration
			declarationVal = generateVariableDeclaration(moduleTable, state, *variableDefinition.pDeclaration, identifierType);

			if (declarationVal.isValid())
			{
				// create the value expression
				result = generateExpression(moduleTable, state, *variableDefinition.pValue, identifierType);

				if (result.isValid())
				{
					// try to convert the value to the right variable type
					AlloyValue::convertValueToType(state, result.Value, declarationVal.Type);
				}
			}
		}

		if (!result.isValid() || !declarationVal.isValid())
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD 
				"Error evaluating expression!");
			return AlloyValue();
		}
		else
		{
			// store the value into the alloca
			state.Builder->CreateStore(result.Value, declarationVal.Ptr);
			result.Ptr = declarationVal.Ptr;
			// update the value in the NamedValues map
			state.NamedValues.UpdateValue(std::string(variableDefinition.pDeclaration->pNameToken->Value), result);
			return result;
		}
	}

	AlloyValue generateArrayAccessExpression(ModuleTable& moduleTable, LLVMState& state, const ARRAY_ACCESS& arrayAccessExpression,
		AlloyType*& identifierType)
	{
		llvm::Type* int32Type = llvm::IntegerType::getInt32Ty(*state.Context);
		AlloyType* temp = AlloyType::get(int32Type);
		AlloyValue memberIndex = generateExpression(moduleTable, state, *arrayAccessExpression.pIndex, temp);
		AlloyValue::convertValueToType(state, memberIndex, temp);
		AlloyValue left;
		llvm::Value* memberPtr = nullptr;
		llvm::Value* memberValue = nullptr;
		bool isIdentifier = false;		// set to true if left hand side is an identifier, to false if it is an expression

		// handle identifier
		if (arrayAccessExpression.pArray->Is<PRIMARY>()
			&& arrayAccessExpression.pArray->Get<PRIMARY>()->Is<VARIABLE>()
			)
		{
			left = generateIdentifier(moduleTable, state, *arrayAccessExpression.pArray->Get<PRIMARY>()->Get<VARIABLE>(), identifierType);
			isIdentifier = true;
		}
		else
			// not a variable, must be an expression that returns an array
		{
			left = generateExpression(moduleTable, state, *arrayAccessExpression.pArray, identifierType);
		}

		// get the type of the left
		bool isArray;
		AlloyType* leftType = left.Type;
		AlloyType* elementType = SmartPointerClass::isSmartPointer(state, leftType, isArray);
		if (elementType) {
			if (!isArray) {
				// pointer to an array, load the actual array
				left.Value = SmartPointerClass::getValue(state, left.Value, memberPtr, 0);
				leftType = elementType;
			}
			// We have a pointer or an array represented by a smart pointer structure
			identifierType = elementType = SmartPointerClass::isSmartPointer(state, leftType, isArray);
			memberValue = SmartPointerClass::getValue(state, left.Value, memberPtr, memberIndex.Value);
		}
		else {
#if 0	// code is kept here in case we go back to using llvm vectors for arrays
			if (leftType->getTypeID() == AlloyType::FixedVectorTyID || leftType->getTypeID() == AlloyType::ScalableVectorTyID) {

				llvm::VectorType* vectorType = static_cast<llvm::VectorType*>(leftType);

				if (isIdentifier) {
					// case of an identifier			
					std::vector<llvm::Value*> indices(2);
					indices[0] = llvm::ConstantInt::get(*state.Context, llvm::APInt(32, 0, true));
					indices[1] = memberIndex;

					memberPtr = state.Builder->CreateGEP(vectorType, left.Ptr, indices, "memberptr");
					memberValue = state.Builder->CreateLoad(vectorType->getElementType(), memberPtr);
				}
				else {
					// case of an expression, we don't have a pointer to the array
					memberValue = state.Builder->CreateExtractElement(left.Value, memberIndex, "extractelem");
				}
			}
			else if (left.Ptr != nullptr) {
				// case of a pointer			
				memberPtr = state.Builder->CreateGEP(identifierType.type, left.Ptr, memberIndex, "memberptr");
				memberValue = state.Builder->CreateLoad(identifierType.type, memberPtr);
			}
			else if (leftType->getTypeID() == AlloyType::PointerTyID
				&& identifierType.containedType != nullptr) {
				// case where the value contains a pointer			
				memberPtr = state.Builder->CreateGEP(identifierType.containedType, left.Value, memberIndex, "memberptr");
				memberValue = state.Builder->CreateLoad(identifierType.containedType, memberPtr);
			}
			else
#endif
			{
				logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: arrayAccessExpression.ArrayExpressionID
					"ArrayAccessExpression: Left should be a smart pointer array!");
				return {};
			}
		}

		{
			// array of pointers, we need to load the underlying value
			AlloyType* elementType2 = SmartPointerClass::isSmartPointer(state, AlloyType::get(memberValue->getType()), isArray);
			if (elementType2 != nullptr) {
				llvm::Value* Ptr = nullptr;
				llvm::Value* Value = SmartPointerClass::getValue(state, memberValue, Ptr, 0);
				identifierType = elementType2;
				return AlloyValue(Value, elementType2, Ptr, left.isConst);
			}
		}

		return AlloyValue(memberValue, elementType, static_cast<llvm::AllocaInst*>(memberPtr), left.isConst);
	}

	AlloyValue generateMemberAccessExpression(ModuleTable& moduleTable, LLVMState& state, const MEMBER_ACCESS& memberAccessExpression,
		AlloyType*& expectedType)
	{
		const std::string_view memberName = memberAccessExpression.pMemberNameToken->Value;

		AlloyValue left;

		// handle nested member access
		if (memberAccessExpression.pObject->Is<POSTFIX>()
			&& memberAccessExpression.pObject->Get<POSTFIX>()->Is<MEMBER_ACCESS>()
			)
		{
			left = generateMemberAccessExpression(moduleTable, state, *memberAccessExpression.pObject->Get<POSTFIX>()->Get<MEMBER_ACCESS>(), expectedType);
		}
		// handle identifier
		else if (memberAccessExpression.pObject->Is<PRIMARY>()
			&& memberAccessExpression.pObject->Get<PRIMARY>()->Is<VARIABLE>())
		{
			left = generateIdentifier(moduleTable, state, *memberAccessExpression.pObject->Get<PRIMARY>()->Get<VARIABLE>(), expectedType);
		}
		else
			// neither an identifier nor a nested member access, must be an expression that returns a structure
		{
			left = generateExpression(moduleTable, state, *memberAccessExpression.pObject, expectedType);
		}

		// get the type of the left
		bool isArray;
		AlloyType* leftType = left.Type;
		AlloyType* elementType = SmartPointerClass::isSmartPointer(state, leftType, isArray);
		llvm::Value* memberPtr = nullptr;
		if (elementType) {
			// case of a smart pointer, load the actual structure
			left.Value = SmartPointerClass::getValue(state, left.Value, left.Ptr, 0);
			leftType = elementType;
		}

		if (leftType->getTypeID() != llvm::Type::StructTyID)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: moduleTable.GetErrorInfo(memberAccessExpressionNode.LeftID)
				"Expected struct type!");
			return {};
		}

		llvm::StructType* structType = reinterpret_cast<llvm::StructType*>(leftType->llvmType);

		// get the name of the struct type
		const std::string_view structName = leftType->name();

		// get the index of the member
		NamedValues::TypeMemberInfo memberInfo = state.NamedValues.GetStructMemberInfo(structName, memberName);

		if (memberInfo.memberIndex == -1)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: memberAccessExpressionNode.RightID
				"Type '{0}' is not a struct type!", structName);
			return {};
		}

		if (memberInfo.memberIndex == -2)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: memberAccessExpressionNode.RightID
				"Struct '{0}' does not have a member '{1}'!", structName, memberName);
			return {};
		}

		if (left.Ptr == nullptr) {
			// case where the structure is returned by evaluating an expression, we can directly access the structure member
			std::vector<unsigned int> indices(1);
			indices[0] = memberInfo.memberIndex;
			llvm::Value* memberValue = state.Builder->CreateExtractValue(left.Value, indices);
			left = AlloyValue(memberValue, memberInfo.Type, nullptr, left.isConst);
		}
		else {
			llvm::Value* memberPtr = state.Builder->CreateStructGEP(structType, left.Ptr, memberInfo.memberIndex, memberName);
			llvm::Value* memberValue = state.Builder->CreateLoad(memberInfo.Type->llvmType, memberPtr, "loadtmp");
			left = AlloyValue(memberValue, memberInfo.Type, memberPtr, left.isConst);
		}

		// return the type and subtype of the structure element to the caller
		expectedType = memberInfo.Type;

		return left;
	}

	AlloyValue generateEnumValue(ModuleTable& moduleTable, LLVMState& state, const ENUM_VALUE& enumValueExpression,
		AlloyType*& expectedType)
	{
		//
		// Given an expression such as EnumType|ValueA(43), this function will return:
		//		- The type of the enum structure in expectedType.parentType
		//		- The type of the enum value in expectedType.type (structure of type EnumPayloadStruct)
		//		- The index of the enum value in the Value member of PtrValuePair as a constant int
		//		- A pointer to the EnumPayloadStruct in the Ptr member of PtrValuePair
		//		- The type of the payload in containedType
		//
		const std::string_view memberName = enumValueExpression.pEnumValueNameToken->Value;
		std::string_view enumName = enumValueExpression.pEnumName->pNameToken->Value;
		NamedValues::TypeMemberInfo memberInfo;
		int memberIndex = -1;
		AlloyValue result;
		llvm::Value* memberPtr = nullptr;
		AlloyType* EnumPayloadStruct = nullptr;

		// generate or retrieve the llvm type for this enumeration
		AlloyType* enumType = getTypeFromTypeName(moduleTable, state, enumValueExpression.pEnumName->pNameToken,
			ProcessGenericArguments(moduleTable, state, enumValueExpression.pEnumName->GenericArguments, {}), {});

		if (nullptr == enumType) {
			logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumName->pNameToken, "Type {0} not found!", enumValueExpression.pEnumName->pNameToken->Value);
			goto failed;
		}

		// get the mangled name
		enumName = enumType->name();

		memberInfo = state.NamedValues.GetEnumMemberInfo(enumName, memberName);
		if (memberInfo.memberIndex == -1)
		{
			logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumName->pNameToken,
				"Type '{0}' is not an enum!", enumName);
			goto failed;
		}

		if (memberInfo.memberIndex == -2)
		{
			logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumName->pNameToken,
				"Enum '{0}' does not have a member '{1}'!", enumName, memberName);
			goto failed;
		}

		if (nullptr == memberInfo.Type && enumValueExpression.pPayloadValue != nullptr) {
			logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumName->pNameToken,
				"Payload specified for enum member '{0}' when member cannot have a payload!", memberName);
			goto failed;
		}

		// create an alloca to a structure of type EnumPayloadStruct
		// and fill the structure with the enum index and payload value if any
		EnumPayloadStruct = state.NamedValues.GetEnumPayloadStruct(*state.Context, memberInfo.Type);
		result.Ptr = createEntryBlockAlloca(
			state.Builder->GetInsertBlock()->getParent(),
			"",
			EnumPayloadStruct
		);
		/* reset memory to 0
		* this call crashes the JIT compiler but only after optimizations are applied
		* the call can be removed as structures are made packed (byte aligned)
		state.Builder->CreateMemSetInline(
			result.Ptr,
			llvm::MaybeAlign(1),
			llvm::ConstantInt::get(*state.Context, llvm::APInt(8, 0)),
			llvm::ConstantInt::get(*state.Context, llvm::APInt(64, state.Module->getDataLayout().getTypeAllocSize(type)))
		);
		*/

		// set the types that we are expecting in return
		expectedType = EnumPayloadStruct;
		expectedType->containedType = memberInfo.Type;	// this is the type of payload, can be null
		result.parentType = enumType;

		// store the index of the enum value in the first element of EnumPayloadStruct
		memberPtr = state.Builder->CreateStructGEP(EnumPayloadStruct->llvmType, result.Ptr, EnumPayloadIndex);
		state.Builder->CreateStore(llvm::ConstantInt::get(*state.Context, llvm::APInt(64, memberInfo.memberIndex)), memberPtr, "savepayload");

		if (enumValueExpression.pPayloadValue != nullptr) {
			// compute the value of the payload
			AlloyType* expressionType = expectedType->containedType;
			llvm::Value* expressionVal = generateExpression(moduleTable, state, *enumValueExpression.pPayloadValue, expressionType).Value;

			if (!expressionVal)
			{
				logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumValueNameToken,
					"Error evaluating '{0}.{1}'!", enumName, memberName);
				goto failed;
			}

			// store the payload in the second element of EnumPayloadStruct
			memberPtr = state.Builder->CreateStructGEP(EnumPayloadStruct->llvmType, result.Ptr, EnumPayloadValue);
			state.Builder->CreateStore(expressionVal, memberPtr, "savepayload");
		}
		else {
			// store a null value for the payload in the second element of EnumPayloadStruct
			memberPtr = state.Builder->CreateStructGEP(EnumPayloadStruct->llvmType, result.Ptr, EnumPayloadValue);
			state.Builder->CreateStore(llvm::Constant::getNullValue(AlloyType::get("i64")->llvmType), memberPtr, "savepayload");
		}

		// store the structure type ID in the third element of EnumPayloadStruct
		memberPtr = state.Builder->CreateStructGEP(EnumPayloadStruct->llvmType, result.Ptr, EnumPayloadEnumID);
		state.Builder->CreateStore(llvm::ConstantInt::get(*state.Context, llvm::APInt(64, state.NamedValues.GetID(expectedType))), memberPtr, "savepayload");

		result.Value = state.Builder->CreateLoad(EnumPayloadStruct->llvmType, result.Ptr, "loadpayload");
		result.Type = expectedType;

	failed:
		return result;
	}

	AlloyValue generateEnumValueIndex(ModuleTable& moduleTable, LLVMState& state, const ENUM_VALUE& enumValueExpression,
		AlloyType*& expectedType)
	{
		//
		// This function is equivalent to generateEnumValue except that it returns the index of the enum value and not the payload structure
		//
		const std::string_view memberName = enumValueExpression.pEnumValueNameToken->Value;
		std::string_view enumName = enumValueExpression.pEnumName->pNameToken->Value;
		NamedValues::TypeMemberInfo memberInfo;
		int memberIndex = -1;
		AlloyValue result;
		llvm::Value* memberPtr = nullptr;

		// generate or retrieve the llvm type for this enumeration
		AlloyType* enumType = getTypeFromTypeName(moduleTable, state, enumValueExpression.pEnumName->pNameToken,
			ProcessGenericArguments(moduleTable, state, enumValueExpression.pEnumName->GenericArguments, {}), {});

		if (nullptr == enumType) {
			logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumName->pNameToken, "Type {0} not found!", enumValueExpression.pEnumName->pNameToken->Value);
			goto failed;
		}

		// get the mangled name
		enumName = enumType->name();

		memberInfo = state.NamedValues.GetEnumMemberInfo(enumName, memberName);
		if (memberInfo.memberIndex == -1)
		{
			logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumName->pNameToken,
				"Type '{0}' is not an enum!", enumName);
			goto failed;
		}

		if (memberInfo.memberIndex == -2)
		{
			logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumName->pNameToken,
				"Enum '{0}' does not have a member '{1}'!", enumName, memberName);
			goto failed;
		}

		// set the types that we are expecting in return
		expectedType = AlloyType::get("i64");		// the index is returned in 64-bit int format
		expectedType->containedType = memberInfo.Type;	// this is the type of payload, can be null
		result.parentType = enumType;
		result.Value = llvm::ConstantInt::get(*state.Context, llvm::APInt(64, memberInfo.memberIndex));

	failed:
		return result;
	}

	bool evaluateFunctionArguments(ModuleTable& moduleTable, LLVMState& state,
		bool insertSelfAsFirstParam,
		FUNCTION_TYPE* pCalleeFunctionType, std::vector<EXPRESSION*> Arguments,
		const GenericTypeMap& typeMap,
		std::vector<AlloyValue>& argumentValues
	)
	{
		//
		// Evaluate all the arguments of a function call
		// returns false if an error occurs during evaluation, true otherwise
		// the values of the arguments are returned in the argumentValues vector
		//

		bool result = false;
		int startIndex = (insertSelfAsFirstParam ? 1 : 0);	// if the first parameter is Self, this will not be in the arguments list so we have to skip one parameter

		for (size_t argi = startIndex; argi < Arguments.size(); argi++)
		{
			const EXPRESSION& argument = *Arguments[argi];

			AlloyValue argVal;
			TypeModifier modifier;

			// retrieve the expected parameter type
			AlloyType* argType = getFunctionParameterType(moduleTable, state, *pCalleeFunctionType, typeMap, modifier, argi);

			// check if the function parameter was declared as const
			bool isConst = isFunctionParameterConst(*pCalleeFunctionType, argi);

			// check if the parameter is passed byref, in which case it should be a variable and we will pass the address of the identifier
			if (modifier == TypeModifier::Reference) {
				bool foundVariable = false;

				if (argument.Is<UNARY>()) {
					const UNARY& unary = *argument.Get<UNARY>();
					std::string_view operatorStr = unary.pOpToken->Value;

					if (operatorStr == "&"
						&& unary.pExpression->Is<PRIMARY>()
						&& unary.pExpression->Get<PRIMARY>()->Is<VARIABLE>()) {
						AlloyType* tempType = nullptr;
						AlloyValue left = generateIdentifier(moduleTable, state, *unary.pExpression->Get<PRIMARY>()->Get<VARIABLE>(),
							tempType);
						if (left.Ptr == nullptr) {
							logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: argumentID
								"Function argument {0} expects a reference to a variable preceded by the & symbol!", argi + 1);
							goto error;
						}
						// check that we are passing a reference to a variable of the right type
						if (tempType != argType->containedType
							&& !pCalleeFunctionType->Parameters[argi]->isAny	// contained type is null if the parameter is if type Any
							) {
							logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: argumentID
								"Function argument {0} expects a reference to a variable of type!", argi + 1, argType->containedType->name());
							goto error;
						}
						argVal.Value = left.Ptr;
						argVal.Ptr = nullptr;
						argVal.Type = argType = AlloyType::getPointerType(tempType);
						argVal.isConst = isConst;
						foundVariable = true;
					}
				}

				// argument is not in the form of &variable, if we are expecting a constant, continue evaluating the expression
				if (!foundVariable) {
					if (!isConst) {
						logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: argumentID
							"Function argument {0} expects a reference to a variable preceded by the & symbol!", argi + 1);
						goto error;
					}

					argVal = generateExpression(moduleTable, state, argument, argType);
					// create a pointer to the evaluated expression and pass the pointer as argument
					llvm::AllocaInst* ptr = state.Builder->CreateAlloca(argVal.Type->llvmType, nullptr,
						(argi < pCalleeFunctionType->Parameters.size() ? pCalleeFunctionType->Parameters[argi]->pNameToken->Value : "argname")
					);
					state.Builder->CreateStore(argVal.Value, ptr);
					argVal.Ptr = ptr;
				}
			}
			else {
				AlloyType* expressionType = nullptr;
				argVal = generateExpression(moduleTable, state, argument, expressionType);

				if (!argVal.isValid())
				{
					logErrorAtCurrentPosition(moduleTable, nullptr, // nodeID
						"Error evaluating expression!");
					goto error;
				}

				if (argType != nullptr) {
					// convert the expression to expected type, this will also load the value pointed to by a pointer or smart pointer
					if (argType) {
						AlloyValue::convertValueToType(state, argVal, argType);
					}

					if (argVal.Type->llvmType != argType->llvmType)
					{
						logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: argumentID
							"Function argument {0} expects value of type '{1}' but given type is '{2}'!", argi + 1,
							argType->name(),
							argVal.Type->name()
						);
						goto error;
					}
				}
				else {
					// calling a function with variable number of parameters, e.g. printf (const str : String, ...)
					// we don't know the expected parameter type and cannot use convertValuetoType to load the value pointed to by a pointer
					// so we have to load the pointed to value ourselves
					bool isArray;
					AlloyType* containedType = SmartPointerClass::isSmartPointer(state, argVal.Type, isArray);
					if (!isArray && containedType != nullptr) {
						// case of smart pointers, load the underlying value and convert to requested type
						llvm::Value* Ptr = SmartPointerClass::get(state, argVal.Value);
						argVal = AlloyValue(state.Builder->CreateLoad(argType->llvmType, Ptr), argType, Ptr, isConst);
					}
				}
			}

			argumentValues.push_back(argVal);
		}
		result = true;

	error:
		return result;

	}

	bool checkFunctionParameterTypes()
	{
#if 0	// TBI

		// for functions with a variable number of arguments, check the argument types till the first optional argument
		// e.g. if the function has 2 mandatory arguments and a number of optional arguments, check for only the first 2 types 
		AlloyType argType = { (argi < calleeFunc->arg_size() ? calleeFunc->getArg(argi)->getType() : nullptr), nullptr };

		// check if the parameter is passed byref, in which case it should be a variable and we will pass the address of the identifier
		auto attr = calleeFunc->getAttributeAtIndex(argi + 1, llvm::Attribute::AttrKind::ByRef);
		bool isByRef = attr.hasAttribute(llvm::Attribute::AttrKind::ByRef);

		// retrieve the actual type for ByRef arguments
		argType.containedType = calleeFunc->getArg(argi)->getParamByRefType();

		if (!calleeFunc->isVarArg() && calleeFunc->arg_size() != argumentValues.size())
		{
			logErrorAtCurrentPosition(moduleTable, functionCallExpressionNode.pFunctionNameToken, "Function '{0}' argument mismatch!", functionName);
			goto error;
		}
#endif
		return true;
	}

	AlloyValue generateFunctionCallExpression(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_CALL& functionCallExpressionNode,
		AlloyType*& expectedType)
	{
		std::string functionName(functionCallExpressionNode.pFunctionNameToken->Value);
		std::string mangledName(functionName);
		std::vector<AlloyValue> argumentValues;	// this vector contains the argument values, types and underlying type for pointers and references
		std::vector<llvm::Value*> argVals;			// this vector contains only the argument values
		AlloyValue result;
		llvm::Function* calleeFunc = nullptr;
		FUNCTION_TYPE* pCalleeFunctionType = nullptr;	// in addition to the LLVM function definition, we need the original function definition in order to properly handle generic and const parameters
		enum { None, Reference, Value } insertSelfAsFirstParam = None;		// indicates whether the first parameter should be the variable value in the case of member function calls
		std::vector<EXPRESSION*> Arguments(functionCallExpressionNode.Arguments);	// creating a copy of the arguments as we might need to insert new elements
		EXPRESSION self;	// this is a fake expression used as a placeholder for Self as first parameter
		SearchResult<FUNCTION_DEFINITION> funcResult;
		AlloyType* parentType = nullptr;	// in the case of member functions, this is the type of the parent variable
		std::string extendedName;
		std::string parentTypeName;			// type name of the parent type if this is a member function
		GenericTypeMap typeMap;

#ifdef TRACE_CODE_GENERATOR
		logInfoAtCurrentPosition(moduleTable, functionCallExpressionNode.pFunctionNameToken,
			"Processing function call {0}\n", mangledName);
#endif

		// handle member function calls
		/*
		In order to differentiate between the different ways of calling a function, the following must be done :
		-check if 'pTypeOrVariableName' of 'FUNCTION_CALL' is 'nullptr', if it is, we have a normal function call
			- if 'pTypeOrVariableName' is the name of a variable which is accessible in this scope, we have a non-static member function call
				- look up the type of the variable
				- find the function named type_name@@func_name
				- check that the first parameter of the function is indeed of type '&Self'
				- pass '&variable_name' as the first parameter and the rest of the arguments as the following parameters
			- if 'pTypeOrVariableName' is the name of a type, we have a static member function call
				- find the function type_name@func_name
				- call it like you would a normal function
		*/

		bool isGenericType = (functionCallExpressionNode.pTypeOrVariableName && functionCallExpressionNode.pTypeOrVariableName->GenericArguments.size() > 0);

		if (functionCallExpressionNode.pObject != nullptr)
		{
			if (functionCallExpressionNode.pObject && functionCallExpressionNode.pObject->Is<VARIABLE>())
			{
				// non-static member function call
				VARIABLE* var = functionCallExpressionNode.pObject->Get<VARIABLE>();
				std::string varOrTypeName(var->pNameToken->Value);
				const AlloyValue* val = state.NamedValues.GetValue(varOrTypeName);
				if (!isGenericType && val) {
					// variable found
					parentType = val->Type;
					parentTypeName = parentType->name();
					// extract any generic parameters from the type name, otherwise we cannot locate the member function
					std::string baseName = parentTypeName;
					size_t pos = parentTypeName.find('@');
					if (pos != std::string::npos) {
						baseName = parentTypeName.substr(0, pos);
					}
					mangledName = NodeBuffer::GetMangledName("", baseName, functionName);
					// insert Self as a first argument
					insertSelfAsFirstParam = Reference;
					Arguments.insert(Arguments.begin(), &self);
				}
				else {
					// not a variable, check if a valid type and make a static function call
					GenericArgumentTypes genericArguments;
					if (functionCallExpressionNode.pTypeOrVariableName)
						genericArguments = ProcessGenericArguments(moduleTable, state, functionCallExpressionNode.pTypeOrVariableName->GenericArguments, typeMap);
					parentType = getTypeFromTypeName(moduleTable, state, var->pNameToken, genericArguments, typeMap);
					if (parentType != nullptr) {
						// static member function call
						parentTypeName = state.NamedValues.GetTypeName(parentType);
						mangledName = NodeBuffer::GetMangledName("", varOrTypeName, functionName);
					}
					else {
						logErrorAtCurrentPosition(moduleTable, var->pNameToken, "'{0}' is not a variable or type name.", varOrTypeName);
						goto error;
					}
				}
			}
		}
		else if (functionCallExpressionNode.pTypeOrVariableName) {
			// this is another way of calling member functions, whether static or not
			std::string varOrTypeName(functionCallExpressionNode.pTypeOrVariableName->pNameToken->Value);
			const AlloyValue* val = state.NamedValues.GetValue(varOrTypeName);
			if (!isGenericType && val)
			{
				// non-static member function call
				parentType = val->parentType;
				parentTypeName = parentType->name();
				std::string baseName = parentTypeName;
				// extract any generic parameters from the type name, otherwise we cannot locate the member function
				size_t pos = parentTypeName.find('@');
				if (pos != std::string::npos) {
					baseName = parentTypeName.substr(0, pos);
				}
				mangledName = NodeBuffer::GetMangledName("", baseName, functionName);
				// insert Self as a first argument
				insertSelfAsFirstParam = Reference;
				Arguments.insert(Arguments.begin(), &self);
			}
			else {
				parentType = getTypeFromTypeName(moduleTable, state, functionCallExpressionNode.pTypeOrVariableName->pNameToken,
					ProcessGenericArguments(moduleTable, state, functionCallExpressionNode.pTypeOrVariableName->GenericArguments, typeMap),
					typeMap);
				if (parentType != nullptr) {
					// static member function call
					parentTypeName = state.NamedValues.GetTypeName(parentType);
					mangledName = NodeBuffer::GetMangledName("", functionCallExpressionNode.pTypeOrVariableName->pNameToken->Value, functionName);
				}
				else {
					logErrorAtCurrentPosition(moduleTable, functionCallExpressionNode.pTypeOrVariableName->pNameToken, "'{0}' is not a variable or type name.", varOrTypeName);
					goto error;
				}
			}
		}

		// retrieve the original function definition
		funcResult = moduleTable.GetFunctionDefinition(mangledName);

		if (funcResult.Code == SearchResultCode::NotFound)
		{
			logErrorAtCurrentPosition(moduleTable, functionCallExpressionNode.pFunctionNameToken, "Could not find function '{0}'.", functionCallExpressionNode.pFunctionNameToken->Value);
			goto error;
		}
		else if (funcResult.Code == SearchResultCode::Inaccessible)
		{
			logErrorAtCurrentPosition(moduleTable, functionCallExpressionNode.pFunctionNameToken, "Function '{0}' is private and cannot be accessed here.", functionCallExpressionNode.pFunctionNameToken->Value);
			goto error;
		}
		else
		{
			pCalleeFunctionType = funcResult.pDefiniton->pFunctionType;
		}

		// build the generic type map before evaluating the function arguments
		buildGenericTypeMap(moduleTable, state,
			funcResult.pDefiniton->pFunctionType->GenericParameters,
			functionCallExpressionNode.GenericArguments,
			typeMap
		);

		// evaluate all the function arguments
		// this has to be done before trying to locate the llvm function to call because the llvm function name is derived from the argument types
		{
			GenericTypeMap parentTypeMap;
			if (parentType != nullptr) {
				state.NamedValues.GetGenericTypeMap(parentTypeName, parentTypeMap);
			}
			// merge the function's generic arguments with the type's generic arguments
			typeMap.merge(parentTypeMap);
			if (!evaluateFunctionArguments(moduleTable, state, (insertSelfAsFirstParam != None), pCalleeFunctionType, Arguments,
				typeMap,
				argumentValues)
				) {
				goto error;
			}
			// look up the function in the global module table
			// for generic functions, we need the full function name including any generic parameters
			{
				std::vector<VARIABLE_DECLARATION*> empty;
				extendedName = getExtendedFunctionName(moduleTable, state, "",
					parentType == nullptr ? "" : std::string(state.NamedValues.GetTypeName(parentType)),
					parentType == nullptr ? funcResult.MangledName : functionName,
					funcResult.pDefiniton->pFunctionType->GenericParameters,
					functionCallExpressionNode.GenericArguments,
					(funcResult.pDefiniton->pBody ? funcResult.pDefiniton->pFunctionType->Parameters : empty),	// do not mangle external function declarations
					argumentValues, parentTypeMap, typeMap);
			}
		}
		// make sure the built-in function has already been generated
		if (funcResult.Code == SearchResultCode::BuiltIn) {
#ifndef FIRST_PARAMETER_BYREF
			insertSelfAsFirstParam = Value;
#endif
			generateBuiltInFunction(state, extendedName);
		}

		// first parameter is &Self
		if (insertSelfAsFirstParam != None)
		{
			VARIABLE* var = functionCallExpressionNode.pObject->Get<VARIABLE>();
			AlloyType* identifierType = nullptr;
			AlloyValue ptrValue = generateIdentifier(moduleTable, state, *var, identifierType);
			if (ptrValue.Ptr == nullptr)
			{
				logErrorAtCurrentPosition(moduleTable, var->pNameToken, "Error evaluating variable '{0}'!", var->pNameToken->Value);
				goto error;
			}

			argumentValues.insert(argumentValues.begin(), AlloyValue(insertSelfAsFirstParam == Reference ? ptrValue.Ptr : ptrValue.Value, identifierType));
		}

		calleeFunc = state.Module->getFunction(extendedName);

		// function not found, it might not have been processed yet
		// check if function is already in the parser and process it
		if (!calleeFunc)
		{
			calleeFunc = generateFunctionDefinition(moduleTable, state, parentType, *funcResult.pDefiniton, funcResult.ModuleName, functionCallExpressionNode.GenericArguments, argumentValues);

			// ASSERT(Arguments.size() > 0 || calleeFunc == state.Module->getFunction(extendedName), "Function was not generated with the right name!");
		}

		if (!calleeFunc
			|| !pCalleeFunctionType)
		{
			logErrorAtCurrentPosition(moduleTable, functionCallExpressionNode.pFunctionNameToken, "Cannot find function '{0}'!", functionName);
			goto error;
		}

		// Create the argument values vector for CreateCall
		for (AlloyValue valueType : argumentValues) {
			argVals.push_back(valueType.Value);
		}

		{
			llvm::Value* value = state.Builder->CreateCall(calleeFunc, argVals,
				(calleeFunc->getReturnType()->getTypeID() != llvm::Type::VoidTyID ? functionName : "")	// giving the return value a name solves a bug internal to llvm, e.g. the switch/case unit test
				);
			if (expectedType != nullptr) {
				AlloyValue::convertValueToType(state, value, expectedType);
			}
			result = AlloyValue(value, AlloyType::get(value->getType()));
		}

	error:
		return result;
	}

	AlloyValue generateEnclosedExpression(ModuleTable& moduleTable, LLVMState& state, const ENCLOSED_EXPRESSION& enclosedExpressionNode,
		AlloyType*& expectedType)
	{
		return generateExpression(moduleTable, state, *enclosedExpressionNode.pExpression, expectedType);
	}

	llvm::Value* compareEnumValues(ModuleTable& moduleTable, LLVMState& state,
		llvm::StructType* leftType, const AlloyValue& leftVal, llvm::StructType* rightType, const AlloyValue& rightVal,
		bool capture
	)
	{
		//
		// for enums, we compare the types and the first element of the structure which is the ID of the enum element
		//
		llvm::Value* result = nullptr;
		llvm::Value* result1 = nullptr;
		llvm::Value* result2 = nullptr;

		if (leftType == rightType)
		{
			llvm::Value* left = state.Builder->CreateLoad(leftType, leftVal.Ptr);
			llvm::Value* right = state.Builder->CreateLoad(rightType, rightVal.Ptr);
			{
				llvm::Value* indexLeft = state.Builder->CreateExtractValue(left, EnumPayloadIndex);
				llvm::Value* indexRight = state.Builder->CreateExtractValue(right, EnumPayloadIndex);
				result1 = state.Builder->CreateICmpEQ(indexLeft, indexRight);
			}

			// in the case of enums with payloads, this is where we capture the payload value
			llvm::Value* payload = state.Builder->CreateExtractValue(left, EnumPayloadValue);
			if (payload
				&& capture
				) {
				state.NamedValues.GetEnumCapturedValue() = payload;
			}

			{
				// now compare the enum type's unique ID
				llvm::Value* idLeft = state.Builder->CreateExtractValue(left, EnumPayloadEnumID);
				llvm::Value* idRight = state.Builder->CreateExtractValue(right, EnumPayloadEnumID);
				result2 = state.Builder->CreateICmpEQ(idLeft, idRight);
			}

			// both the index and enum structure ID should be equal
			result = state.Builder->CreateAnd(result1, result2);
		}

		return result;
	}

	llvm::Value* compareStructValues(ModuleTable& moduleTable, LLVMState& state,
		llvm::StructType* leftType, llvm::Value* leftVal, llvm::StructType* rightType, llvm::Value* rightVal
	)
	{
		llvm::Value* result = nullptr;

		// retrieve the StructCompare library function
		llvm::Function* structCompare = generateStructureComparisonFunction(moduleTable, state);
		if (structCompare != nullptr) {
			// and call it with the pointers and sizes of the two structures
			std::vector<llvm::Value*> args;
			llvm::AllocaInst* p1 = state.Builder->CreateAlloca(leftType, 0, nullptr, "p1");
			state.Builder->CreateStore(leftVal, p1);
			args.push_back(p1);
			// a very convulated way of getting the size of the structure
			args.push_back(llvm::ConstantInt::get(*state.Context, llvm::APInt(32, (size_t)state.Module->getDataLayout().getTypeAllocSize(leftType))));
			llvm::AllocaInst* p2 = state.Builder->CreateAlloca(rightType, 0, nullptr, "p2");
			state.Builder->CreateStore(rightVal, p2);
			args.push_back(p2);
			args.push_back(llvm::ConstantInt::get(*state.Context, llvm::APInt(32, (size_t)state.Module->getDataLayout().getTypeAllocSize(rightType))));
			result = state.Builder->CreateCall(structCompare, args);
		}
		else {
			ASSERT(false, "Something is wrong in the library function " StructCompareFunctionName "!");
		}

		return result;
	}

	AlloyValue generateBinaryExpression(ModuleTable& moduleTable, LLVMState& state, const BINARY& binaryExpressionNode)
	{
		AlloyType* leftExpressionType = nullptr;
		AlloyValue left = generateExpression(moduleTable, state, *binaryExpressionNode.pLeft, leftExpressionType);
		llvm::Value* leftVal = left.Value;
		AlloyType* rightExpressionType = left.Type;	// when computing rightVal, try to obtain a value of same type as leftVal
		AlloyValue right = generateExpression(moduleTable, state, *binaryExpressionNode.pRight, rightExpressionType);
		llvm::Value* rightVal = right.Value;

		if (!left.isValid() || !right.isValid())
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID, 
				"Error evaluating expression!");
			return AlloyValue();
		}

		// the parent type is set in case of enumerations and allows us to check if the enumerations are of the same type
		AlloyType* leftType = (left.parentType == nullptr ? left.Type : left.parentType);
		AlloyType* rightType = (right.parentType == nullptr ? right.Type : right.parentType);

		// check if either the left or right values are smart pointers and load underlying value
		bool isArray;
		AlloyType* elementType = SmartPointerClass::isSmartPointer(state, leftType, isArray);
		if (elementType) {
			// We have a pointer represented by a smart pointer structure
			leftVal = SmartPointerClass::getValue(state, leftVal, left.Ptr, 0);
			leftType = elementType;
		}
		elementType = SmartPointerClass::isSmartPointer(state, rightType, isArray);
		if (elementType) {
			// We have a pointer represented by a smart pointer structure
			llvm::Value* memberPtr = nullptr;
			rightVal = SmartPointerClass::getValue(state, rightVal, right.Ptr, 0);
			rightType = elementType;
		}

		// check that types match
		if (leftType->llvmType != rightType->llvmType)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID
				"Binary operator must be applied to matching types! Current types are '{0}' and '{1}'.",
				state.NamedValues.GetTypeName(leftType), state.NamedValues.GetTypeName(rightType));
			return AlloyValue();
		}

		std::string_view operatorStr = binaryExpressionNode.pOpToken->Value;
		bool isFloatingPoint = leftType->llvmType->isFloatingPointTy();

		// for structures or enums, llvm does not support comparing their values. Use our own procedure for comparison
		// only equal and not equal operators are supported
		if (leftType->getTypeID() == llvm::Type::StructTyID || rightType->getTypeID() == llvm::Type::StructTyID) {
			llvm::Value* result = nullptr;
			std::string_view leftTypeName = state.NamedValues.GetTypeName(leftType);
			ASSERT((reinterpret_cast<llvm::StructType*>(leftType->llvmType))->isPacked(), "Structures should be created with the packed flag ON otherwise we cannot compare them");
			if (operatorStr != "==" && operatorStr != "!=") {
				logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID
					"Binary operator cannot be applied to structure or enum types! Current type is '{0}'.",
					leftTypeName);
			}
			else if (state.NamedValues.IsEnumType(leftType)) {
				// according to the unwritten specs, we only capture when the right hand side is an enum value and not an identifier (or anything else)
				bool capturePayload = binaryExpressionNode.pRight->Is<PRIMARY>() && binaryExpressionNode.pRight->Get<PRIMARY>()->Is<ENUM_VALUE>();
				result = compareEnumValues(moduleTable, state, static_cast<llvm::StructType*>(leftExpressionType->llvmType), left, static_cast<llvm::StructType*>(rightExpressionType->llvmType), right, capturePayload);
			}
			else {
				result = compareStructValues(moduleTable, state, static_cast<llvm::StructType*>(leftType->llvmType), leftVal, static_cast<llvm::StructType*>(rightType->llvmType), rightVal);
			}

			// for the not equal operator, invert the result
			if (operatorStr == "!=") {
				result = state.Builder->CreateNot(result);
			}

			return AlloyValue(result, AlloyType::get("bool"));
		}

		// logical operators

		if (operatorStr == "&&")
		{
			return AlloyValue(state.Builder->CreateAnd(leftVal, rightVal, "andtmp"), left.Type);
		}

		if (operatorStr == "||")
		{
			return AlloyValue(state.Builder->CreateOr(leftVal, rightVal, "ortmp"), left.Type);
		}

		// relational operators

		if (operatorStr == "==")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFCmpOEQ(leftVal, rightVal, "eqtmp")
				: state.Builder->CreateICmpEQ(leftVal, rightVal, "eqtmp"), AlloyType::get("bool"));
		}

		if (operatorStr == "!=")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFCmpONE(leftVal, rightVal, "neqtmp")
				: state.Builder->CreateICmpNE(leftVal, rightVal, "neqtmp"), AlloyType::get("bool"));
		}

		if (operatorStr == ">")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFCmpUGT(leftVal, rightVal, "gttmp")
				: state.Builder->CreateICmpSGT(leftVal, rightVal, "gttmp"), AlloyType::get("bool"));
		}

		if (operatorStr == "<")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFCmpULT(leftVal, rightVal, "lttmp")
				: state.Builder->CreateICmpSLT(leftVal, rightVal, "lttmp"), AlloyType::get("bool"));
		}

		if (operatorStr == ">=")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFCmpUGE(leftVal, rightVal, "getmp")
				: state.Builder->CreateICmpSGE(leftVal, rightVal, "getmp"), AlloyType::get("bool"));
		}

		if (operatorStr == "<=")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFCmpULE(leftVal, rightVal, "letmp")
				: state.Builder->CreateICmpSLE(leftVal, rightVal, "letmp"), AlloyType::get("bool"));
		}

		// additive operators

		if (operatorStr == "+")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFAdd(leftVal, rightVal, "addtmp")
				: state.Builder->CreateAdd(leftVal, rightVal, "addtmp"), left.Type);
		}

		if (operatorStr == "-")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFSub(leftVal, rightVal, "subtmp")
				: state.Builder->CreateSub(leftVal, rightVal, "subtmp"), left.Type);
		}

		// multiplicative operators

		if (operatorStr == "*")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFMul(leftVal, rightVal, "multmp")
				: state.Builder->CreateMul(leftVal, rightVal, "multmp"), left.Type);
		}

		if (operatorStr == "/")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFDiv(leftVal, rightVal, "divtmp")
				: state.Builder->CreateUDiv(leftVal, rightVal, "divtmp"), left.Type);
		}

		if (operatorStr == "%")
		{
			return AlloyValue(isFloatingPoint
				? state.Builder->CreateFRem(leftVal, rightVal, "modtmp")
				: state.Builder->CreateURem(leftVal, rightVal, "modtmp"), left.Type);
		}

		ASSERT(false, "Unknown binary operator '{0}'!", operatorStr);
		return AlloyValue();
	}

	AlloyValue generateUnaryExpression(ModuleTable& moduleTable, LLVMState& state, const UNARY& unaryExpressionNode,
		AlloyType*& expectedType)
	{
		std::string_view operatorStr = unaryExpressionNode.pOpToken->Value;
		AlloyValue result;

		if (operatorStr == "-")
		{
			result = generateExpression(moduleTable, state, *unaryExpressionNode.pExpression, expectedType);

			if (result.isValid())
			{
				bool isFloatingPoint = result.Type->llvmType->isFloatingPointTy();
				if (isFloatingPoint)
					result.Value = state.Builder->CreateFNeg(result.Value, "negtmp");
				else
					result.Value = state.Builder->CreateNeg(result.Value, "negtmp");
			}
		}
		else
			if (operatorStr == "!")
			{
				result = generateExpression(moduleTable, state, *unaryExpressionNode.pExpression, expectedType);

				if (result.isValid())
				{
					result.Value = state.Builder->CreateNot(result.Value, "nottmp");
				}
			}
			else
				if (operatorStr == "&"
					&& unaryExpressionNode.pExpression->Is<PRIMARY>()
					&& unaryExpressionNode.pExpression->Get<PRIMARY>()->Is<VARIABLE>())
				{
					AlloyType* tempType = nullptr;
					// retrieve the identifier
					result = generateIdentifier(moduleTable, state,
						*unaryExpressionNode.pExpression->Get<PRIMARY>()->Get<VARIABLE>(),
						tempType);
					if (result.isValid()) {
						// convert the identifier into a pointer to the identifier
						result = AlloyValue(result.Ptr, AlloyType::getPointerType(tempType), nullptr, result.isConst);
					}
				}
				else
				{
					ASSERT(false, "Unknown unary operator!");
				}

		return result;
	}

	AlloyValue generatePrimaryExpression(ModuleTable& moduleTable, LLVMState& state, const PRIMARY& primary,
		AlloyType*& expectedType)
	{
		//
		// using PRIMARY = VariantNode<LITERAL, VARIABLE, VARIABLE_DEFINITION, FUNCTION_CALL, CONSTRUCTOR,
		//  	POINTER_INIT, POINTER_MOVE, INITIALIZER_LIST, ENCLOSED_EXPRESSION>;
		//
		AlloyValue result;

		if (primary.Is<LITERAL>()) {
			result = generateLiteral(moduleTable, state, *primary.Get<LITERAL>(), expectedType);
		}
		else if (primary.Is<VARIABLE>()) {
			result = generateIdentifier(moduleTable, state, *primary.Get<VARIABLE>(), expectedType);
		}
		else if (primary.Is<CONSTRUCTOR>()) {
			result = generateConstructorExpression(moduleTable, state, *primary.Get<CONSTRUCTOR>());
		}
		else if (primary.Is<POINTER_INIT>()) {
			result = generatePointerInitializerExpression(moduleTable, state, *primary.Get<POINTER_INIT>(), expectedType);
		}
		else if (primary.Is<ARRAY_INIT>()) {
			result = generateArrayInitializerExpression(moduleTable, state, *primary.Get<ARRAY_INIT>(), expectedType);
		}
		else if (primary.Is<POINTER_MOVE>()) {
			result = generatePointerMoveExpression(moduleTable, state, *primary.Get<POINTER_MOVE>(), expectedType);
		}
		else if (primary.Is<INITIALIZER_LIST>()) {
			result = generateInitializerListExpression(moduleTable, state, *primary.Get<INITIALIZER_LIST>(), expectedType);
		}
		else if (primary.Is<VARIABLE_DEFINITION>()) {
			result = generateVariableDefinitionExpression(moduleTable, state, *primary.Get<VARIABLE_DEFINITION>());
		}
		else if (primary.Is<ARRAY_ACCESS>()) {
			result = generateArrayAccessExpression(moduleTable, state, *primary.Get<ARRAY_ACCESS>(), expectedType);
		}
		else if (primary.Is<MEMBER_ACCESS>()) {
			result = generateMemberAccessExpression(moduleTable, state, *primary.Get<MEMBER_ACCESS>(), expectedType);
		}
		else if (primary.Is<FUNCTION_CALL>()) {
			result = generateFunctionCallExpression(moduleTable, state, *primary.Get<FUNCTION_CALL>(), expectedType);
		}
		else if (primary.Is<ENCLOSED_EXPRESSION>()) {
			result = generateEnclosedExpression(moduleTable, state, *primary.Get<ENCLOSED_EXPRESSION>(), expectedType);
		}
		else if (primary.Is<ENUM_VALUE>()) {
			result = generateEnumValue(moduleTable, state, *primary.Get<ENUM_VALUE>(), expectedType);
		}
		else {
			ASSERT(false, "Unknown primary expression node kind!");
		}

		return result;
	}

	AlloyValue generateAssignmentExpression(ModuleTable& moduleTable, LLVMState& state, const ASSIGNMENT& assignment,
		AlloyType*& expectedType)
	{
		AlloyValue ptrValue;

		if (assignment.pVariable->Is<POSTFIX>()
			&& assignment.pVariable->Get<POSTFIX>()->Is<MEMBER_ACCESS>()
			)
		{
			ptrValue = generateMemberAccessExpression(moduleTable, state, *assignment.pVariable->Get<POSTFIX>()->Get<MEMBER_ACCESS>(), expectedType);
		}
		else if (assignment.pVariable->Is<PRIMARY>()
			&& assignment.pVariable->Get<PRIMARY>()->Is<VARIABLE>())
		{
			ptrValue = generateIdentifier(moduleTable, state, *assignment.pVariable->Get<PRIMARY>()->Get<VARIABLE>(), expectedType);
		}
		else if (assignment.pVariable->Is<POSTFIX>()
			&& assignment.pVariable->Get<POSTFIX>()->Is<ARRAY_ACCESS>())
		{
			ptrValue = generateArrayAccessExpression(moduleTable, state, *assignment.pVariable->Get<POSTFIX>()->Get<ARRAY_ACCESS>(), expectedType);
		}

		if (!ptrValue.isValid())
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID 
				"Error evaluating identifier!");
			return AlloyValue();
		}

		if (ptrValue.isConst) {
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID 
				"Assigning a value to a constant!");
			return AlloyValue();
		}

		AlloyValue expressionVal = generateExpression(moduleTable, state, *assignment.pValue, expectedType);

		if (!expressionVal.isValid())
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID
				"Error evaluating expression!");
			return AlloyValue();
		}

		// convert value to right type if possible
		AlloyValue::convertValueToType(state, expressionVal, ptrValue.Type);

		// store the value into the alloca
		state.Builder->CreateStore(expressionVal.Value, ptrValue.Ptr);
		return expressionVal;
	}

	AlloyValue generateExpression(ModuleTable& moduleTable, LLVMState& state, const EXPRESSION& expressionNode,
		AlloyType*& expectedType)
	{
		AlloyValue result;

		if (expressionNode.Is<BINARY>()) {
			result = generateBinaryExpression(moduleTable, state, *expressionNode.Get<BINARY>());
		}
		else if (expressionNode.Is<UNARY>()) {
			result = generateUnaryExpression(moduleTable, state, *expressionNode.Get<UNARY>(), expectedType);
		}
		else if (expressionNode.Is<PRIMARY>()) {
			result = generatePrimaryExpression(moduleTable, state, *expressionNode.Get<PRIMARY>(), expectedType);
		}
		else if (expressionNode.Is<ASSIGNMENT>()) {
			result = generateAssignmentExpression(moduleTable, state, *expressionNode.Get<ASSIGNMENT>(), expectedType);
		}
		else if (expressionNode.Is<POSTFIX>()) {
			const POSTFIX& postfix = *expressionNode.Get<POSTFIX>();
			if (postfix.Is<ARRAY_ACCESS>()) {
				result = generateArrayAccessExpression(moduleTable, state, *postfix.Get<ARRAY_ACCESS>(), expectedType);
			}
			else if (postfix.Is<MEMBER_ACCESS>()) {
				result = generateMemberAccessExpression(moduleTable, state, *postfix.Get<MEMBER_ACCESS>(), expectedType);
			}
			else if (postfix.Is<FUNCTION_CALL>()) {
				result = generateFunctionCallExpression(moduleTable, state, *postfix.Get<FUNCTION_CALL>(), expectedType);
			}
			else {
				ASSERT(false, "Unknown postfix expression node kind!");
			}
		}
		else {
			ASSERT(false, "Unknown expression node kind!");
		}

		return result;
	}

#pragma endregion

#pragma region Statements

	llvm::Value* generateVariableDefinitionStatement(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DEFINITION& variableDefinition)
	{
		return generateVariableDefinitionExpression(moduleTable, state, variableDefinition).Value;
	}

	llvm::Value* generateFunctionCallStatement(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_CALL& functionCall)
	{
		AlloyType* expectedType = nullptr;
		return generateFunctionCallExpression(moduleTable, state, functionCall, expectedType).Value;
	}

	llvm::Value* generateAssignmentStatement(ModuleTable& moduleTable, LLVMState& state, const ASSIGNMENT& assignment)
	{
		AlloyType* identifierType = nullptr;
		return generateAssignmentExpression(moduleTable, state, assignment, identifierType).Value;
	}

	llvm::Value* generateForLoopStatement(ModuleTable& moduleTable, LLVMState& state, const FOR_LOOP& forLoop)
	{
		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();

		ASSERT(func != nullptr, "No function to insert into!");

		// emit init code before the loop
		if (forLoop.pInitialization != nullptr)
		{
			AlloyType* tempType = nullptr;
			llvm::Value* initVal = generateExpression(moduleTable, state, *forLoop.pInitialization, tempType).Value;

			if (initVal == nullptr)
			{
				logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: forLoop...
					"Error evaluating expression!");
				return nullptr;
			}
		}

		// create a new basic block to start insertion into
		llvm::BasicBlock* loopBlock = llvm::BasicBlock::Create(*state.Context, "loop", func);

		// insert an explicit fall through from the current block to the loopBlock
		state.Builder->CreateBr(loopBlock);

		// start insertion into the loopBlock
		state.Builder->SetInsertPoint(loopBlock);

		// generate the body of the loop
		if (forLoop.pBody != nullptr) {
			if (!generateStatement(moduleTable, state, *forLoop.pBody)) {
				logErrorAtCurrentPosition(moduleTable, nullptr,	// TBD: forLoop... 
					"Error evaluating expression!");
				return nullptr;
			}
		}

		// emit step value
		if (forLoop.pIncrement != nullptr) {
			AlloyType* tempType = nullptr;
			llvm::Value* stepVal = generateExpression(moduleTable, state, *forLoop.pIncrement, tempType).Value;

			if (stepVal == nullptr)
			{
				logErrorAtCurrentPosition(moduleTable, nullptr,	// TBD: forLoop... 
					"Error evaluating expression!");
				return nullptr;
			}
		}

		// emit the condition
		llvm::Value* conditionVal = nullptr;
		if (forLoop.pCondition != nullptr) {
			AlloyType* tempType = nullptr;
			conditionVal = generateExpression(moduleTable, state, *forLoop.pCondition, tempType).Value;

			if (conditionVal == nullptr)
			{
				logErrorAtCurrentPosition(moduleTable, nullptr,	// TBD: forLoop... 
					"Error evaluating expression!");
				return nullptr;
			}

			conditionVal = convertToBool(state, conditionVal);
		}

		// create the "after loop" block and insert it
		llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(*state.Context, "afterloop", func);

		// insert the conditional branch into the end of afterBlock
		state.Builder->CreateCondBr(conditionVal, loopBlock, afterBlock);

		// any new code will be inserted in afterBlock
		state.Builder->SetInsertPoint(afterBlock);

		// for expr always returns 0.0
		return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*state.Context));
	}

	llvm::Value* generateWhileLoopStatement(ModuleTable& moduleTable, LLVMState& state, const WHILE_LOOP& whileLoop)
	{
		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();

		ASSERT(func != nullptr, "No function to insert into!");

		// create a new basic block to start insertion into
		llvm::BasicBlock* loopBlock = llvm::BasicBlock::Create(*state.Context, "loop", func);

		// insert an explicit fall through from the current block to the loopBlock
		state.Builder->CreateBr(loopBlock);

		// start insertion into the loopBlock
		state.Builder->SetInsertPoint(loopBlock);

		// generate the body of the loop
		if (!generateStatement(moduleTable, state, *whileLoop.pStatement)) {
			return nullptr;
		}

		// emit the condition
		AlloyType* tempType = nullptr;
		llvm::Value* conditionVal = generateExpression(moduleTable, state, *whileLoop.pCondition, tempType).Value;

		if (conditionVal == nullptr)
		{
			return nullptr;
		}

		conditionVal = convertToBool(state, conditionVal);

		// create the "after loop" block and insert it
		llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(*state.Context, "afterloop", func);

		// insert the conditional branch into the end of afterBlock
		state.Builder->CreateCondBr(conditionVal, loopBlock, afterBlock);

		// any new code will be inserted in afterBlock
		state.Builder->SetInsertPoint(afterBlock);

		// while expr always returns 0.0
		return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*state.Context));
	}

	void captureEnumPayload(ModuleTable& moduleTable, LLVMState& state, const Token* varNameToken, llvm::Value* payload, llvm::BasicBlock* insertionBlock)
	{
		//
		// Create a variable with the enum payload value
		//
		if (varNameToken != nullptr) {
			const std::string name(varNameToken->Value);
			if (payload == nullptr) {
				logErrorAtCurrentPosition(moduleTable,
					varNameToken,
					"Payload capture variable '{0}' is given but enum value does not a payload!",
					name
				);
			}
			else {
				// llvm::IRBuilder<> tempBuilder(insertionBlock, insertionBlock->begin());
				// llvm::AllocaInst* allocaInst = tempBuilder.CreateAlloca(payload->getType(), nullptr, name);
				// create the alloca
				llvm::AllocaInst* allocaInst = createEntryBlockAlloca(
					state.Builder->GetInsertBlock()->getParent(),
					name,
					AlloyType::get(payload->getType())
				);
				state.Builder->CreateStore(payload, allocaInst);
				// add the variable to the named values
				state.NamedValues.InsertValue(name, allocaInst, AlloyType::get(payload->getType()),
					nullptr, false, false);
			}
		}
	}

	llvm::Value* generateIfStatement(ModuleTable& moduleTable, LLVMState& state, const IF_STATEMENT& ifStatement)
	{
		AlloyType* tempType = nullptr;
		llvm::Value* payload = nullptr;

		state.NamedValues.GetEnumCapturedValue() = nullptr;	// reset the captured payload value in case it is used in the condition statement
		llvm::Value* conditionVal = generateExpression(moduleTable, state, *ifStatement.pCondition, tempType).Value;

		if (conditionVal == nullptr)
		{
			return nullptr;
		}

		conditionVal = convertToBool(state, conditionVal);

		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();
		ASSERT(func != nullptr, "No function to insert into!");

		// in order to accomodate enum payload capture, we need to create a new identifier scope before entering the if's statement block
		// PushScope will reset the EnumCapturedValue, so save it first
		payload = state.NamedValues.GetEnumCapturedValue();
		// reset the captured value for the next call
		state.NamedValues.GetEnumCapturedValue() = nullptr;
		state.NamedValues.PushScope("");

		// create blocks for the then and else cases
		llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(*state.Context, "then", func);
		llvm::BasicBlock* elseBlock = llvm::BasicBlock::Create(*state.Context, "else");
		llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(*state.Context, "ifcont");

		state.Builder->CreateCondBr(conditionVal, thenBlock, elseBlock);

		// emit then value
		state.Builder->SetInsertPoint(thenBlock);

		// if provided, create a variable with name ifStatement.pCaptureNameToken and set the value to the captured value
		// the captured value is set in the binary comparison expression
		captureEnumPayload(moduleTable, state, ifStatement.pCaptureNameToken, payload, &func->getEntryBlock());

		// the then block can be empty and return a null value, we don't really care
		generateStatement(moduleTable, state, *ifStatement.pStatement);

		// insert the branch into the end of the if block
		branchIfNotDuplicate(state, mergeBlock);

		// emit else block if any
		func->insert(func->end(), elseBlock);
		state.Builder->SetInsertPoint(elseBlock);

		if (ifStatement.pElseStatement != nullptr)
		{
			if (!generateStatement(moduleTable, state, *ifStatement.pElseStatement)) {
				return nullptr;
			}
		}

		// insert the branch into the end of the if block
		branchIfNotDuplicate(state, mergeBlock);

		// emit merge block
		func->insert(func->end(), mergeBlock);
		state.Builder->SetInsertPoint(mergeBlock);

		// end of the if statement identifier scope
		state.NamedValues.PopScope();

		return llvm::ConstantInt::getTrue(*state.Context);
	}

	llvm::Value* generateStatementBlock(ModuleTable& moduleTable, LLVMState& state, const STATEMENT_BLOCK& statementBlock)
	{
		llvm::Value* statementVal = llvm::ConstantInt::getTrue(*state.Context);		// set a default value for empty blocks

		// statement blocks have their own identifier scope
		state.NamedValues.PushScope("StmtBlock");

		for (const STATEMENT* statement : statementBlock.Statements)
		{
			statementVal = generateStatement(moduleTable, state, *statement);

			if (statementVal == nullptr)
			{
				break;
			}
		}
		state.NamedValues.PopScope();

		return statementVal;
	}

	llvm::Value* generateReturnStatement(ModuleTable& moduleTable, LLVMState& state, const RETURN& returnStatement)
	{
		// handle void return
		if (returnStatement.pValue == nullptr)
		{
			if (state.CurrentReturnValue != nullptr)
			{
				logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID
					"Function expects a return value of type '{0}'!", state.NamedValues.GetTypeName(AlloyType::get(state.CurrentReturnValue->getType())));

				return nullptr;
			}

			return state.Builder->CreateBr(state.FuncExitBlock);
		}

		AlloyType* tempType = nullptr;
		llvm::Value* expressionValue = generateExpression(moduleTable, state, *returnStatement.pValue, tempType).Value;

		if (expressionValue == nullptr)
		{
			return nullptr;
		}

		if (state.CurrentReturnValue != nullptr) {
			// convert the return value to the right type
			// TODO: should we generate a warning here?
			AlloyValue::convertValueToType(state, expressionValue, AlloyType::get(state.CurrentReturnValue->getAllocatedType()));
		}

		if (state.CurrentReturnValue == nullptr
			|| expressionValue->getType() != state.CurrentReturnValue->getAllocatedType())
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID,
				"Function has return type '{0}' but provided return value is of type '{1}'!",
				state.CurrentReturnValue == nullptr ? "void" : state.NamedValues.GetTypeName(AlloyType::get(state.CurrentReturnValue->getAllocatedType())),
				state.NamedValues.GetTypeName(AlloyType::get(expressionValue->getType())));
			return nullptr;
		}

		// store the return value and branch to the exit block
		state.Builder->CreateStore(expressionValue, state.CurrentReturnValue);
		return state.Builder->CreateBr(state.FuncExitBlock);
	}

	llvm::Value* generateSwitchStatement(ModuleTable& moduleTable, LLVMState& state, const SWITCH_STATEMENT& statement)
	{
		llvm::Value* result = nullptr;
		llvm::SwitchInst* switchInst = nullptr;
		llvm::BasicBlock* afterBlock = nullptr;
		llvm::BasicBlock* switchBlock = nullptr;
		AlloyType* EnumType = nullptr;
		llvm::Value* payload = nullptr;

		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();
		ASSERT(func != nullptr, "No function to insert into!");

		// generate and check the condition expression for the switch statement
		AlloyType* tempType = nullptr;
		AlloyValue conditionVal = generateExpression(moduleTable, state, *statement.pSwitchValue, tempType);
		if (!conditionVal.isValid())
		{
			goto failed;
		}

		// switch/case statements using enumerations, we need to retrieve the index of enum value
		if (conditionVal.Type->name().starts_with(_EnumPayloadStruct_))
		{
			EnumType = conditionVal.parentType;
			// check if there is a payload and load it
			payload = state.Builder->CreateExtractValue(conditionVal.Value, EnumPayloadValue);
			// now extract the index of the enum value
			conditionVal.Value = state.Builder->CreateExtractValue(conditionVal.Value, EnumPayloadIndex);
		}

		// create a new basic block to start insertion into
		switchBlock = llvm::BasicBlock::Create(*state.Context, "switch", func);

		// insert an explicit fall through from the current block to the switchBlock
		state.Builder->CreateBr(switchBlock);

		// start insertion into the switchBlock
		state.Builder->SetInsertPoint(switchBlock);

		// create the default block and insert it
		afterBlock = llvm::BasicBlock::Create(*state.Context, "default", func);

		// create an llvm switch instruction class, default branch is set to null
		// Note: if this line asserts in LLVM, make sure SDL checks are off in the compiler settings
		switchInst = state.Builder->CreateSwitch(conditionVal.Value, afterBlock, statement.Cases.size());

		for (auto& caseStmt : statement.Cases) {
			EXPRESSION* expr = std::get<0>(caseStmt);
			tempType = nullptr;

			// in order to accomodate enum payload capture, we need to create a new identifier scope before entering the case's statement block
			state.NamedValues.PushScope("");

			// create a new basic block to start insertion into
			llvm::BasicBlock* caseBlock = llvm::BasicBlock::Create(*state.Context, "case", func);

			// start insertion into the caseBlock
			state.Builder->SetInsertPoint(caseBlock);

			AlloyValue cond = generateExpression(moduleTable, state, *expr, tempType);
			if (EnumType != nullptr) {
				// switch/case statements using enumerations, the enumerations should be of the same type
				if (EnumType != cond.parentType) {
					logErrorAtCurrentPosition(moduleTable, nullptr, "Case condition type {0} is not of the same enum type as switch value {1}!",
						cond.parentType->name(), EnumType->name());
					goto failed;
				}

				if (payload != nullptr) {
					// if provided, create a variable and set the value to the captured value
					Token* tok = std::get<1>(caseStmt);
					if (tok != nullptr && !tok->Value.empty()) {
						captureEnumPayload(moduleTable, state, tok, payload, caseBlock);
					}
				}

				// retrieve the index of the enum value which should be a constant int
				if (!expr->Is<PRIMARY>() || !expr->Get<PRIMARY>()->Is<ENUM_VALUE>()) {
					logErrorAtCurrentPosition(moduleTable, nullptr, "Case condition is not part of the enumeration!");
					goto failed;
				}
				ENUM_VALUE* ev = expr->Get<PRIMARY>()->Get<ENUM_VALUE>();
				cond.Value = generateEnumValueIndex(moduleTable, state, *ev, tempType).Value;
				if (!llvm::isa<llvm::ConstantInt>(cond.Value)) {
					logErrorAtCurrentPosition(moduleTable, nullptr, "Switch/Case condition is not a constant value!");
					goto failed;
				}

			}

			// generate the body of the case statement (can be empty)
			if (std::get<2>(caseStmt) != nullptr) {
				if (!generateStatement(moduleTable, state, *std::get<2>(caseStmt))) {
					logErrorAtCurrentPosition(moduleTable, nullptr,
						"Error generating case statement!");
					goto failed;
				}
			}

			// jump to after the switch statement
			state.Builder->CreateBr(afterBlock);

			switchInst->addCase(static_cast<llvm::ConstantInt*>(cond.Value), caseBlock);

			// end of scope for this case condition
			state.NamedValues.PopScope();
		}

		// any new code will be inserted in afterBlock
		state.Builder->SetInsertPoint(afterBlock);

		// switch expr always returns 0.0
		result = llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*state.Context));

	failed:
		return result;
	}

	llvm::Value* generateStatement(ModuleTable& moduleTable, LLVMState& state, const STATEMENT& statement)
	{
		llvm::Value* result = nullptr;

		if (statement.Is<VARIABLE_DEFINITION>()) {
			result = generateVariableDefinitionStatement(moduleTable, state, *statement.Get<VARIABLE_DEFINITION>());
		}
		else if (statement.Is<FUNCTION_CALL>()) {
			result = generateFunctionCallStatement(moduleTable, state, *statement.Get<FUNCTION_CALL>());
		}
		else if (statement.Is<ASSIGNMENT>()) {
			result = generateAssignmentStatement(moduleTable, state, *statement.Get<ASSIGNMENT>());
		}
		else if (statement.Is<FOR_LOOP>()) {
			result = generateForLoopStatement(moduleTable, state, *statement.Get<FOR_LOOP>());
		}
		else if (statement.Is<WHILE_LOOP>()) {
			result = generateWhileLoopStatement(moduleTable, state, *statement.Get<WHILE_LOOP>());
		}
		else if (statement.Is<IF_STATEMENT>()) {
			result = generateIfStatement(moduleTable, state, *statement.Get<IF_STATEMENT>());
		}
		else if (statement.Is<STATEMENT_BLOCK>()) {
			result = generateStatementBlock(moduleTable, state, *statement.Get<STATEMENT_BLOCK>());
		}
		else if (statement.Is<RETURN>()) {
			result = generateReturnStatement(moduleTable, state, *statement.Get<RETURN>());
		}
		else if (statement.Is<SWITCH_STATEMENT>()) {
			result = generateSwitchStatement(moduleTable, state, *statement.Get<SWITCH_STATEMENT>());
		}
		else {
			ASSERT(false, "Unknown statement node kind!");
		}

		return result;
	}

#pragma endregion

#pragma region Definitions

	llvm::Function* generateExternDefinition(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_DEFINITION& externDefinition, const std::string& moduleName)
	{
		GenericTypeMap typeMap;
		return generateFunctionDeclaration(moduleTable, state, nullptr, externDefinition, moduleName, {}, {}, typeMap);
	}

	AlloyValue generateVariableDefinition(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DEFINITION& variableDefinition)
	{
		return generateVariableDefinitionExpression(moduleTable, state, variableDefinition);
	}

	AlloyType* generateArrayDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
		const ARRAY_TYPE& arrayDefinition, const GenericArgumentTypes& genericArguments,
		const GenericTypeMap& parentTypeMap)
	{
		//
		// return the type and subtype
		// e.g. var array : [i64; 10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }; when we encounter var array : [i64; 10] we know that we are
		// initializing an i64 array of 10 elements
		//

		AlloyType* identifierType = nullptr;
		uint64_t arraySize = 0;
		TypeModifier modifier = TypeModifier::None;
		AlloyType* elementType = nullptr;
		std::string arrayName;
		GenericTypeMap genericTypeMap(parentTypeMap);

		if (typeIdentifier.pNameToken) {
			// create the map from the array's generic types to the actual types requested by the caller
			int arg = 0;
			if (genericArguments.size())
				for (auto& type : typeIdentifier.GenericParameters) {
					genericTypeMap[std::string(type->pIdentifierToken->Value)] = genericArguments[arg];
					arg++;
				}

			// create mangled array name (if needed)
			arrayName = GetMangledName(state, moduleTable.GetCurrentContext(), typeIdentifier.pNameToken->Value, genericArguments, genericTypeMap);

			if (genericArguments.size() != typeIdentifier.GenericParameters.size())
			{
				logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
					"Invalid number of arguments for generic type '{0}'!", arrayName);
				goto exit;
			}
		}

		// get size if given
		if (arrayDefinition.pSizeLiteral != nullptr)
		{
			std::string_view sizeStr = arrayDefinition.pSizeLiteral->pValueToken->Value;
			std::from_chars(sizeStr.data(), sizeStr.data() + sizeStr.size(), arraySize);

			if (arraySize == 0)
			{
				logErrorAtCurrentPosition(moduleTable, arrayDefinition.pSizeLiteral->pValueToken, "Array size must be greater than 0!");
				identifierType = nullptr;
				goto exit;
			}
		}

		identifierType = generateTypeIdentifier(moduleTable, state, *arrayDefinition.pElementType, nullptr, genericTypeMap);
		modifier = arrayDefinition.pElementType->Modifier;
		elementType = identifierType;

		if (identifierType == nullptr)
		{
			logErrorAtCurrentPosition(moduleTable, arrayDefinition.pElementType->GetErrorToken(), "Unknown array element type!");
			goto exit;
		}

		switch (modifier) {
		case TypeModifier::None:
			break;

		case TypeModifier::Reference:
		{
			// allocating a pointer to this type
			elementType = AlloyType::getPointerType(identifierType);	// the contained type is not stored by llvm as all pointers are treated as "opaque"
			break;
		}

		case TypeModifier::Pointer:
		{
			// allocating a pointer to this type
			elementType = SmartPointerClass::GetSmartPointerStruct(state, identifierType, false);
			break;
		}

		default:
		{
			ASSERT(false, "Invalid type modifier!");
			identifierType = nullptr;
			goto exit;
			break;
		}
		}

		if (true	// creating all arrays as pointers in order to accomodate arrays of structures which are not supported by llvm
			|| arraySize == 0) {
			// when size is unknown, create a smart pointer instead of an array
			identifierType = SmartPointerClass::GetSmartPointerStruct(state, elementType, true);
		}
		else {
			identifierType = AlloyType::get(llvm::VectorType::get(elementType->llvmType, arraySize,
				false //not scalable
			));
		}

		// if we have a name token, add the newly created array type to NamedValues
		if (!arrayName.empty()) {
			state.NamedValues.InsertType(arrayName, identifierType, NamedValues::UserDefinedType::array);
		}

	exit:
		return identifierType;
	}

	AlloyType* generateStructDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
		const STRUCT_TYPE& structDefinition, const GenericArgumentTypes& genericArguments
	)
	{
		llvm::StructType* structType = nullptr;
		int memberIndex, arg;
		std::vector<llvm::Type*> memberTypes;
		std::unordered_map<std::string_view, NamedValues::TypeMemberInfo> memberNames;
		GenericTypeMap genericTypeMap;

		// create the map from the structure's generic types to the actual types requested by the caller
		arg = 0;
		if (genericArguments.size())
			for (auto& type : typeIdentifier.GenericParameters) {
				genericTypeMap[std::string(type->pIdentifierToken->Value)] = genericArguments[arg];
				arg++;
			}

		// create mangled structure name (if needed)
		std::string structName;
		structName = GetMangledName(state, moduleTable.GetCurrentContext(), typeIdentifier.pNameToken->Value, genericArguments, genericTypeMap);

#ifdef TRACE_CODE_GENERATOR
		logInfoAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
			"Processing structure {0}\n", structName);
#endif

		if (genericArguments.size() != typeIdentifier.GenericParameters.size())
		{
			logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
				"Invalid number of arguments for generic type '{0}'!", structName);
			goto failed;
		}

		// get a vector of member types
		memberIndex = 0;
		for (auto id : structDefinition.Members)
		{
			TypeModifier modifier = id.second->Modifier;
			AlloyType* identifierType = generateTypeIdentifier(moduleTable, state, *id.second, nullptr, genericTypeMap);

			if (!identifierType)
			{
				logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
					"Invalid structure member type for structure '{0}'!", structName);
				goto failed;
			}

			switch (modifier) {
			case TypeModifier::None:
				break;

			case TypeModifier::Pointer:
			{
				// allocating a smart pointer to this type
				identifierType = SmartPointerClass::GetSmartPointerStruct(state, identifierType, false);
				break;
			}

			case TypeModifier::Reference:
			{
				// allocating a pointer to this type
				identifierType = AlloyType::getPointerType(identifierType);
				break;
			}

			default:
			{
				ASSERT(false, "Invalid type modifier!");
				identifierType = nullptr;
				goto failed;
				break;
			}
			}

			memberTypes.push_back(identifierType->llvmType);

			// retrieve member name and add it to memberNames map
			const std::string_view memberName = id.first->Value;

			memberNames[memberName] = { memberIndex++, identifierType };
		}

		structType = llvm::StructType::create(*state.Context, memberTypes, structName, true);

		if (!structType)
		{
			logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
				"Invalid structure type for structure '{0}'!", structName);
			goto failed;
		}

		state.NamedValues.InsertType(structName, AlloyType::get(structType), NamedValues::UserDefinedType::structure,
			memberNames, genericTypeMap);

	failed:
		return AlloyType::get(structType);
	}

	AlloyType* generateEnumDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
		const ENUM_TYPE& enumDefinition, const GenericArgumentTypes& genericArguments
	)
	{
		//
		// In order to support payloads and generics in enumerations, we are processing and storing them in a way very similar to structures
		//
		llvm::StructType* enumType = nullptr;
		int memberIndex, arg;
		std::vector<llvm::Type*> memberTypes;
		std::unordered_map<std::string_view, NamedValues::TypeMemberInfo> memberNames;
		GenericTypeMap genericTypeMap;

		// create the map from the enum's generic types to the actual types requested by the caller
		arg = 0;
		if (genericArguments.size())
			for (auto& type : typeIdentifier.GenericParameters) {
				genericTypeMap[std::string(type->pIdentifierToken->Value)] = genericArguments[arg];
				arg++;
			}

		// create mangled enum name (if needed)
		std::string enumName = GetMangledName(state, moduleTable.GetCurrentContext(), typeIdentifier.pNameToken->Value, genericArguments, genericTypeMap);

		if (genericArguments.size() != typeIdentifier.GenericParameters.size())
		{
			logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
				"Invalid number of arguments for generic type '{0}'!", enumName);
			goto failed;
		}

		// get a vector of member types
		memberIndex = 0;
		for (auto id : enumDefinition.Members)
		{
			TypeModifier modifier = id.second ? id.second->Modifier : TypeModifier::None;
			AlloyType* identifierType = nullptr;
			if (id.second != nullptr) {
				identifierType = generateTypeIdentifier(moduleTable, state, *id.second, nullptr, genericTypeMap);

				if (!identifierType)
				{
					logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
						"Invalid enum member type for enum '{0}'!", enumName);
					goto failed;
				}

				switch (modifier) {
				case TypeModifier::None:
					break;

				case TypeModifier::Pointer:
				{
					// allocating a smart pointer to this type
					identifierType = SmartPointerClass::GetSmartPointerStruct(state, identifierType, false);
					break;
				}

				case TypeModifier::Reference:
				{
					// allocating a pointer to this type
					identifierType = AlloyType::getPointerType(identifierType);
					break;
				}

				default:
				{
					ASSERT(false, "Invalid type modifier!");
					identifierType = nullptr;
					goto failed;
					break;
				}
				}

				memberTypes.push_back(identifierType->llvmType);
			}

			// retrieve member name and add it to memberNames map
			const std::string_view memberName = id.first->Value;

			memberNames[memberName] = { memberIndex++, identifierType };
		}

		enumType = llvm::StructType::create(*state.Context, memberTypes, enumName, true);

		if (!enumType)
		{
			logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
				"Invalid enum type for enum '{0}'!", enumName);
			goto failed;
		}

		state.NamedValues.InsertType(enumName, AlloyType::get(enumType), NamedValues::UserDefinedType::enumeration,
			memberNames, genericTypeMap);

	failed:
		return AlloyType::get(enumType);
	}

	llvm::Function* generateFunctionDefinition(ModuleTable& moduleTable, LLVMState& state,
		AlloyType* parentType,
		const FUNCTION_DEFINITION& functionDefinition, const std::string& moduleName,
		const std::vector<TYPE*>& functionArguments,	// generic arguments, if any
		const std::vector<AlloyValue>& argumentValues
	)
	{
		//
		// Generate either a global function definition or a member function definition
		// If type is other than nullptr string, we are defining a member function, in which case the name of the function is defined as Type@Name in LLVM
		// typeMap is a map from the generic type to the actual function parameter type. On entry, it should contain the map for generic types, e.g.:
		// fn List(type T):create(const default: &T) : the map should contain what T refers to
		//
		moduleTable.PushContext(moduleName);

		GenericTypeMap typeMap;

		if (nullptr != parentType) {
			state.NamedValues.GetGenericTypeMap(state.NamedValues.GetTypeName(parentType), typeMap);
		}
		if (state.NamedValues.GetGenericTypeMap().size() > 0)
			typeMap.insert(state.NamedValues.GetGenericTypeMap().begin(), state.NamedValues.GetGenericTypeMap().end());

		// push a new scope for the function
		state.NamedValues.PushScope(functionDefinition.pFunctionNameToken->Value);

		// for generic functions, build the generic types map
		for (auto t : typeMap) {
			state.NamedValues.SetGenericType(t.first, t.second);
		}

		llvm::Function* func = generateFunctionDeclaration(moduleTable, state, parentType, functionDefinition, moduleName, functionArguments, argumentValues, typeMap);

		if (func == nullptr
			|| functionDefinition.pBody == nullptr		// this is the case for external function definitions
			)
		{
			state.NamedValues.PopScope();
			moduleTable.PopContext();
			return func;
		}

		// create a new builder for this function, this will allow us to generate multiple functions in parallel
		llvm::IRBuilder<>* PreviousBuilder = state.Builder.release();
		state.Builder.reset(new llvm::IRBuilder<>(*state.Context));

		// create a new basic block to start insertion into
		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", func);
		state.Builder->SetInsertPoint(entryBlock);

		// create an alloca for the function's return value, if any
		llvm::AllocaInst* previousReturnValue = state.CurrentReturnValue;
		if (func->getReturnType() != nullptr && !func->getReturnType()->isVoidTy()) {
			state.CurrentReturnValue = createEntryBlockAlloca(func, "", AlloyType::get(func->getReturnType()));
		}
		else {
			state.CurrentReturnValue = nullptr;
		}

		// create allocations for function arguments
		for (size_t index = 0; index < func->arg_size(); index++)
		{
			llvm::Argument* arg = func->getArg(index);

			// check that we do not have a named value with the same name
			if (state.NamedValues.GetValue(arg->getName().str(), false /*searchInParents*/))
			{
				logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID
					"Variable '{0}' already defined!", arg->getName().str());

				func->eraseFromParent();
				state.NamedValues.FreeHeapPointers(*state.Builder);
				state.NamedValues.PopScope();
				state.CurrentReturnValue = previousReturnValue;
				state.Builder.reset(PreviousBuilder);
				moduleTable.PopContext();
				return nullptr;
			}

			llvm::Attribute attr = arg->getAttribute(llvm::Attribute::AttrKind::ByRef);
			AlloyType* subType = nullptr;
			llvm::AllocaInst* allocaInst = nullptr;
			if (attr.hasAttribute(llvm::Attribute::AttrKind::ByRef)) {
				subType = AlloyType::get(arg->getParamByRefType());
			}
			// create an alloca for this variable
			AlloyType* argType = AlloyType::get(arg->getType());
			argType->containedType = subType;
			allocaInst = createEntryBlockAlloca(func, arg->getName().str(), argType);

			// store the initial value into the alloca
			state.Builder->CreateStore(arg, allocaInst);

			// add arguments to named values
			AlloyValue argVal(arg, argType, allocaInst, isFunctionParameterConst(*functionDefinition.pFunctionType, index));
			state.NamedValues.InsertValue(std::string(arg->getName()), argVal);
		}

		// every function has an exit block for cleanup and setting the return value
		llvm::BasicBlock* previousExitBlock = state.FuncExitBlock;
		state.FuncExitBlock = llvm::BasicBlock::Create(*state.Context, "exit", func);

		// check if function has any statements and generate them
		if (functionDefinition.pBody->Statements.size() > 0) {
			if (!generateStatementBlock(moduleTable, state, *functionDefinition.pBody))
			{
				ASSERT(false, "Unexpected error generating function body!");
				func->eraseFromParent();
				state.NamedValues.FreeHeapPointers(*state.Builder);
				state.NamedValues.PopScope();
				state.CurrentReturnValue = previousReturnValue;
				state.FuncExitBlock = previousExitBlock;
				state.Builder.reset(PreviousBuilder);
				moduleTable.PopContext();
				return nullptr;
			}
		}

		// insert the branch into the end of the function
		branchIfNotDuplicate(state, state.FuncExitBlock);

		// create the exit block
		state.Builder->SetInsertPoint(state.FuncExitBlock);

		// clear all pointers allocated on the heap
		state.NamedValues.FreeHeapPointers(*state.Builder);
		// restore the named values of the higher level
		state.NamedValues.PopScope();
		// set the return value if any
		if (state.CurrentReturnValue != nullptr) {
			llvm::Value* retValue = state.Builder->CreateLoad(state.CurrentReturnValue->getAllocatedType(), state.CurrentReturnValue, "returnval");
			if (retValue != nullptr) {
				state.Builder->CreateRet(retValue);
			}
			else {
				state.Builder->CreateRetVoid();
			}
		}
		else {
			state.Builder->CreateRetVoid();
		}

		// restore the previous exit block and return value
		state.FuncExitBlock = previousExitBlock;
		state.CurrentReturnValue = previousReturnValue;

		// validate the generated code, checking for consistency
		llvm::verifyFunction(*func);

		if (state.Optimizations != LLVMState::OptimizeNone)
		{
			// run the optimizer on the function.
			state.FPM->run(*func, *state.FAM);
		}

		state.Builder.reset(PreviousBuilder);
		moduleTable.PopContext();

		return func;
	}

	AlloyType* generateTypeDefinition(ModuleTable& moduleTable, LLVMState& state,
		const TYPE_DEFINITION& typeDefinition, const std::string& moduleName,
		const GenericArgumentTypes& genericArguments
	)
	{
		moduleTable.PushContext(moduleName);

		AlloyType* result = nullptr;

		if (typeDefinition.pType->Type.Is<STRUCT_TYPE>()) {
			const STRUCT_TYPE& structType = *typeDefinition.pType->Type.Get<STRUCT_TYPE>();
			result = generateStructDefinition(moduleTable, state, *typeDefinition.pTypeIdentifier, structType, genericArguments);
		}
		else if (typeDefinition.pType->Type.Is<TYPE_NAME>()) {
			const TYPE_NAME* typeName = typeDefinition.pType->Type.Get<TYPE_NAME>();

			result = getTypeFromTypeName(moduleTable, state, typeName->pNameToken, ProcessGenericArguments(moduleTable, state, typeName->GenericArguments, {}), {});

			// insert the new type (which is a copy of the type it refers to) into the namedValues of the current scope
			if (result != nullptr) {
				// search for the type definition in the module table, we need to retrieve the actual mangled name
				SearchResult<TYPE_DEFINITION> secondTypeDefinition = moduleTable.GetTypeDefinition(typeDefinition.pTypeIdentifier->pNameToken->Value);
				bool isStruct = result->llvmType->isStructTy();
				if (isStruct) {
					std::unordered_map<std::string_view, NamedValues::TypeMemberInfo> structMembers;
					state.NamedValues.GetStructMembers(state.NamedValues.GetTypeName(result), structMembers);
					state.NamedValues.InsertType(secondTypeDefinition.MangledName, result, NamedValues::UserDefinedType::structure, structMembers);
				}
				else if (secondTypeDefinition.pDefiniton->pType->Type.Is<ENUM_TYPE>()) {
					state.NamedValues.InsertType(secondTypeDefinition.MangledName, result, NamedValues::UserDefinedType::enumeration);
				}
				else {
					state.NamedValues.InsertType(secondTypeDefinition.MangledName, result, NamedValues::UserDefinedType::basic);
				}
			}
		}
		else if (typeDefinition.pType->Type.Is<ENUM_TYPE>()) {
			const ENUM_TYPE& enumType = *typeDefinition.pType->Type.Get<ENUM_TYPE>();
			result = generateEnumDefinition(moduleTable, state, *typeDefinition.pTypeIdentifier, enumType, genericArguments);
		}
		else if (typeDefinition.pType->Type.Is<ARRAY_TYPE>()) {
			const ARRAY_TYPE& arrayType = *typeDefinition.pType->Type.Get<ARRAY_TYPE>();
			result = generateArrayDefinition(moduleTable, state, *typeDefinition.pTypeIdentifier, arrayType, genericArguments, {});
		}
		else {
			ASSERT(false, "Type definition not implemented!");
		}

		moduleTable.PopContext();

		return result;
	}

#pragma endregion

	bool Generate(ModuleTable& moduleTable, LLVMState& state)
	{
		//
		// search for main entry point and start code generation
		//
		FUNCTION_DEFINITION* pMainFunction = moduleTable.GetMainFunction();
		if (pMainFunction == nullptr)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, "Cannot find main entry point!");
			return false;
		}

		// generate all the casting functions for the built-in types
		// generateAllCastFunctions(state);

		/* pre-process all types definitions as these might be used throughout the code
		* for this to work properly we need to process the types in the order they appear in the file because one type can reference a previous type
		* for now, we are processing each type the first time it is encountered in the code
		* also note that generic structures cannot be pre-processed as the generic type will not be defined
		for (auto t = moduleTable.TypeDefinitions.begin(); t != moduleTable.TypeDefinitions.end(); t++) {
			generateTypeDefinition(moduleTable, state, *t->second);
		}
		*/

		/* pre - process all function definitions leaving the main function till the end
		for (auto& [moduleName, module] : moduleTable.GetModules())
		{
			for (auto& [funcName, func] : module.GetNodeBuffer().GetFunctionDefinitions())
			{
				if (funcName != "main")
				{
					generateFunctionDefinition(moduleTable, state, *func.pDefinition, moduleName, {});
				}
			}

			}*/

		GenericTypeMap typeMap;
		llvm::Function* result = generateFunctionDefinition(moduleTable, state, nullptr, *pMainFunction, moduleTable.GetCurrentContext(), {}, {});
		state.MainFunctionName = moduleTable.GetCurrentContext() + "::main";

		std::error_code errorCode;
		llvm::raw_fd_ostream out("c:\\temp\\out.ll", errorCode);

		if (state.Optimizations == LLVMState::OptimizeModule)
		{
			// run the optimizer on the module
			state.MPM.run(*state.Module, *state.MAM);
		}
		state.Module->print(out, nullptr);

		return true;
	}

	int Execute(LLVMState& state)
	{
		//
		// Use the recommended LLJIT engine to execute the code
		// Return -1 in case of error
		//
#ifndef NO_CODE_EXECUTION
		llvm::InitializeNativeTarget();
		LLVMInitializeNativeAsmPrinter();

		// create an LLJIT instance
		auto J = llvm::orc::LLJITBuilder().create();
		if (auto E = J.takeError()) {
			ASSERT(false, "Failed creating the JIT engine!");
			Log::Error("Error creating the JIT engine: {0}", toString(std::move(E)));
			return -1;
		}

		std::unique_ptr<llvm::orc::LLJIT>& jit = *J;
		llvm::Error err = jit->addIRModule(llvm::orc::ThreadSafeModule(std::move(state.Module), std::move(state.Context)));
		if (!err) {
			// look up the JIT'd function, cast it to a function pointer, then call it.
			auto mainAddr = jit->lookup(state.MainFunctionName);
			if (auto E = mainAddr.takeError()) {
				ASSERT(false, "Cannot find main entry point {0}!", state.MainFunctionName);
				Log::Error("Error locating main entry point: {0}", toString(std::move(E)));
				return -1;
			}
			llvm::orc::ExecutorAddr& mainptr = *mainAddr;
			int (*main)() = mainptr.toPtr<int()>();
			return main();
		}
		else {
			ASSERT(false, "Failed adding module!");
			Log::Error("Error adding module");
			return -1;
		}

#else
		ASSERT(false, "Code execution is disabled!");
		return -1;
#endif
	}
}