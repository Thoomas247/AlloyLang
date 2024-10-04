#include "CodeGenerator.hpp"
#include "LibraryFunctions.hpp"
#include "Inlines.hpp"

namespace AlloyCompiler
{

#pragma region Util

	struct PtrValuePair
	{
		//
		// structure that contains the pointer to a variable and its value
		//
		llvm::Value* Ptr = nullptr;
		llvm::Value* Value = nullptr;
		bool isConst = false;	// indicates that we are pointing to constant variables
	};

	// forward declarations
	PtrValuePair generateExpression(ModuleTable& moduleTable, LLVMState& state, const EXPRESSION& expressionNode,
		TypeSubtypePair& expectedType);
	llvm::Value* generateVariableDefinition(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DEFINITION& variableDefinition);
	llvm::Value* generateStatement(ModuleTable& moduleTable, LLVMState& state, const STATEMENT& statement);
	llvm::Value* generateStatementBlock(ModuleTable& moduleTable, LLVMState& state, const STATEMENT_BLOCK& statementBlock);
	llvm::Function* generateFunctionDefinition(ModuleTable& moduleTable, LLVMState& state, llvm::Type* parentType, const FUNCTION_DEFINITION& functionDefinition, const std::string& moduleName,
		const std::vector<EXPRESSION*>& functionArguments);
	llvm::Type* generateTypeDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_DEFINITION& typeDefinition, const std::string& moduleName,
		const GenericArgumentTypes& genericArguments);
	llvm::Type* generateStructDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
		const STRUCT_TYPE& structDefinition, const GenericArgumentTypes& genericArguments);
	llvm::Type* generateEnumDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
		const ENUM_TYPE& enumDefinition, const GenericArgumentTypes& genericArguments);
	TypeSubtypePair generateArrayDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
		const ARRAY_TYPE& arrayDefinition, const GenericArgumentTypes& genericArguments,
		const GenericTypeMap& parentTypeMap);
	llvm::Function* generateExternDefinition(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_DEFINITION& externDefinition, const std::string& moduleName);
	llvm::Type* getTypeFromTypeName(ModuleTable& moduleTable, LLVMState& state, Token* pNameToken,
		const GenericArgumentTypes& genericArguments,
		const GenericTypeMap& genericTypeMap);

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
				std::string typeName(std::get<0>(t));
				if (genericTypeMap.contains(typeName))
					mangledName = mangledName + "@" + std::string(state.NamedValues.GetTypeName(genericTypeMap.at(typeName)));
				else
					mangledName = mangledName + "@" + typeName;
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
			Log::Print("    {0}{1}", std::string(location.Column - 2, ' '), std::string(tokenSize, '~'));
			Log::Print("    {0}{1}", std::string(location.Column - 2, ' '), std::vformat(format, std::make_format_args(args...)));
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
			Log::Error("    {0}{1}", std::string(location.Column - 2, ' '), std::string(tokenSize, '~'));
			Log::Error("    {0}{1}", std::string(location.Column - 2, ' '), std::vformat(format, std::make_format_args(args...)));
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

	bool convertValueToType(LLVMState& state, llvm::Value*& value, llvm::Type* newType)
	{
		//
		// Create an llvm instruction to convert input value from its current type to another type
		// TODO: 
		//		- Extend to include other needed types
		//		- Add a warning in case the conversion loses precision 
		//
		bool result = false;
		if (value != nullptr)
		{
			llvm::Type* oldType = value->getType();

			if (oldType == newType) {
				return true;	// nothing to do
			}

			switch (oldType->getTypeID()) {
			case llvm::Type::IntegerTyID:
			{
				switch (newType->getTypeID()) {
					//
					// int to float or double
					//
				case llvm::Type::FloatTyID:
				case llvm::Type::DoubleTyID:
					value = state.Builder->CreateSIToFP(value, newType);
					result = true;
					break;

					//
					// int to int of another bitsize
					//
				case llvm::Type::IntegerTyID:
					value = state.Builder->CreateIntCast(value, newType, true);
					result = true;
					break;

					//
					// int to pointer (e.g. null pointer)
					//
				case llvm::Type::PointerTyID:
					value = state.Builder->CreateIntToPtr(value, newType);
					result = true;
					break;

				default:
					ASSERT(false, "Conversion to type is not implemented");
					break;
				}
				break;
			}

			case llvm::Type::FloatTyID:
			{
				switch (newType->getTypeID()) {
					//
					// float to double
					// 
				case llvm::Type::DoubleTyID:
					value = state.Builder->CreateFPCast(value, newType);
					result = true;
					break;
				default:
					ASSERT(false, "Conversion to type is not implemented");
					break;
				}
				break;
			}

			case llvm::Type::DoubleTyID:
			{
				switch (newType->getTypeID()) {
					//
					// double to float
					//
				case llvm::Type::FloatTyID:
					value = state.Builder->CreateFPCast(value, newType);
					result = true;
					break;

				default:
					ASSERT(false, "Conversion to type is not implemented");
					break;
				}
				break;
			}

			default:
				/// ASSERT(false, "Conversion from type is not implemented");
				break;
			}
		}
		return result;
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

	GenericArgumentTypes ProcessGenericArguments(ModuleTable& moduleTable,  LLVMState& state, 
					const std::vector<TYPE*>& genericArguments,
					const GenericTypeMap& typeMap)
	{
		//
		// Given a vector of arguments of type AlloyCompiler::TYPE, convert to a vector of llvm::type(s) for these arguments
		//
		GenericArgumentTypes arguments;

		for (TYPE* argument : genericArguments) {
			if (argument->Type.Is<TYPE_NAME>()) {
				TYPE_NAME* typeName = argument->Type.Get<TYPE_NAME>();
				std::string typeNameStr(typeName->pNameToken->Value);
				llvm::Type* llvmType = nullptr;
				if (typeMap.contains(typeNameStr))
					llvmType = typeMap.at(typeNameStr);
				if (llvmType == nullptr)
				{
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
						llvmType = getTypeFromTypeName(moduleTable, state, typeName->pNameToken,
							ProcessGenericArguments(moduleTable, state, typeName->GenericArguments, typeMap),
							typeMap);
					}
				}
				arguments.push_back({ typeNameStr, llvmType });
			}
			else {
				ASSERT(false, "Expected TYPE_NAME");
				arguments.push_back({ "", nullptr });
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

	llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function, const std::string_view& varName, llvm::Type* type, int numElements = 0)
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
		result = tempBuilder.CreateAlloca(type, elements, varName);
		return result;
	}

	llvm::Type* getTypeFromTypeName(ModuleTable& moduleTable, LLVMState& state, Token* pNameToken,
		const GenericArgumentTypes& genericArguments,
		const GenericTypeMap& genericTypeMap)
	{
		//
		// Helper function to return an llvm type given its type name
		// Also searches the named nodes if the type has not been defined yet and define it
		// For generic types, genericArguments and genericTypeMap are used top map the generic type to an actual type (e.g. T to i32)
		// This function assumes that the current module has been set by the caling function
		//

		llvm::Type* type = nullptr;
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
		const std::vector<FUNCTION_PARAMETER*>& functionParameters,
		const std::vector<EXPRESSION*>& functionArguments,
		GenericTypeMap& typeMap
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
		if (!mangled.empty())
			mangled += "::";
		if (!typeName.empty()) {
			mangled += typeName;
			mangled += "@";
		}
		mangled += std::string(functionName);
		int argument = 0;
		for (FUNCTION_PARAMETER* parameter : functionParameters) {
			if (parameter->Is<GENERIC_PARAMETER>()) {
				GENERIC_PARAMETER* type = parameter->Get<GENERIC_PARAMETER>();
				EXPRESSION* exp = functionArguments[argument];
				PRIMARY* primary = nullptr;
				if (exp->Is<PRIMARY>())
					primary = exp->Get<PRIMARY>();
				if (primary == nullptr
					|| !primary->Is<VARIABLE>()
					)
				{
					// we are expecting a literal expression representing a type, nothing else
					logErrorAtCurrentPosition(moduleTable, type->pIdentifierToken, "Expected a type name.");

					mangled = "";
					goto failed;
				}

				VARIABLE* var = exp->Get<PRIMARY>()->Get<VARIABLE>();

				// check that the generic parameter has not been encountered already
				if (typeMap.contains(std::string(var->pNameToken->Value))) {
					logErrorAtCurrentPosition(moduleTable, type->pIdentifierToken, "Type {0} cannot be defined more than once.", type->pIdentifierToken->Value);
					mangled = "";
					goto failed;
				}

				mangled += "@";
				mangled += var->pNameToken->Value;

				typeMap[std::string(type->pIdentifierToken->Value)] = getTypeFromTypeName(moduleTable, state, var->pNameToken, {}, {});
			}
			argument++;
		}

	failed:
		return mangled;
	}

	std::string getExtendedFunctionName(ModuleTable& moduleTable, LLVMState& state, 
		const std::string_view& moduleName, Token* pTypeNameToken, Token* pFunctionNameToken,
		const std::vector<FUNCTION_PARAMETER*>& functionParameters,
		const std::vector<EXPRESSION*>& functionArguments,
		GenericTypeMap& typeMap
	)
	{
		return getExtendedFunctionName(moduleTable, state, moduleName,
			pTypeNameToken ? pTypeNameToken->Value : "",
			pFunctionNameToken->Value,
			functionParameters,
			functionArguments,
			typeMap
		);
	}

	bool isFunctionParameterConst(const FUNCTION_TYPE& functionType, int index)
	{
		//
		// Helper function to determine if a function parameter is declared as const
		// This function will skip any type parameter as these are not forwarded to llvm
		//
		bool result = false;
		for (int i = 0; i < functionType.Parameters.size(); i++) {
			const FUNCTION_PARAMETER* parameter = functionType.Parameters[i];

			// we are skipping the generic type parameters as these are not really function parameters as far as llvm is concerned
			if (parameter->Is<GENERIC_PARAMETER>()) {
				index++;
				continue;
			}

			if (i == index) {
				if (parameter->Is<VARIABLE_DECLARATION>()) {
					VARIABLE_DECLARATION* pParameterVariableDeclaration = parameter->Get<VARIABLE_DECLARATION>();
					result = (pParameterVariableDeclaration->VarType == VariableType::Constant);
				}
				else {
					ASSERT(false, "isFunctionParameterConst can only be called on variable parameters and not on types!");
				}
				break;
			}
		}

		return result;
	}

	bool isFunctionParameterGeneric(const FUNCTION_TYPE& functionType, int index)
	{
		//
		// Helper function to determine if a function parameter is a type
		//
		// for functions with variable parameters, we can only check the known parameters
		if (functionType.IsVarArg
			&& index >= functionType.Parameters.size()) {
			return false;
		}
		else {
			const FUNCTION_PARAMETER* parameter = functionType.Parameters[index];
			if (parameter->Is<GENERIC_PARAMETER>()) {
				return true;
			}
			else {
				return false;
			}
		}
	}
#pragma endregion

#pragma region Literals

	llvm::Value* generateLiteral(ModuleTable& moduleTable, LLVMState& state, const LITERAL& literalNode,
		const TypeSubtypePair& identifierType)
	{
		//
		// Convert a literal into an llvm value
		// identifierType.type can be null, if provided it should contain the expected type
		//
		const std::string_view literalStr = literalNode.pValueToken->Value;
		llvm::Value* result = nullptr;

		// get the correct type of value that we are generating
		llvm::Type* thisType = identifierType.type;
		if (thisType && isa<llvm::VectorType>(thisType)) {
			thisType = static_cast<llvm::VectorType*>(thisType)->getElementType();		// for arrays, we want the element type, not the array itself
		}

		// TODO: allow negative values for ints and floats
		// in the current implementation, an additional call to the unary "-" is made. This call is removed by the optimizers so there are no adverse effects

		switch (literalNode.Type)
		{
		case LiteralType::Integer:
		{
			uint64_t uintValue;
			int bits = 64;	// default conversion to 64-bit integers
			if (thisType && thisType->isIntegerTy()) {
				// if we are expecting a specific integer type, generate an integer with the right type
				bits = thisType->getIntegerBitWidth();
			}
			std::from_chars_result temp = std::from_chars(literalStr.data(), literalStr.data() + literalStr.size(), uintValue);
			result = llvm::ConstantInt::get(*state.Context, llvm::APInt(bits, uintValue));
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
			result = llvm::ConstantInt::get(*state.Context, llvm::APInt(8, literalStr[0]));
			break;

		default:
			ASSERT(false, "Unknown literal type!");
			break;
		}

		// if we know the type that we are expecting, convert right away to that type
		if (result && thisType) {
			if (isa<llvm::PointerType>(thisType) && identifierType.containedType != nullptr)
				convertValueToType(state, result, identifierType.containedType);
			else
				convertValueToType(state, result, thisType);
		}
		return result;
	}

#pragma endregion

#pragma region Identifiers

	PtrValuePair generateIdentifier(ModuleTable& moduleTable, LLVMState& state, const VARIABLE& variable,
		TypeSubtypePair& identifierType)
	{
		const std::string_view name = variable.pNameToken->Value;

		// check if we have a local variable with this name
		ValueTypePair* valueTypePair = state.NamedValues.GetValue(std::string(name));

		if (valueTypePair)
		{
			// the "Value" is an AllocaInst, load the actual value pointed to by the AllocaInst
			llvm::Value* valueOrPtr = state.Builder->CreateLoad(valueTypePair->value->getAllocatedType(),
				valueTypePair->value, name);

			// set the current type, subtype and parentType (for structs and enums)
			identifierType.type = valueOrPtr->getType();
			identifierType.parentType = valueTypePair->parentType;

			if (valueTypePair->containedType
				&& llvm::isa<llvm::PointerType>(valueOrPtr->getType())) {
				// the type is set in the case of pointers and references, we need to load the pointed to value
				llvm::Value* Value = state.Builder->CreateLoad(valueTypePair->containedType, valueOrPtr, "loadtmp");
				identifierType = { Value->getType(), nullptr };
				return PtrValuePair{ .Ptr = valueOrPtr, .Value = Value, .isConst = valueTypePair->isConst };
			}
			else {
				identifierType.containedType = valueTypePair->containedType;
				return PtrValuePair{ .Ptr = valueTypePair->value, .Value = valueOrPtr, .isConst = valueTypePair->isConst };
			}
		}

		// check if we have a global
		llvm::GlobalVariable* globalVar = state.Module->getGlobalVariable(name, true);

		if (globalVar)
		{
			llvm::Value* value = state.Builder->CreateLoad(globalVar->getValueType(), globalVar, name);
			// TODO: we need to handle const globals
			return PtrValuePair{ .Ptr = globalVar, .Value = value };
		}

		// check if the identifier is a function and return a pointer to that function
		SearchResult<FUNCTION_DEFINITION> funcResult = moduleTable.GetFunctionDefinition(name);

		if (funcResult.Code == SearchResultCode::Found)
		{
			// look up the function in the global module table
			llvm::Function* calleeFunc = state.Module->getFunction(funcResult.MangledName);
			if (calleeFunc == nullptr) {
				calleeFunc = generateFunctionDefinition(moduleTable, state, nullptr, *funcResult.pDefiniton, funcResult.ModuleName, {});
			}

			if (calleeFunc != nullptr) {
				identifierType = { llvm::PointerType::get(*state.Context, 0), nullptr };
				return PtrValuePair{ .Ptr = nullptr, .Value = calleeFunc, .isConst = false };
			}
		}

		logErrorAtCurrentPosition(moduleTable, variable.pNameToken, "Unknown variable name '{0}'!", name);
		return {};
	}

	TypeSubtypePair generateTypeIdentifier(ModuleTable& moduleTable, LLVMState& state, const TYPE& typeIdentifier,
		Token* pParentTypeNameToken, TypeModifier& modifier,
		const GenericTypeMap& genericTypeMap)
	{
		//
		// genericTypeMap maps from the generic type names to the actual type names, can be empty
		//
		TypeSubtypePair identifierType = { nullptr, nullptr };
		GenericArgumentTypes genericArgumentTypes;

		// let the caller know if this is a pointer, a reference or none
		modifier = typeIdentifier.Modifier;

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
					identifierType = { nullptr, nullptr };
					goto exit;
				}
				pNameToken = pParentTypeNameToken;
			}

			genericArgumentTypes = ProcessGenericArguments(moduleTable, state, typeName.GenericArguments, genericTypeMap);
			identifierType.type = getTypeFromTypeName(moduleTable, state, pNameToken, 
														genericArgumentTypes,
														genericTypeMap);

			if (!identifierType.type)
			{
				logErrorAtCurrentPosition(moduleTable, typeName.pNameToken,
					"Unknown type name '{0}'!", pNameToken->Value);
				identifierType = { nullptr, nullptr };
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
			identifierType.type = generateStructDefinition(moduleTable, state, ti, type, genericArgumentTypes);
		}
		// handle ènum types
		else if (typeIdentifier.Type.Is<ENUM_TYPE>())
		{
			const ENUM_TYPE& type = *typeIdentifier.Type.Get<ENUM_TYPE>();
			TYPE_IDENTIFIER ti = { nullptr, {} };
			identifierType.type = generateEnumDefinition(moduleTable, state, ti, type, genericArgumentTypes);
		}
		else {
			ASSERT(false, "Unknown type definition!");
		}

	exit:
		return identifierType;
	}

#pragma endregion

#pragma region Declarations

	TypeSubtypePair generateTypeDeclaration(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_TYPE& typeDeclarationNode)
	{
		// TODO: var and const

		TypeModifier modifier = TypeModifier::None;
		if (typeDeclarationNode.pReturnType->pType == nullptr) {
			return {};
		}
		else {
			return generateTypeIdentifier(moduleTable, state, *typeDeclarationNode.pReturnType->pType, nullptr, modifier, state.NamedValues.GetGenericTypeMap());
		}
	}

	llvm::Value* generateVariableDeclaration(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DECLARATION& variableDeclarationNode,
		TypeSubtypePair& identifierType)
	{
		const std::string_view name = variableDeclarationNode.pNameToken->Value;
		TypeModifier modifier = TypeModifier::None;
		GenericTypeMap genericTypeMap;

		// if the type is already known, i.e. inferred from the expression's value, we do not try get the type again
		if (nullptr == identifierType.type) {
			identifierType = generateTypeIdentifier(moduleTable, state, *variableDeclarationNode.pType, nullptr, modifier, genericTypeMap);

			if (!identifierType.type)
			{
				logErrorAtCurrentPosition(moduleTable, variableDeclarationNode.pNameToken, "Variable '{0}' type error!", name);
				return nullptr;
			}

			switch (modifier) {
			case TypeModifier::None:
				break;

			case TypeModifier::Pointer:
			{
				// allocating a pointer to this type
				if (!llvm::isa<llvm::PointerType>(identifierType.type)) {
					identifierType.containedType = identifierType.type;
					identifierType.type = llvm::PointerType::get(identifierType.containedType, 0);
				}
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
			identifierType.type
		);

		// add the variable to the named values
		state.NamedValues.InsertValue(std::string(name), allocaInst, identifierType.containedType,
			identifierType.parentType,
			(variableDeclarationNode.VarType == VariableType::Constant),
			(modifier == TypeModifier::Pointer));

		return allocaInst;
	}

	llvm::Function* generateFunctionDeclaration(ModuleTable& moduleTable, LLVMState& state,
		const llvm::Type* parentType,
		const FUNCTION_DEFINITION& functionDeclarationNode, const std::string& moduleName,
		const std::vector<EXPRESSION*>& functionArguments,
		GenericTypeMap& typeMap		// map from the generic type to the actual function parameter type
	)
	{
		//
		// If type is not nullptr, we are generating a member function in the form of Type@Name
		// If the parameter list contains types (generic functions), also add the type names and function arguments to the mangled name
		//
		std::string name = getExtendedFunctionName(
			moduleTable, state,
			((parentType != nullptr) ? "" : moduleName),	// when parentType is provided, GetTypeName will already include the module name so don't repeat it here
			((parentType != nullptr) ? state.NamedValues.GetTypeName(parentType) : ""),
			functionDeclarationNode.pFunctionNameToken->Value,
			functionDeclarationNode.pFunctionType->Parameters,
			functionArguments,
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
		std::vector<llvm::Type*> paramSubTypes;
		std::vector<TypeModifier> paramModifiers;
		std::vector< VariableType> paramVarTypes;

		////////////////////////////////////////////////////
		// note change from VARIABLE_DECLARATION 		  //
		// to FUNCTION_PARAMETER for variable 'parameter' //
		// this is to add support for generics			  //
		////////////////////////////////////////////////////
		for (FUNCTION_PARAMETER* parameter : functionDeclarationNode.pFunctionType->Parameters)
		{
			if (parameter->Is<GENERIC_PARAMETER>()) {
				// nothing to do at this point... processing the type later when it is used
			}
			else {

				ASSERT(parameter->Is<VARIABLE_DECLARATION>(), "If not a generic parameter, this must be a variable declaration!");
				VARIABLE_DECLARATION* pParameterVariableDeclaration = parameter->Get<VARIABLE_DECLARATION>();

				TypeModifier modifier = TypeModifier::None;
				Location location(0, 0, 0);
				Token tok{ ((parentType != nullptr) ? state.NamedValues.GetTypeName(parentType) : ""), location, TokenKind::string_literal };
				TypeSubtypePair identifierType = generateTypeIdentifier(moduleTable, state, *pParameterVariableDeclaration->pType,
					((parentType != nullptr) ? &tok : nullptr),
					modifier, typeMap);

				if (!identifierType.type)
				{
					logErrorAtCurrentPosition(moduleTable, pParameterVariableDeclaration->pNameToken, "Function '{0}' parameter type error!", name);
					return nullptr;
				}

				// References should be passed as pointers
				if (modifier == TypeModifier::Reference || modifier == TypeModifier::Pointer) {
					paramTypes.push_back(llvm::PointerType::get(identifierType.type, 0));
					paramSubTypes.push_back(identifierType.type);
				}
				else {
					paramTypes.push_back(identifierType.type);
					paramSubTypes.push_back(nullptr);
				}
				paramModifiers.push_back(modifier);
				paramVarTypes.push_back(pParameterVariableDeclaration->VarType);
			}
		}

		// retrieve the return types
		llvm::Type* returnType = llvm::Type::getVoidTy(*state.Context);

		if (functionDeclarationNode.pFunctionType != nullptr
			&& functionDeclarationNode.pFunctionType->pReturnType != nullptr)
		{
			returnType = generateTypeDeclaration(moduleTable, state, *functionDeclarationNode.pFunctionType).type;
		}

		if (!returnType)
		{
			logErrorAtCurrentPosition(moduleTable, functionDeclarationNode.pFunctionNameToken, "Function '{0}' return type error!", name);
			return nullptr;
		}

		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, functionDeclarationNode.pFunctionType->IsVarArg);

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
		for (size_t i = 0, arg = 0; i < functionDeclarationNode.pFunctionType->Parameters.size(); i++)
		{
			FUNCTION_PARAMETER* parameter = functionDeclarationNode.pFunctionType->Parameters[i];
			if (parameter->Is<GENERIC_PARAMETER>()) {
				// nothing to do at this point...
			}
			else {

				ASSERT(parameter->Is<VARIABLE_DECLARATION>(), "If not a generic parameter, this must be a variable declaration!");
				const std::string_view paramName = parameter->Get<VARIABLE_DECLARATION>()->pNameToken->Value;

				function->getArg(arg)->setName(paramName);

				// set the ByRef attribute on parameters passed byref
				if (paramModifiers[arg] == TypeModifier::Reference) {
					function->addAttributeAtIndex(arg + 1, llvm::Attribute::getWithByRefType(*state.Context, paramSubTypes[arg]));
				}

				/* ReadOnly is not the same as const and LLVM does not accept ReadOnly on anything other than pointers
				// set the ReadOnly attribute on parameters passed as const
				if (paramVarTypes[arg] == VariableType::Constant) {
					function->addAttributeAtIndex(arg+1, llvm::Attribute::get(*state.Context, llvm::Attribute::AttrKind::ReadOnly));
				}
				*/

				arg++;
			}
		}

		return function;
	}

#pragma endregion

#pragma region Expressions

	llvm::Value* generateConstructorExpression(ModuleTable& moduleTable, LLVMState& state, const CONSTRUCTOR& constructorExpression)
	{
		// search for the struct definition in the module table
		std::string structName;
		GenericTypeMap genericTypeMap = state.NamedValues.GetGenericTypeMap();
		GenericArgumentTypes genericArgumentTypes = ProcessGenericArguments(moduleTable, state, constructorExpression.pType->GenericArguments, genericTypeMap);
		llvm::Type* type = getTypeFromTypeName(moduleTable, state, constructorExpression.pType->pNameToken,
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
			return nullptr;
		}

		llvm::StructType* structType = static_cast<llvm::StructType*>(type);
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

			NamedValues::TypeMemberInfo memberInfo = state.NamedValues.GetStructMemberIndex(structName, memberName);

			if (memberInfo.memberIndex == -1)
			{
				logErrorAtCurrentPosition(moduleTable, constructorExpression.pType->pNameToken,
					"Type '{0}' is not a struct!", structName);
				return nullptr;
			}

			if (memberInfo.memberIndex == -2)
			{
				logErrorAtCurrentPosition(moduleTable, constructorExpression.pType->pNameToken,
					"Struct '{0}' does not have a member '{1}'!", structName, memberName);
				return nullptr;
			}

			// set the type that we are expecting for each structure element
			TypeSubtypePair identifierType = { structType->getElementType(memberInfo.memberIndex), memberInfo.containedType };

			llvm::Value* expressionVal = generateExpression(moduleTable, state, *constructorExpression.Arguments[i].second, identifierType).Value;

			if (!expressionVal)
			{
				logErrorAtCurrentPosition(moduleTable, nullptr, // nodeID
					"Error evaluating '{0}.{1}'!", structName, memberName);
				return nullptr;
			}

			if (llvm::isa<llvm::PointerType>(structType->getElementType(memberInfo.memberIndex))) {
				// TBD: check if special processing is needed in case of pointers (e.g. strings?)
			}

			llvm::Value* memberPtr = state.Builder->CreateStructGEP(structType, structPtr, memberInfo.memberIndex, memberName);

			convertValueToType(state, expressionVal, structType->getElementType(memberInfo.memberIndex));

			state.Builder->CreateStore(expressionVal, memberPtr, "savetmp");
		}

		return state.Builder->CreateLoad(structPtr->getAllocatedType(), structPtr);	// result contains the whole initialized structure
	}

	llvm::Value* generatePointerInitializerExpression(ModuleTable& moduleTable, LLVMState& state, const POINTER_INIT& pointerInitializerNode,
		const TypeSubtypePair& identifierType)
	{
		//
		// new ( EXPRESSION | ( [ EXPRESSION; EXPRESSION ] ) ) ;
		//
		llvm::Type* elementType = identifierType.type;
		llvm::Type* subElementType = identifierType.containedType;
		if (isa<llvm::PointerType>(elementType))
		{
			// for pointers, we need the underlying object type
			elementType = identifierType.containedType;
		}

		if (elementType == nullptr) {
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD *pointerInitializerNode... 
				"Unknown element type!");
			return nullptr;
		}

		// if this is a pointer to an array, we need the underlying element type
		if (llvm::isa<llvm::VectorType>(elementType)) {
			subElementType = static_cast<llvm::VectorType*>(elementType)->getElementType();
		}
		llvm::PointerType* pointerType = llvm::PointerType::get(elementType, 0);	// note that elementType is not actually stored by llvm

		// get the value to set for each element
		TypeSubtypePair temp = { elementType, nullptr };
		llvm::Value* defaultValue = generateExpression(moduleTable, state, *pointerInitializerNode.pValue, temp).Value;
		// try to convert the value to the expected type	
		convertValueToType(state, defaultValue, subElementType);

		// now get the size of the vector to create
		llvm::Value* count;
		if (nullptr == pointerInitializerNode.pSize) {
			// creating a single object
			count = llvm::ConstantInt::get(*state.Context, llvm::APInt(64, 1, true));
		}
		else {
			TypeSubtypePair temp = { llvm::IntegerType::getInt64Ty(*state.Context), nullptr };
			count = generateExpression(moduleTable, state, *pointerInitializerNode.pSize, temp).Value;
		}

		// create a mutable variable on the heap
		llvm::Type* PointerType = llvm::Type::getInt64Ty(*state.Context);	// pointers are 64-bit values
		llvm::Constant* AllocSize = llvm::ConstantExpr::getSizeOf(subElementType);
		AllocSize = llvm::ConstantExpr::getTruncOrBitCast(AllocSize, PointerType);
		llvm::CallInst* Ptr = state.Builder->CreateMalloc(PointerType, subElementType, AllocSize, count);

		// create code to go through the list of members and initialize them to the given value
		// we need to create the loop within the llvm code because we don't know beforehand how many objects we are creating
		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();
		ASSERT(func != nullptr, "No function to insert into!");

		llvm::IRBuilder<>& tempBuilder = *state.Builder;

		// emit init code before the loop
		// start is the loop variable, initialize it to 0
		llvm::AllocaInst* start = tempBuilder.CreateAlloca(llvm::IntegerType::getInt64Ty(*state.Context), nullptr);
		tempBuilder.CreateStore(llvm::ConstantInt::get(*state.Context, llvm::APInt(64, 0)), start);
		llvm::Value* step = llvm::ConstantInt::get(*state.Context, llvm::APInt(64, 1));

		// create a new basic block to start insertion into
		llvm::BasicBlock* loopBlock = llvm::BasicBlock::Create(*state.Context, "loop", func);
		// insert an explicit fall through from the current block to the loopBlock
		tempBuilder.CreateBr(loopBlock);

		// start insertion into the loopBlock
		tempBuilder.SetInsertPoint(loopBlock);

		// generate the body of the loop

		// create the instructions to store the value for each element
		llvm::Value* current = tempBuilder.CreateLoad(start->getAllocatedType(), start);
		llvm::Value* memberPtr = tempBuilder.CreateGEP(subElementType, Ptr, current, "pointerinit");
		tempBuilder.CreateStore(defaultValue, memberPtr);

		// increment loop variable (start) by step value
		current = tempBuilder.CreateAdd(current, step);
		tempBuilder.CreateStore(current, start);

		// emit the condition
		llvm::Value* conditionVal = tempBuilder.CreateICmpUGE(current, count);
		conditionVal = convertToBool(state, conditionVal);

		// create the "after loop" block and insert it
		llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(*state.Context, "afterloop", func);

		// insert the conditional branch into the end of afterBlock
		tempBuilder.CreateCondBr(conditionVal, afterBlock, loopBlock);

		// any new code will be inserted in afterBlock
		tempBuilder.SetInsertPoint(afterBlock);

		return Ptr;	// return the initialized pointer
	}

	llvm::Value* generatePointerMoveExpression(ModuleTable& moduleTable, LLVMState& state, const POINTER_MOVE& pointerMoveNode,
		TypeSubtypePair& expectedType)
	{
		//
		// move Identifier ;
		//
		PtrValuePair ptrValue = generateIdentifier(moduleTable, state, *pointerMoveNode.pVariable, expectedType);

		// retrieve the name of the identifier in the right-side node in order to remove it from the NamedValues map
		const Token& rightNode = *pointerMoveNode.pVariable->pNameToken;
		const std::string name(rightNode.Value);
		state.NamedValues.RemoveValue(name);

		return ptrValue.Ptr;
	}

	llvm::Value* generateInitializerListExpression(ModuleTable& moduleTable, LLVMState& state, const INITIALIZER_LIST& initListExpressionNode,
		TypeSubtypePair& expectedType)
	{
		//
		// Array initialization in the form of: { EXPRESSION, EXPRESSION, ... }
		//

		if (!expectedType.type || (expectedType.type->getTypeID() != llvm::Type::FixedVectorTyID && expectedType.type->getTypeID() != llvm::Type::ScalableVectorTyID))
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: initListExpressionNode...
				"Unknown vector type!");
			return nullptr;
		}

		llvm::VectorType* vectorType = static_cast<llvm::VectorType*>(expectedType.type);

		// create a mutable variable at the end of the insertion block
		llvm::IRBuilder<> tempBuilder(state.Builder->GetInsertBlock(), state.Builder->GetInsertBlock()->end());
		llvm::AllocaInst* arrayPtr = tempBuilder.CreateAlloca(vectorType, nullptr);

		// go through the list of initializers and initialize all members
		for (int i = 0; i < initListExpressionNode.Values.size(); i++)
		{
			// set the type that we are expecting for each array element
			TypeSubtypePair identifierType = { vectorType->getElementType(), expectedType.containedType };

			llvm::Value* expressionVal = generateExpression(moduleTable, state, *initListExpressionNode.Values[i], identifierType).Value;

			if (!expressionVal)
			{
				logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: initListExpressionNode.Values[i]...
					"Error evaluating expression!");
				return nullptr;
			}

			// convert our index to the format required by GEP operations
			std::vector<llvm::Value*> indices = getGEPIndex(state, i);

			llvm::Value* memberPtr = state.Builder->CreateGEP(vectorType, arrayPtr, indices, "memberptr");

			state.Builder->CreateStore(expressionVal, memberPtr);
		}

		return state.Builder->CreateLoad(arrayPtr->getAllocatedType(), arrayPtr);	// result contains the whole initialized array
	}

	llvm::Value* generateVariableDefinitionExpression(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DEFINITION& variableDefinition)
	{
		//
		// ( var | const ) identifier:TYPE = expression;
		//
		TypeSubtypePair identifierType = { nullptr, nullptr };
		llvm::Value* declarationVal = nullptr;
		llvm::Value* value = nullptr;

		if (nullptr == variableDefinition.pDeclaration->pType)
		{
			// case where the type is not given but has to be inferred from the type of the expression

			// create the value expression
			value = generateExpression(moduleTable, state, *variableDefinition.pValue, identifierType).Value;

			if (value)
			{
				// create the declaration of a given type
				identifierType.type = value->getType();
				declarationVal = generateVariableDeclaration(moduleTable, state, *variableDefinition.pDeclaration, identifierType);
			}
		}
		else
		{
			// case where the type of the variable is given, the expression should return a value of compatible type

			// create the declaration
			declarationVal = generateVariableDeclaration(moduleTable, state, *variableDefinition.pDeclaration, identifierType);

			if (declarationVal)
			{
				// create the value expression
				value = generateExpression(moduleTable, state, *variableDefinition.pValue, identifierType).Value;

				if (value)
				{
					// try to convert the value to the right variable type
					if (llvm::isa<llvm::AllocaInst>(declarationVal)) {
						llvm::AllocaInst* alloc = static_cast<llvm::AllocaInst*>(declarationVal);
						convertValueToType(state, value, alloc->getAllocatedType());
					}
				}
			}
		}

		if (nullptr == value || nullptr == declarationVal)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD 
				"Error evaluating expression!");
			return nullptr;
		}
		else
		{
			// store the value into the alloca
			return state.Builder->CreateStore(value, declarationVal);
		}
	}

	PtrValuePair generateArrayAccessExpression(ModuleTable& moduleTable, LLVMState& state, const ARRAY_ACCESS& arrayAccessExpression,
		TypeSubtypePair& identifierType)
	{
		TypeSubtypePair i64Type = { llvm::IntegerType::getInt64Ty(*state.Context), nullptr };
		llvm::Value* memberIndex = generateExpression(moduleTable, state, *arrayAccessExpression.pIndex, i64Type).Value;
		PtrValuePair left = { nullptr, nullptr, false };
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
			left.Value = generateExpression(moduleTable, state, *arrayAccessExpression.pArray, identifierType).Value;
		}

		// get the type of the left
		llvm::Type* leftType = left.Value->getType();

		if (leftType->getTypeID() == llvm::Type::FixedVectorTyID || leftType->getTypeID() == llvm::Type::ScalableVectorTyID) {

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
		else if (leftType->getTypeID() == llvm::Type::PointerTyID
			&& identifierType.containedType != nullptr) {
			// case where the value contains a pointer			
			memberPtr = state.Builder->CreateGEP(identifierType.containedType, left.Value, memberIndex, "memberptr");
			memberValue = state.Builder->CreateLoad(identifierType.containedType, memberPtr);
		}
		else {
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: arrayAccessExpression.ArrayExpressionID
				"Expected vector type!");
			return {};
		}

		// array of pointers, we need to load the underlying value
		if (llvm::isa<llvm::PointerType>(memberValue->getType())) {
			if (identifierType.containedType == nullptr) {
				logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: moduleTable.GetErrorInfo(arrayAccessExpression.ArrayExpressionID)
					"Array contains pointers of unknown type!");
				return {};
			}

			llvm::Value* Value = state.Builder->CreateLoad(identifierType.containedType, memberValue, "loadtmp");

			return PtrValuePair{ .Ptr = memberValue, .Value = Value, .isConst = left.isConst };
		}

		return PtrValuePair{ .Ptr = memberPtr, .Value = memberValue, .isConst = left.isConst };
	}

	PtrValuePair generateMemberAccessExpression(ModuleTable& moduleTable, LLVMState& state, const MEMBER_ACCESS& memberAccessExpression,
		TypeSubtypePair& expectedType)
	{
		const std::string_view memberName = memberAccessExpression.pMemberNameToken->Value;

		PtrValuePair left;

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
		llvm::Type* leftType = left.Value->getType();

		if (leftType->getTypeID() != llvm::Type::StructTyID)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: moduleTable.GetErrorInfo(memberAccessExpressionNode.LeftID)
				"Expected struct type!");
			return {};
		}

		llvm::StructType* structType = static_cast<llvm::StructType*>(leftType);

		// get the name of the struct type
		const std::string_view structName = structType->getName();

		// get the index of the member
		NamedValues::TypeMemberInfo memberInfo = state.NamedValues.GetStructMemberIndex(structName, memberName);

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

		llvm::Type* memberType = structType->getTypeAtIndex(memberInfo.memberIndex);

		if (left.Ptr == nullptr) {
			// case where the structure is returned by evaluating an expression, we can directly access the structure member
			std::vector<unsigned int> indices(1);
			indices[0] = memberInfo.memberIndex;
			llvm::Value* memberValue = state.Builder->CreateExtractValue(left.Value, indices);
			left = PtrValuePair{ .Ptr = nullptr, .Value = memberValue, .isConst = left.isConst };
		}
		else {
			llvm::Value* memberPtr = state.Builder->CreateStructGEP(structType, left.Ptr, memberInfo.memberIndex, memberName);
			llvm::Value* memberValue = state.Builder->CreateLoad(memberType, memberPtr, "loadtmp");
			left = PtrValuePair{ .Ptr = memberPtr, .Value = memberValue, .isConst = left.isConst };
		}

		// return the type and subtype of the structure element to the caller
		expectedType.type = memberType;
		expectedType.containedType = memberInfo.containedType;

		return left;
	}

	PtrValuePair generateEnumValue(ModuleTable& moduleTable, LLVMState& state, const ENUM_VALUE& enumValueExpression,
		TypeSubtypePair& expectedType)
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
		PtrValuePair result = {};
		llvm::Value* memberPtr = nullptr;
		llvm::Type* EnumPayloadStruct = nullptr;

		// generate or retrieve the llvm type for this enumeration
		llvm::Type* type = getTypeFromTypeName(moduleTable, state, enumValueExpression.pEnumName->pNameToken, 
			ProcessGenericArguments(moduleTable, state, enumValueExpression.pEnumName->GenericArguments, {}), {});

		if (nullptr == type) {
			logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumName->pNameToken, "Type {0} not found!", enumValueExpression.pEnumName->pNameToken->Value);
			goto failed;
		}

		// get the mangled name
		enumName = state.NamedValues.GetTypeName(type);

		memberInfo = state.NamedValues.GetEnumMemberIndex(enumName, memberName);
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

		if (nullptr == memberInfo.containedType && enumValueExpression.pPayloadValue != nullptr) {
			logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumName->pNameToken,
				"Payload specified for enum member '{0}' when member cannot have a payload!", memberName);
			goto failed;
		}

		// create an alloca to a structure of type EnumPayloadStruct
		// and fill the structure with the enum index and payload value if any
		EnumPayloadStruct = state.NamedValues.GetEnumPayloadStruct(*state.Context, memberInfo.containedType);
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
		expectedType.type = EnumPayloadStruct;
		expectedType.containedType = memberInfo.containedType;	// this is the type of payload, can be null
		expectedType.parentType = type;

		// store the index of the enum value in the first element of EnumPayloadStruct
		memberPtr = state.Builder->CreateStructGEP(EnumPayloadStruct, result.Ptr, EnumPayloadIndex);
		state.Builder->CreateStore(llvm::ConstantInt::get(*state.Context, llvm::APInt(64, memberInfo.memberIndex)), memberPtr, "savepayload");

		if (enumValueExpression.pPayloadValue != nullptr) {
			// compute the value of the payload
			TypeSubtypePair expressionType = { expectedType.containedType, nullptr, nullptr };
			llvm::Value* expressionVal = generateExpression(moduleTable, state, *enumValueExpression.pPayloadValue, expressionType).Value;

			if (!expressionVal)
			{
				logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumValueNameToken,
					"Error evaluating '{0}.{1}'!", enumName, memberName);
				goto failed;
			}

			// store the payload in the second element of EnumPayloadStruct
			memberPtr = state.Builder->CreateStructGEP(EnumPayloadStruct, result.Ptr, EnumPayloadValue);
			state.Builder->CreateStore(expressionVal, memberPtr, "savepayload");
		}
		else {
			// store a null value for the payload in the second element of EnumPayloadStruct
			memberPtr = state.Builder->CreateStructGEP(EnumPayloadStruct, result.Ptr, EnumPayloadValue);
			state.Builder->CreateStore(llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*state.Context)), memberPtr, "savepayload");
		}

		// store the structure type ID in the third element of EnumPayloadStruct
		memberPtr = state.Builder->CreateStructGEP(EnumPayloadStruct, result.Ptr, EnumPayloadEnumID);
		state.Builder->CreateStore(llvm::ConstantInt::get(*state.Context, llvm::APInt(64, state.NamedValues.GetID(expectedType.type))), memberPtr, "savepayload");

		result.Value = state.Builder->CreateLoad(EnumPayloadStruct, result.Ptr, "loadpayload");

	failed:
		return result;
	}

	PtrValuePair generateEnumValueIndex(ModuleTable& moduleTable, LLVMState& state, const ENUM_VALUE& enumValueExpression,
		TypeSubtypePair& expectedType)
	{
		//
		// This function is equivalent to generateEnumValue except that it returns the index of the enum value and not the payload structure
		//
		const std::string_view memberName = enumValueExpression.pEnumValueNameToken->Value;
		std::string_view enumName = enumValueExpression.pEnumName->pNameToken->Value;
		NamedValues::TypeMemberInfo memberInfo;
		int memberIndex = -1;
		PtrValuePair result = {};
		llvm::Value* memberPtr = nullptr;

		// generate or retrieve the llvm type for this enumeration
		llvm::Type* type = getTypeFromTypeName(moduleTable, state, enumValueExpression.pEnumName->pNameToken, 
			ProcessGenericArguments(moduleTable, state, enumValueExpression.pEnumName->GenericArguments, {}), {});

		if (nullptr == type) {
			logErrorAtCurrentPosition(moduleTable, enumValueExpression.pEnumName->pNameToken, "Type {0} not found!", enumValueExpression.pEnumName->pNameToken->Value);
			goto failed;
		}

		// get the mangled name
		enumName = state.NamedValues.GetTypeName(type);

		memberInfo = state.NamedValues.GetEnumMemberIndex(enumName, memberName);
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
		expectedType.type = llvm::Type::getInt64Ty(*state.Context);		// the index is return in 64-bit int format
		expectedType.containedType = memberInfo.containedType;	// this is the type of payload, can be null
		expectedType.parentType = type;

		result.Value = llvm::ConstantInt::get(*state.Context, llvm::APInt(64, memberInfo.memberIndex));

	failed:
		return result;
	}

	llvm::Value* generateFunctionCallExpression(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_CALL& functionCallExpressionNode)
	{
		std::string functionName(functionCallExpressionNode.pFunctionNameToken->Value);
		std::string mangledName(functionName);
		std::vector<llvm::Value*> args;
		llvm::Value* result = nullptr;
		llvm::Function* calleeFunc = nullptr;
		FUNCTION_TYPE* pCalleeFunctionType = nullptr;	// in addition to the LLVM function definition, we need the original function definition in order to properly handle generic and const parameters
		enum { None, Reference, Value } insertSelfAsFirstParam = None;		// indicates whether the first parameter should be the variable value in the case of member function calls
		std::vector<EXPRESSION*> Arguments(functionCallExpressionNode.Arguments);	// creating a copy of the arguments as we might need to insert new elements
		EXPRESSION self;	// this is a fake expression used as a placeholder for Self as first parameter
		SearchResult<FUNCTION_DEFINITION> funcResult;
		llvm::Type* type = nullptr;
		GenericTypeMap typeMap;

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

		if (functionCallExpressionNode.pObject != nullptr)
		{
			bool isGenericType = (functionCallExpressionNode.pTypeOrVariableName && functionCallExpressionNode.pTypeOrVariableName->GenericArguments.size() > 0);

			if (functionCallExpressionNode.pObject && functionCallExpressionNode.pObject->Is<VARIABLE>())
			{
				// non-static member function call
				VARIABLE* var = functionCallExpressionNode.pObject->Get<VARIABLE>();
				std::string varOrTypeName(var->pNameToken->Value);
				ValueTypePair* val = state.NamedValues.GetValue(varOrTypeName);
				if (!isGenericType && val) {
					// variable found
					type = val->value->getAllocatedType();
					std::string typeName(state.NamedValues.GetTypeName(type));
					// extract any generic parameters from the type name, otherwise we cannot locate the member function
					size_t pos = typeName.find('@');
					if (pos != std::string::npos) {
						typeName = typeName.substr(0, pos);
					}
					mangledName = NodeBuffer::GetMangledName("", typeName, functionName);
					// insert Self as a first argument
					insertSelfAsFirstParam = Reference;
					Arguments.insert(Arguments.begin(), &self);
				}
				else {
					// not a variable, check if a valid type and make a static function call
					GenericArgumentTypes genericArguments;
					if (functionCallExpressionNode.pTypeOrVariableName)
						genericArguments = ProcessGenericArguments(moduleTable, state, functionCallExpressionNode.pTypeOrVariableName->GenericArguments, typeMap);
					type = getTypeFromTypeName(moduleTable, state, var->pNameToken, genericArguments, typeMap);
					if (type != nullptr) {
						// static member function call
						mangledName = NodeBuffer::GetMangledName("", varOrTypeName, functionName);
					}
					else {
						logErrorAtCurrentPosition(moduleTable, var->pNameToken, "'{0}' is not a variable or type name.", varOrTypeName);
						goto error;
					}
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

			// look up the function in the global module table
			// for generic functions, we need the full function name including any generic parameters
			std::string extendedName = getExtendedFunctionName(moduleTable, state, "",
				type == nullptr ? "" : std::string(state.NamedValues.GetTypeName(type)),
				type == nullptr ? funcResult.MangledName : functionName,
				funcResult.pDefiniton->pFunctionType->Parameters, Arguments, typeMap);

			// make sure the built-in function has already been generated
			if (funcResult.Code == SearchResultCode::BuiltIn) {
#ifndef FIRST_PARAMETER_BYREF
				insertSelfAsFirstParam = Value;
#endif
				generateBuiltInFunction(state, extendedName);
			}

			calleeFunc = state.Module->getFunction(extendedName);
		}

		// function not found, it might not have been processed yet
		// check if function is already in the parser and process it
		if (!calleeFunc)
		{
			calleeFunc = generateFunctionDefinition(moduleTable, state, type, *funcResult.pDefiniton, funcResult.ModuleName, Arguments);

			// ASSERT(Arguments.size() > 0 || calleeFunc == state.Module->getFunction(funcResult.MangledName), "Function was not generated with the right name!");
		}

		if (!calleeFunc
			|| !pCalleeFunctionType)
		{
			logErrorAtCurrentPosition(moduleTable, functionCallExpressionNode.pFunctionNameToken, "Cannot find function '{0}'!", functionName);
			goto error;
		}

		/* if the function was found, check for argument count mismatch (not counting the additional arguments that we added)
		* this test does not work with generic functions as the type parameters are not actual parameters
		* a more accurate test in done in the loop below
		if (!calleeFunc->isVarArg() && calleeFunc->arg_size() != Arguments.size() + argi)
		{
			logErrorAtCurrentPosition(moduleTable, functionCallExpressionNode.pFunctionNameToken, "Function '{0}' argument mismatch!", functionName);
			goto error;
		}
		*/

		// evaluate all the arguments
		for (size_t argi = 0; argi < Arguments.size(); argi++)
		{
			// first parameter is &Self
			if (argi == 0 && insertSelfAsFirstParam != None)
			{
				VARIABLE* var = functionCallExpressionNode.pObject->Get<VARIABLE>();
				TypeSubtypePair identifierType = {};
				PtrValuePair ptrValue = generateIdentifier(moduleTable, state, *var, identifierType);
				if (ptrValue.Ptr == nullptr)
				{
					logErrorAtCurrentPosition(moduleTable, var->pNameToken, "Error evaluating variable '{0}'!", var->pNameToken->Value);
					goto error;
				}

				args.push_back(insertSelfAsFirstParam == Reference ? ptrValue.Ptr : ptrValue.Value);
				continue;
			}

			const EXPRESSION& argument = *Arguments[argi];

			// for functions with a variable number of arguments, check the argument types till the first optional argument
			// e.g. if the function has 2 mandatory arguments and a number of optional arguments, check for only the first 2 types 
			TypeSubtypePair argType = { (argi < calleeFunc->arg_size() ? calleeFunc->getArg(argi)->getType() : nullptr), nullptr };

			llvm::Value* argVal = nullptr;

			// check if this is a type in a generic function call, in which case do not evaluate it
			bool isType = isFunctionParameterGeneric(*pCalleeFunctionType, argi);
			if (isType) {
				continue;
			}

			// check if the function parameter was declared as const
			bool isConst = isFunctionParameterConst(*pCalleeFunctionType, argi);

			// check if the parameter is passed byref, in which case it should be a variable and we will pass the address of the identifier
			auto attr = calleeFunc->getAttributeAtIndex(argi + 1, llvm::Attribute::AttrKind::ByRef);
			bool isByRef = attr.hasAttribute(llvm::Attribute::AttrKind::ByRef);
			if (isByRef) {
				bool foundVariable = false;

				// retrieve the actual type for ByRef arguments
				argType.containedType = calleeFunc->getArg(argi)->getParamByRefType();

				if (argument.Is<UNARY>()) {
					const UNARY& unary = *argument.Get<UNARY>();
					std::string_view operatorStr = unary.pOpToken->Value;

					if (operatorStr == "&"
						&& unary.pExpression->Is<PRIMARY>()
						&& unary.pExpression->Get<PRIMARY>()->Is<VARIABLE>()) {
						TypeSubtypePair tempType = { nullptr, nullptr };
						PtrValuePair left = generateIdentifier(moduleTable, state, *unary.pExpression->Get<PRIMARY>()->Get<VARIABLE>(),
							tempType);
						if (left.Ptr == nullptr) {
							logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: argumentID
								"Function argument {0} expects a reference to a variable preceded by the & symbol!", argi + 1);
							goto error;
						}
						// check that we are passing a reference to a variable of the right type
						if (tempType.type != argType.containedType) {
							logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: argumentID
								"Function argument {0} expects a reference to a variable of type!", argi + 1, state.NamedValues.GetTypeName(argType.containedType));
							goto error;
						}
						argVal = left.Ptr;
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

					argVal = generateExpression(moduleTable, state, argument, argType).Value;
					// create a pointer to the evaluated expression and pass the pointer as argument
					llvm::AllocaInst* ptr = state.Builder->CreateAlloca(argVal->getType());
					state.Builder->CreateStore(argVal, ptr);
					argVal = ptr;
				}
			}
			else {
				argVal = generateExpression(moduleTable, state, argument, argType).Value;

				if (argVal == nullptr)
				{
					logErrorAtCurrentPosition(moduleTable, nullptr, // nodeID
						"Error evaluating expression!");
					goto error;
				}

				if (argi < calleeFunc->arg_size()) {
					convertValueToType(state, argVal, calleeFunc->getArg(argi)->getType());

					if (argVal->getType() != calleeFunc->getArg(argi)->getType())
					{
						logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: argumentID
							"Function argument {0} expects value of type '{1}' but given type is '{2}'!", argi + 1,
							state.NamedValues.GetTypeName(calleeFunc->getArg(argi)->getType()),
							state.NamedValues.GetTypeName(argVal->getType()));
						goto error;
					}
				}
			}

			args.push_back(argVal);
		}

		if (!calleeFunc->isVarArg() && calleeFunc->arg_size() != args.size())
		{
			logErrorAtCurrentPosition(moduleTable, functionCallExpressionNode.pFunctionNameToken, "Function '{0}' argument mismatch!", functionName);
			goto error;
		}

		result = state.Builder->CreateCall(calleeFunc, args,
			(calleeFunc->getReturnType()->getTypeID() != llvm::Type::VoidTyID ? functionName : "")	// giving the return value a name solves a bug internal to llvm, e.g. the switch/case unit test
		);

	error:
		return result;
	}

	PtrValuePair generateEnclosedExpression(ModuleTable& moduleTable, LLVMState& state, const ENCLOSED_EXPRESSION& enclosedExpressionNode,
		TypeSubtypePair& expectedType)
	{
		return generateExpression(moduleTable, state, *enclosedExpressionNode.pExpression, expectedType);
	}

	llvm::Value* compareEnumValues(ModuleTable& moduleTable, LLVMState& state,
		llvm::StructType* leftType, const PtrValuePair* leftVal, llvm::StructType* rightType, const PtrValuePair* rightVal,
		bool capture
	)
	{
		//
		// for enums, we compare the types and the first element of the structure which the ID of the enum element
		//
		llvm::Value* result = nullptr;
		llvm::Value* result1 = nullptr;
		llvm::Value* result2 = nullptr;

		if (leftType == rightType)
		{
			llvm::Value* left = state.Builder->CreateLoad(leftType, leftVal->Ptr);
			llvm::Value* right = state.Builder->CreateLoad(rightType, rightVal->Ptr);
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

	llvm::Value* generateBinaryExpression(ModuleTable& moduleTable, LLVMState& state, const BINARY& binaryExpressionNode)
	{
		TypeSubtypePair leftExpressionType = { nullptr, nullptr };
		PtrValuePair left = generateExpression(moduleTable, state, *binaryExpressionNode.pLeft, leftExpressionType);
		llvm::Value* leftVal = left.Value;
		TypeSubtypePair rightExpressionType = { leftVal->getType(), nullptr };	// when computing rightVal, try to obtain a value of same type as leftVal
		PtrValuePair right = generateExpression(moduleTable, state, *binaryExpressionNode.pRight, rightExpressionType);
		llvm::Value* rightVal = right.Value;

		if (!leftVal || !rightVal)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID, 
				"Error evaluating expression!");
			return nullptr;
		}

		// the parent type is set in case of enumerations and allows us to check if the enumerations are of the same type
		llvm::Type* leftType = (leftExpressionType.parentType == nullptr ? leftVal->getType() : leftExpressionType.parentType);
		llvm::Type* rightType = (rightExpressionType.parentType == nullptr ? rightVal->getType() : rightExpressionType.parentType);

		// check that types match
		if (leftType != rightType)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID
				"Binary operator must be applied to matching types! Current types are '{0}' and '{1}'.",
				state.NamedValues.GetTypeName(leftType), state.NamedValues.GetTypeName(rightType));
			return nullptr;
		}

		std::string_view operatorStr = binaryExpressionNode.pOpToken->Value;
		bool isFloatingPoint = leftType->isFloatingPointTy();

		// for structures or enums, llvm does not support comparing their values. Use our own procedure for comparison
		// only equal and not equal operators are supported
		if (leftType->isStructTy() || rightType->isStructTy()) {
			llvm::Value* result = nullptr;
			std::string_view leftTypeName = state.NamedValues.GetTypeName(leftType);
			ASSERT((static_cast<llvm::StructType*>(leftType))->isPacked(), "Structures should be created with the packed flag ON otherwise we cannot compare them");
			if (operatorStr != "==" && operatorStr != "!=") {
				logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID
					"Binary operator cannot be applied to structure or enum types! Current type is '{0}'.",
					leftTypeName);
			}
			else if (state.NamedValues.IsEnumType(leftType)) {
				// according to the unwritten specs, we only capture when the right hand side is an enum value and not an identifier (or anything else)
				bool capturePayload = binaryExpressionNode.pRight->Is<PRIMARY>() && binaryExpressionNode.pRight->Get<PRIMARY>()->Is<ENUM_VALUE>();
				result = compareEnumValues(moduleTable, state, static_cast<llvm::StructType*>(leftExpressionType.type), &left, static_cast<llvm::StructType*>(rightExpressionType.type), &right, capturePayload);
			}
			else {
				result = compareStructValues(moduleTable, state, static_cast<llvm::StructType*>(leftType), leftVal, static_cast<llvm::StructType*>(rightType), rightVal);
			}

			// for the not equal operator, invert the result
			if (operatorStr == "!=") {
				result = state.Builder->CreateNot(result);
			}

			return result;
		}

		// logical operators

		if (operatorStr == "&&")
		{
			return state.Builder->CreateAnd(leftVal, rightVal, "andtmp");
		}

		if (operatorStr == "||")
		{
			return state.Builder->CreateOr(leftVal, rightVal, "ortmp");
		}

		// relational operators

		if (operatorStr == "==")
		{
			return isFloatingPoint
				? state.Builder->CreateFCmpOEQ(leftVal, rightVal, "eqtmp")
				: state.Builder->CreateICmpEQ(leftVal, rightVal, "eqtmp");
		}

		if (operatorStr == "!=")
		{
			return isFloatingPoint
				? state.Builder->CreateFCmpONE(leftVal, rightVal, "neqtmp")
				: state.Builder->CreateICmpNE(leftVal, rightVal, "neqtmp");
		}

		if (operatorStr == ">")
		{
			return isFloatingPoint
				? state.Builder->CreateFCmpUGT(leftVal, rightVal, "gttmp")
				: state.Builder->CreateICmpSGT(leftVal, rightVal, "gttmp");
		}

		if (operatorStr == "<")
		{
			return isFloatingPoint
				? state.Builder->CreateFCmpULT(leftVal, rightVal, "lttmp")
				: state.Builder->CreateICmpSLT(leftVal, rightVal, "lttmp");
		}

		if (operatorStr == ">=")
		{
			return isFloatingPoint
				? state.Builder->CreateFCmpUGE(leftVal, rightVal, "getmp")
				: state.Builder->CreateICmpSGE(leftVal, rightVal, "getmp");
		}

		if (operatorStr == "<=")
		{
			return isFloatingPoint
				? state.Builder->CreateFCmpULE(leftVal, rightVal, "letmp")
				: state.Builder->CreateICmpSLE(leftVal, rightVal, "letmp");
		}

		// additive operators

		if (operatorStr == "+")
		{
			return isFloatingPoint
				? state.Builder->CreateFAdd(leftVal, rightVal, "addtmp")
				: state.Builder->CreateAdd(leftVal, rightVal, "addtmp");
		}

		if (operatorStr == "-")
		{
			return isFloatingPoint
				? state.Builder->CreateFSub(leftVal, rightVal, "subtmp")
				: state.Builder->CreateSub(leftVal, rightVal, "subtmp");
		}

		// multiplicative operators

		if (operatorStr == "*")
		{
			return isFloatingPoint
				? state.Builder->CreateFMul(leftVal, rightVal, "multmp")
				: state.Builder->CreateMul(leftVal, rightVal, "multmp");
		}

		if (operatorStr == "/")
		{
			return isFloatingPoint
				? state.Builder->CreateFDiv(leftVal, rightVal, "divtmp")
				: state.Builder->CreateUDiv(leftVal, rightVal, "divtmp");
		}

		if (operatorStr == "%")
		{
			return isFloatingPoint
				? state.Builder->CreateFRem(leftVal, rightVal, "modtmp")
				: state.Builder->CreateURem(leftVal, rightVal, "modtmp");
		}

		ASSERT(false, "Unknown binary operator '{0}'!", operatorStr);
		return nullptr;
	}

	llvm::Value* generateUnaryExpression(ModuleTable& moduleTable, LLVMState& state, const UNARY& unaryExpressionNode,
		TypeSubtypePair& expectedType)
	{
		std::string_view operatorStr = unaryExpressionNode.pOpToken->Value;
		llvm::Value* result = nullptr;

		if (operatorStr == "-")
		{
			llvm::Value* expressionVal = generateExpression(moduleTable, state, *unaryExpressionNode.pExpression, expectedType).Value;

			if (expressionVal)
			{
				bool isFloatingPoint = expressionVal->getType()->isFloatingPointTy();
				if (isFloatingPoint)
					result = state.Builder->CreateFNeg(expressionVal, "negtmp");
				else
					result = state.Builder->CreateNeg(expressionVal, "negtmp");
			}
		}
		else
			if (operatorStr == "!")
			{
				llvm::Value* expressionVal = generateExpression(moduleTable, state, *unaryExpressionNode.pExpression, expectedType).Value;

				if (expressionVal)
				{
					result = state.Builder->CreateNot(expressionVal, "nottmp");
				}
			}
			else
				if (operatorStr == "&"
					&& unaryExpressionNode.pExpression->Is<PRIMARY>()
					&& unaryExpressionNode.pExpression->Get<PRIMARY>()->Is<VARIABLE>())
				{
					TypeSubtypePair tempType = { nullptr, nullptr };
					PtrValuePair left = generateIdentifier(moduleTable, state,
						*unaryExpressionNode.pExpression->Get<PRIMARY>()->Get<VARIABLE>(),
						tempType);
					result = left.Ptr;
				}
				else
				{
					ASSERT(false, "Unknown unary operator!");
				}

		return result;
	}

	PtrValuePair generatePrimaryExpression(ModuleTable& moduleTable, LLVMState& state, const PRIMARY& primary,
		TypeSubtypePair& expectedType)
	{
		//
		// using PRIMARY = VariantNode<LITERAL, VARIABLE, VARIABLE_DEFINITION, FUNCTION_CALL, CONSTRUCTOR,
		//  	POINTER_INIT, POINTER_MOVE, INITIALIZER_LIST, ENCLOSED_EXPRESSION>;
		//
		PtrValuePair result;

		if (primary.Is<LITERAL>()) {
			result.Value = generateLiteral(moduleTable, state, *primary.Get<LITERAL>(), expectedType);
		}
		else if (primary.Is<VARIABLE>()) {
			result = generateIdentifier(moduleTable, state, *primary.Get<VARIABLE>(), expectedType);
		}
		else if (primary.Is<CONSTRUCTOR>()) {
			result.Value = generateConstructorExpression(moduleTable, state, *primary.Get<CONSTRUCTOR>());
		}
		else if (primary.Is<POINTER_INIT>()) {
			result.Value = generatePointerInitializerExpression(moduleTable, state, *primary.Get<POINTER_INIT>(), expectedType);
		}
		else if (primary.Is<POINTER_MOVE>()) {
			result.Value = generatePointerMoveExpression(moduleTable, state, *primary.Get<POINTER_MOVE>(), expectedType);
		}
		else if (primary.Is<INITIALIZER_LIST>()) {
			result.Value = generateInitializerListExpression(moduleTable, state, *primary.Get<INITIALIZER_LIST>(), expectedType);
		}
		else if (primary.Is<VARIABLE_DEFINITION>()) {
			result.Value = generateVariableDefinitionExpression(moduleTable, state, *primary.Get<VARIABLE_DEFINITION>());
		}
		else if (primary.Is<ARRAY_ACCESS>()) {
			result = generateArrayAccessExpression(moduleTable, state, *primary.Get<ARRAY_ACCESS>(), expectedType);
		}
		else if (primary.Is<MEMBER_ACCESS>()) {
			result = generateMemberAccessExpression(moduleTable, state, *primary.Get<MEMBER_ACCESS>(), expectedType);
		}
		else if (primary.Is<FUNCTION_CALL>()) {
			result.Value = generateFunctionCallExpression(moduleTable, state, *primary.Get<FUNCTION_CALL>());
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

	llvm::Value* generateAssignmentExpression(ModuleTable& moduleTable, LLVMState& state, const ASSIGNMENT& assignment,
		TypeSubtypePair& expectedType)
	{
		PtrValuePair ptrValue;
		llvm::Value* ptr = nullptr;

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

		ptr = ptrValue.Ptr;

		if (!ptr)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID 
				"Error evaluating identifier!");
			return nullptr;
		}

		if (ptrValue.isConst) {
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID 
				"Assigning a value to a constant!");
			return nullptr;
		}

		llvm::Value* expressionVal = generateExpression(moduleTable, state, *assignment.pValue, expectedType).Value;

		if (!expressionVal)
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID
				"Error evaluating expression!");
			return nullptr;
		}

		// store the value into the alloca
		if (llvm::isa<llvm::AllocaInst>(ptr)) {
			llvm::AllocaInst* alloc = static_cast<llvm::AllocaInst*>(ptr);
			convertValueToType(state, expressionVal, alloc->getAllocatedType());
		}

		// store the value into the alloca
		return state.Builder->CreateStore(expressionVal, ptr);
	}

	PtrValuePair generateExpression(ModuleTable& moduleTable, LLVMState& state, const EXPRESSION& expressionNode,
		TypeSubtypePair& expectedType)
	{
		PtrValuePair result;

		if (expressionNode.Is<BINARY>()) {
			result.Value = generateBinaryExpression(moduleTable, state, *expressionNode.Get<BINARY>());
		}
		else if (expressionNode.Is<UNARY>()) {
			result.Value = generateUnaryExpression(moduleTable, state, *expressionNode.Get<UNARY>(), expectedType);
		}
		else if (expressionNode.Is<PRIMARY>()) {
			result = generatePrimaryExpression(moduleTable, state, *expressionNode.Get<PRIMARY>(), expectedType);
		}
		else if (expressionNode.Is<ASSIGNMENT>()) {
			result.Value = generateAssignmentExpression(moduleTable, state, *expressionNode.Get<ASSIGNMENT>(), expectedType);
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
				result.Value = generateFunctionCallExpression(moduleTable, state, *postfix.Get<FUNCTION_CALL>());
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
		return generateVariableDefinitionExpression(moduleTable, state, variableDefinition);
	}

	llvm::Value* generateFunctionCallStatement(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_CALL& functionCall)
	{
		return generateFunctionCallExpression(moduleTable, state, functionCall);
	}

	llvm::Value* generateAssignmentStatement(ModuleTable& moduleTable, LLVMState& state, const ASSIGNMENT& assignment)
	{
		TypeSubtypePair identifierType = { nullptr, nullptr };
		return generateAssignmentExpression(moduleTable, state, assignment, identifierType);
	}

	llvm::Value* generateForLoopStatement(ModuleTable& moduleTable, LLVMState& state, const FOR_LOOP& forLoop)
	{
		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();

		ASSERT(func != nullptr, "No function to insert into!");

		// emit init code before the loop
		if (forLoop.pInitialization != nullptr)
		{
			TypeSubtypePair tempType = { nullptr, nullptr };
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
			llvm::Value* bodyVal = generateStatement(moduleTable, state, *forLoop.pBody);

			if (bodyVal == nullptr)
			{
				logErrorAtCurrentPosition(moduleTable, nullptr,	// TBD: forLoop... 
					"Error evaluating expression!");
				return nullptr;
			}
		}

		// emit step value
		if (forLoop.pIncrement != nullptr) {
			TypeSubtypePair tempType = { nullptr, nullptr };
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
			TypeSubtypePair tempType = { nullptr, nullptr };
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
		llvm::Value* bodyVal = generateStatement(moduleTable, state, *whileLoop.pStatement);

		if (bodyVal == nullptr)
		{
			return nullptr;
		}

		// emit the condition
		TypeSubtypePair tempType = { nullptr, nullptr };
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
					payload->getType()
				);
				state.Builder->CreateStore(payload, allocaInst);
				// add the variable to the named values
				state.NamedValues.InsertValue(name, allocaInst, payload->getType(),
					nullptr, false, false);
			}
		}
	}

	llvm::Value* generateIfStatement(ModuleTable& moduleTable, LLVMState& state, const IF_STATEMENT& ifStatement)
	{
		TypeSubtypePair tempType = { nullptr, nullptr };
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
		llvm::Value* elseVal = nullptr;

		func->insert(func->end(), elseBlock);
		state.Builder->SetInsertPoint(elseBlock);

		if (ifStatement.pElseStatement != nullptr)
		{
			elseVal = generateStatement(moduleTable, state, *ifStatement.pElseStatement);

			if (elseVal == nullptr)
			{
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
					"Function expects a return value of type '{0}'!", state.NamedValues.GetTypeName(state.CurrentReturnValue->getType()));

				return nullptr;
			}

			return state.Builder->CreateBr(state.FuncExitBlock);
		}

		TypeSubtypePair tempType = { nullptr, nullptr };
		llvm::Value* expressionValue = generateExpression(moduleTable, state, *returnStatement.pValue, tempType).Value;

		if (expressionValue == nullptr)
		{
			return nullptr;
		}

		if (state.CurrentReturnValue != nullptr) {
			// convert the return value to the right type
			// TODO: should we generate a warning here?
			convertValueToType(state, expressionValue, state.CurrentReturnValue->getAllocatedType());
		}

		if (state.CurrentReturnValue == nullptr
			|| expressionValue->getType() != state.CurrentReturnValue->getAllocatedType())
		{
			logErrorAtCurrentPosition(moduleTable, nullptr, // TBD: nodeID,
				"Function has return type '{0}' but provided return value is of type '{1}'!",
				state.CurrentReturnValue == nullptr ? "void" : state.NamedValues.GetTypeName(state.CurrentReturnValue->getAllocatedType()),
				state.NamedValues.GetTypeName(expressionValue->getType()));
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
		llvm::Type* EnumType = nullptr;
		llvm::Value* payload = nullptr;

		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();
		ASSERT(func != nullptr, "No function to insert into!");

		// generate and check the condition expression for the switch statement
		TypeSubtypePair tempType = { nullptr, nullptr };
		llvm::Value* conditionVal = generateExpression(moduleTable, state, *statement.pSwitchValue, tempType).Value;
		if (conditionVal == nullptr)
		{
			goto failed;
		}

		// switch/case statements using enumerations, we need to retrieve the index of enum value
		if (state.NamedValues.GetTypeName(conditionVal->getType()).starts_with(_EnumPayloadStruct_))
		{
			EnumType = tempType.parentType;
			// check if there is a payload and load it
			payload = state.Builder->CreateExtractValue(conditionVal, EnumPayloadValue);
			// now extract the index of the enum value
			conditionVal = state.Builder->CreateExtractValue(conditionVal, EnumPayloadIndex);
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
		switchInst = state.Builder->CreateSwitch(conditionVal, afterBlock, statement.Cases.size());

		for (auto& caseStmt : statement.Cases) {
			EXPRESSION* expr = std::get<0>(caseStmt);
			tempType = { nullptr, nullptr };

			// in order to accomodate enum payload capture, we need to create a new identifier scope before entering the case's statement block
			state.NamedValues.PushScope("");

			// create a new basic block to start insertion into
			llvm::BasicBlock* caseBlock = llvm::BasicBlock::Create(*state.Context, "case", func);

			// start insertion into the caseBlock
			state.Builder->SetInsertPoint(caseBlock);

			PtrValuePair cond = generateExpression(moduleTable, state, *expr, tempType);
			if (EnumType != nullptr) {
				// switch/case statements using enumerations, the enumerations should be of the same type
				if (EnumType != tempType.parentType) {
					logErrorAtCurrentPosition(moduleTable, nullptr, "Case condition type {0} is not of the same enum type as switch value {1}!",
						state.NamedValues.GetTypeName(tempType.parentType), state.NamedValues.GetTypeName(EnumType));
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
				llvm::Value* bodyVal = generateStatement(moduleTable, state, *std::get<2>(caseStmt));

				if (bodyVal == nullptr)
				{
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
		return generateFunctionDeclaration(moduleTable, state, nullptr, externDefinition, moduleName, {}, typeMap);
	}

	llvm::Value* generateVariableDefinition(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DEFINITION& variableDefinition)
	{
		return generateVariableDefinitionExpression(moduleTable, state, variableDefinition);
	}

	TypeSubtypePair generateArrayDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
		const ARRAY_TYPE& arrayDefinition, const GenericArgumentTypes& genericArguments,
		const GenericTypeMap& parentTypeMap)
	{
		//
		// return the type and subtype
		// e.g. var array : [i64; 10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }; when we encounter var array : [i64; 10] we know that we are
		// initializing an i64 array of 10 elements
		//

		TypeSubtypePair identifierType = { nullptr, nullptr };
		uint64_t arraySize = 0;
		TypeModifier modifier = TypeModifier::None;
		llvm::Type* elementType = nullptr;
		std::string arrayName;
		GenericTypeMap genericTypeMap(parentTypeMap);

		if (typeIdentifier.pNameToken) {
			// create the map from the array's generic types to the actual types requested by the caller
			int arg = 0;
			if (genericArguments.size())
				for (auto& type : typeIdentifier.GenericParameters) {
					genericTypeMap[std::string(type->pIdentifierToken->Value)] = std::get<1>(genericArguments[arg]);
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
				identifierType = { nullptr, nullptr };
				goto exit;
			}
		}

		identifierType = generateTypeIdentifier(moduleTable, state, *arrayDefinition.pElementType, nullptr, modifier, genericTypeMap);
		elementType = identifierType.type;

		if (identifierType.type == nullptr)
		{
			logErrorAtCurrentPosition(moduleTable, arrayDefinition.pElementType->GetErrorToken(), "Unknown array element type!");
			identifierType = { nullptr, nullptr };
			goto exit;
		}

		switch (modifier) {
		case TypeModifier::None:
			break;

		case TypeModifier::Pointer:
		case TypeModifier::Reference:
		{
			// allocating a pointer to this type
			identifierType.containedType = identifierType.type;
			elementType = llvm::PointerType::get(identifierType.containedType, 0);	// the contained type is not stored by llvm as all pointers are treated as "opaque"
			break;
		}

		default:
		{
			ASSERT(false, "Invalid type modifier!");
			identifierType = { nullptr, nullptr };
			goto exit;
			break;
		}
		}

		if (arraySize == 0) {
			identifierType.type = llvm::PointerType::get(elementType, 0);	// when size is unknown, just create a pointer. elementType is not used by llvm
			identifierType.containedType = elementType;
		}
		else {
			identifierType.type = llvm::VectorType::get(elementType, arraySize,
				false //not scalable
			);
		}

		// if we have a name token, add the newly created array type to NamedValues
		if (!arrayName.empty()) {
			state.NamedValues.InsertType(arrayName, identifierType.type, NamedValues::UserDefinedType::array);
		}

	exit:
		return identifierType;
	}

	llvm::Type* generateStructDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
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
				genericTypeMap[std::string(type->pIdentifierToken->Value)] = std::get<1>(genericArguments[arg]);
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
			TypeModifier modifier = TypeModifier::None;
			TypeSubtypePair identifierType = generateTypeIdentifier(moduleTable, state, *id.second, nullptr, modifier, genericTypeMap);

			if (!identifierType.type)
			{
				logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
					"Invalid structure member type for structure '{0}'!", structName);
				goto failed;
			}

			switch (modifier) {
			case TypeModifier::None:
				break;

			case TypeModifier::Pointer:
			case TypeModifier::Reference:
			{
				// allocating a pointer to this type, unless it is already a pointer
				if (!llvm::isa<llvm::PointerType>(identifierType.type)) {
					identifierType.containedType = identifierType.type;
					identifierType.type = llvm::PointerType::get(identifierType.containedType, 0);	// the contained type is not stored by llvm as all pointers are treated as "opaque"
				}
				break;
			}

			default:
			{
				ASSERT(false, "Invalid type modifier!");
				identifierType = { nullptr, nullptr };
				goto failed;
				break;
			}
			}

			memberTypes.push_back(identifierType.type);

			// retrieve member name and add it to memberNames map
			const std::string_view memberName = id.first->Value;

			memberNames[memberName] = { memberIndex++, identifierType.containedType };
		}

		structType = llvm::StructType::create(*state.Context, memberTypes, structName, true);

		if (!structType)
		{
			logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
				"Invalid structure type for structure '{0}'!", structName);
			goto failed;
		}

		state.NamedValues.InsertType(structName, structType, NamedValues::UserDefinedType::structure,
			memberNames, genericTypeMap);

	failed:
		return structType;
	}

	llvm::Type* generateEnumDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
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
				genericTypeMap[std::string(type->pIdentifierToken->Value)] = std::get<1>(genericArguments[arg]);
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
			TypeModifier modifier = TypeModifier::None;
			TypeSubtypePair identifierType = { nullptr, nullptr };
			if (id.second != nullptr) {
				identifierType = generateTypeIdentifier(moduleTable, state, *id.second, nullptr, modifier, genericTypeMap);

				if (!identifierType.type)
				{
					logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
						"Invalid enum member type for enum '{0}'!", enumName);
					goto failed;
				}

				switch (modifier) {
				case TypeModifier::None:
					break;

				case TypeModifier::Pointer:
				case TypeModifier::Reference:
				{
					// allocating a pointer to this type, unless it is already a pointer
					if (!llvm::isa<llvm::PointerType>(identifierType.type)) {
						identifierType.containedType = identifierType.type;
						identifierType.type = llvm::PointerType::get(identifierType.containedType, 0);	// the contained type is not stored by llvm as all pointers are treated as "opaque"
					}
					break;
				}

				default:
				{
					ASSERT(false, "Invalid type modifier!");
					identifierType = { nullptr, nullptr };
					goto failed;
					break;
				}
				}

				memberTypes.push_back(identifierType.type);
			}

			// retrieve member name and add it to memberNames map
			const std::string_view memberName = id.first->Value;

			memberNames[memberName] = { memberIndex++, identifierType.type };
		}

		enumType = llvm::StructType::create(*state.Context, memberTypes, enumName, true);

		if (!enumType)
		{
			logErrorAtCurrentPosition(moduleTable, typeIdentifier.pNameToken,
				"Invalid enum type for enum '{0}'!", enumName);
			goto failed;
		}

		state.NamedValues.InsertType(enumName, enumType, NamedValues::UserDefinedType::enumeration,
			memberNames, genericTypeMap);

	failed:
		return enumType;
	}

	llvm::Function* generateFunctionDefinition(ModuleTable& moduleTable, LLVMState& state,
		llvm::Type* parentType,
		const FUNCTION_DEFINITION& functionDefinition, const std::string& moduleName,
		const std::vector<EXPRESSION*>& functionArguments)
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

		llvm::Function* func = generateFunctionDeclaration(moduleTable, state, parentType, functionDefinition, moduleName, functionArguments, typeMap);

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
			state.CurrentReturnValue = createEntryBlockAlloca(func, "", func->getReturnType());
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
			llvm::Type* subType = nullptr;
			llvm::AllocaInst* allocaInst = nullptr;
			if (attr.hasAttribute(llvm::Attribute::AttrKind::ByRef)) {
				subType = arg->getParamByRefType();
			}
			// create an alloca for this variable
			allocaInst = createEntryBlockAlloca(func, arg->getName().str(), arg->getType());

			// store the initial value into the alloca
			state.Builder->CreateStore(arg, allocaInst);

			// add arguments to named values
			state.NamedValues.InsertValue(std::string(arg->getName()), allocaInst, subType, nullptr, isFunctionParameterConst(*functionDefinition.pFunctionType, index), false);
		}

		// every function has an exit block for cleanup and setting the return value
		llvm::BasicBlock* previousExitBlock = state.FuncExitBlock;
		state.FuncExitBlock = llvm::BasicBlock::Create(*state.Context, "exit", func);

		// check if function has any statements and generate them
		if (functionDefinition.pBody->Statements.size() > 0) {
			llvm::Value* bodyVal = generateStatementBlock(moduleTable, state, *functionDefinition.pBody);

			if (!bodyVal)
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

		if (state.Optimizations)
		{
			// run the optimizer on the function.
			state.FPM->run(*func, *state.FAM);
		}

		state.Builder.reset(PreviousBuilder);
		moduleTable.PopContext();

		return func;
	}

	llvm::Type* generateTypeDefinition(ModuleTable& moduleTable, LLVMState& state,
		const TYPE_DEFINITION& typeDefinition, const std::string& moduleName,
		const GenericArgumentTypes& genericArguments
	)
	{
		moduleTable.PushContext(moduleName);

		llvm::Type* result = nullptr;

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
				bool isStruct = result->isStructTy();
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
			result = generateArrayDefinition(moduleTable, state, *typeDefinition.pTypeIdentifier, arrayType, genericArguments, {}).type;
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
		llvm::Function* result = generateFunctionDefinition(moduleTable, state, nullptr, *pMainFunction, moduleTable.GetCurrentContext(), {});
		state.MainFunctionName = moduleTable.GetCurrentContext() + "::main";

		std::error_code errorCode;
		llvm::raw_fd_ostream out("c:\\temp\\out.ll", errorCode);

		if (state.Optimizations)
		{
			// run the optimizer on the module
			// state.MPM.run(*state.Module, *state.MAM);
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