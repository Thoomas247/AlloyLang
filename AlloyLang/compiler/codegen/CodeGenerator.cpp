#include "CodeGenerator.hpp"

namespace AlloyCompiler
{

	// forward declarations
	llvm::Value* generateExpression(ModuleTable& moduleTable, LLVMState& state, const EXPRESSION& expressionNode,
		const TypeSubtypePair& expectedType);
	llvm::Value* generateVariableDefinition(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DEFINITION& variableDefinition);
	llvm::Value* generateStatement(ModuleTable& moduleTable, LLVMState& state, const STATEMENT& statement);
	llvm::Value* generateStatementBlock(ModuleTable& moduleTable, LLVMState& state, const STATEMENT_BLOCK& statementBlock);
	llvm::Function* generateFunctionDefinition(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_DEFINITION& functionDefinition);
	llvm::Type* generateTypeDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_DEFINITION& typeDefinition, const std::vector<TYPE*>& genericArguments);
	llvm::Function* generateExternDefinition(ModuleTable& moduleTable, LLVMState& state, const EXTERN_DEFINITION& externDefinition);

#pragma region Util

	struct PtrValuePair
	{
		llvm::Value* Ptr = nullptr;
		llvm::Value* Value = nullptr;
		bool isConst = false;	// indicates that we are pointing to constant variables
	};

	template<typename ...Args>
	constexpr void logErrorAtCurrentPosition(const Token* token, const std::string& format, Args && ...args)
	{
		if (token != nullptr) {
			const Location& location = token->Location;
			Log::Error("Error at location ({0} : {1}):", location.Line, location.Column);
/// TBD		Log::Error("\t{0}", tokenBuffers.GetLine(location.LineStart));
			Log::Error("\t{0}^", std::string(location.Column - 1, ' '));
			Log::Error("\t{0}{1}", std::string(location.Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));
		}
		else {
			Log::Error("\t{0}", std::vformat(format, std::make_format_args(args...)));
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
		llvm::Type* oldType = value->getType();
		bool result = false;

		if (oldType == newType) {
			return true;	// nothing to do
		}

		switch (oldType->getTypeID()) {
		case llvm::Type::IntegerTyID:
		{
			switch (newType->getTypeID()) {
			case llvm::Type::FloatTyID:
			case llvm::Type::DoubleTyID:
				value = state.Builder->CreateSIToFP(value, newType);
				result = true;
				break;

			case llvm::Type::IntegerTyID:
				value = state.Builder->CreateIntCast(value, newType, true);
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
		// numElements added for array support
		//
		llvm::IRBuilder<> tempBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
		llvm::Value* elements = nullptr;
		if (numElements > 0) {
			elements = llvm::ConstantInt::get(function->getContext(), llvm::APInt(64, numElements));
		}
		return tempBuilder.CreateAlloca(type, elements, varName);
	}

	llvm::Type* getTypeFromTypeName(ModuleTable& moduleTable, LLVMState& state, Token* pNameToken,
									const std::vector<TYPE*>& genericArguments,
									std::unordered_map<std::string, std::string>* genericTypeMap)
	{
		//
		// Helper function to return an llvm type given its type name
		// Also searches the named nodes if the type has not been defined yet and define it
		//
		std::string name = NodeBuffer::GetMangledName(pNameToken->Value, genericArguments);
		
		// if we have a type map that contains the requested type, retrieve the actual type, e.g. T might be an i32
		if (genericTypeMap != nullptr
			&& genericTypeMap->contains(name)
			) {
			name = genericTypeMap->at(name);
		}
		llvm::Type* type = state.NamedValues.GetType(name);

		if (type == nullptr)
		{
			// type not found, it might not have been processed yet
			// try to locate it in the program and process it

			SearchResult<TYPE_DEFINITION> result = moduleTable.GetTypeDefinition(pNameToken->Value);

			if (result.Code == SearchResultCode::NotFound)
			{
				logErrorAtCurrentPosition(pNameToken, "Type '{0}' does not exist.", pNameToken->Value);
			}

			else if (result.Code == SearchResultCode::Inaccessible)
			{
				logErrorAtCurrentPosition(pNameToken, "Type '{0}' is private and cannot be accessed here.", pNameToken->Value);
			}
			else
			{
				generateTypeDefinition(moduleTable, state, *result.pDefiniton, genericArguments);
				type = state.NamedValues.GetType(name);
			}
		}

		return type;
	}

	//
	// Helper function to determine if a function parameter is declared as const
	//
	bool isFunctionParameterConst(const FUNCTION_TYPE& functionType, int index) {

		// for functions with variable parameters, we can only check the known parameters
		if (functionType.IsVarArg
			&& index >= functionType.Parameters.size()) {
			return false;
		}
		else {
			const FUNCTION_PARAMETER* parameter = functionType.Parameters[index];
			ASSERT(parameter->Is<VARIABLE_DECLARATION>(), "TODO: add support for generics in codegen");

			// TODO: fix this
			// temporary hack to get the code compiling
			VARIABLE_DECLARATION* pParameterVariableDeclaration = parameter->Get<VARIABLE_DECLARATION>();
			return (pParameterVariableDeclaration->VarType == VariableType::Constant);
		}
	}
#pragma endregion

#pragma region Literals

	llvm::Value* generateLiteral(ModuleTable& moduleTable, LLVMState& state, const LITERAL& literalNode,
								const TypeSubtypePair& identifierType)
	{
		const std::string_view literalStr = literalNode.pValueToken->Value;
		llvm::Value* result = nullptr;

		// get the correct type of value that we are generating
		llvm::Type* thisType = identifierType.type;
		if (thisType && isa<llvm::VectorType>(thisType)) {
			thisType = static_cast<llvm::VectorType*>(thisType)->getElementType();		// for arrays, we want the element type, not the array itself
		}

		switch (literalNode.Type)
		{
		case LiteralType::Integer:
		{
			// TODO: allow negative values
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

			// set the current type and subtype
			identifierType.type = valueOrPtr->getType();

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

		logErrorAtCurrentPosition(variable.pNameToken, "Unknown variable name '{0}'!", name);
		return {};
	}

	TypeSubtypePair generateTypeIdentifier(ModuleTable& moduleTable, LLVMState& state, const TYPE& typeIdentifier,
														Token* pParentTypeNameToken, TypeModifier& modifier,
														std::unordered_map<std::string, std::string>* genericTypeMap)
	{
		//
		// genericTypeMap maps from the generic type names to the actual type names, can be null
		//
		TypeSubtypePair identifierType = { nullptr, nullptr };

		// let the caller know if this is a pointer, a reference or none
		modifier = typeIdentifier.Modifier;

		// handle non-array types
		if (typeIdentifier.Type.Is<TYPE_NAME>() )
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
					logErrorAtCurrentPosition(typeName.pNameToken,
						"Self used with non member function!");
					identifierType = { nullptr, nullptr };
					goto exit;
				}
				pNameToken = pParentTypeNameToken;
			}

			identifierType.type = getTypeFromTypeName(moduleTable, state, pNameToken, typeName.GenericArguments, genericTypeMap);

			if (!identifierType.type)
			{
				logErrorAtCurrentPosition(typeName.pNameToken,
					"Unknown type name '{0}'!", pNameToken->Value);
				identifierType = { nullptr, nullptr };
				goto exit;
			}
		}
		// handle array types
		else
		{
			ASSERT(typeIdentifier.Type.Is<ARRAY_TYPE>(), "Expected type identifier node!");

			uint64_t arraySize = 0;
			const ARRAY_TYPE& type = *typeIdentifier.Type.Get<ARRAY_TYPE>();

			// get size if given
			if (type.pSizeLiteral != nullptr)
			{
				std::string_view sizeStr = type.pSizeLiteral->pValueToken->Value;
				std::from_chars(sizeStr.data(), sizeStr.data() + sizeStr.size(), arraySize);

				if (arraySize == 0)
				{
					logErrorAtCurrentPosition(nullptr, // typeIdentifierNode.ArraySizeID
											"Array size must be greater than 0!");
					identifierType = { nullptr, nullptr };
					goto exit;
				}
			}

			TypeModifier modifier = TypeModifier::None;
			identifierType = generateTypeIdentifier(moduleTable, state, *type.pElementType, pParentTypeNameToken, modifier, nullptr);
			llvm::Type* elementType = identifierType.type;

			if (identifierType.type == nullptr)
			{
				logErrorAtCurrentPosition(nullptr, // elementTypeIdentifierNode
										"Unknown array element type!");
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

			identifierType.type = llvm::VectorType::get(elementType, 
														(arraySize == 0 ? 1 : arraySize), 
														(arraySize == 0 ? true : false)		//scalable
														);
		}

		// return the identifier type and subtype
		// e.g. var array : [i64; 10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }; when we encounter var array : [i64; 10] we know that we are
		// initializing an i64 array of 10 elements
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
			return generateTypeIdentifier(moduleTable, state, *typeDeclarationNode.pReturnType->pType, nullptr, modifier, nullptr);
		}
	}

	llvm::Value* generateVariableDeclaration(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DECLARATION& variableDeclarationNode,
											TypeSubtypePair& identifierType)
	{
		const std::string_view name = variableDeclarationNode.pNameToken->Value;
		TypeModifier modifier =	TypeModifier::None;
		identifierType = generateTypeIdentifier(moduleTable, state, *variableDeclarationNode.pType, nullptr, modifier, nullptr);

		if (!identifierType.type)
		{
			logErrorAtCurrentPosition(variableDeclarationNode.pNameToken, "Variable '{0}' type error!", name);
			return nullptr;
		}

		switch (modifier) {
		case TypeModifier::None:
				break;

			case TypeModifier::Pointer:
			{
				// allocating a pointer to this type
				identifierType.containedType = identifierType.type;
				identifierType.type = llvm::PointerType::get(identifierType.containedType, 0);
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

		// create the alloca
		llvm::AllocaInst* allocaInst = createEntryBlockAlloca(
			state.Builder->GetInsertBlock()->getParent(),
			name,
			identifierType.type
			);

		// add the variable to the named values
		state.NamedValues.InsertValue(std::string(name), allocaInst, identifierType.containedType, 
						(variableDeclarationNode.VarType == VariableType::Constant), 
						(modifier == TypeModifier::Pointer));

		return allocaInst;
	}

	llvm::Function* generateFunctionDeclaration(ModuleTable& moduleTable, LLVMState& state, 
												const FUNCTION_DEFINITION& functionDeclarationNode)
	{
		//
		// If type is not empty, we are generating a member function in the form of Type@Name
		//
		std::string name;
		if (functionDeclarationNode.pTypeIdentifier == nullptr)
		{
			name = functionDeclarationNode.pFunctionNameToken->Value;
		}
		else
		{
			name = NodeBuffer::GetMangledName(functionDeclarationNode.pTypeIdentifier->pNameToken, functionDeclarationNode.pFunctionNameToken);
		}

		// check if function already exists
		if (state.Module->getFunction(name))
		{
			// TODO: replace '@' in mangled name by ':' for error messages
			logErrorAtCurrentPosition(functionDeclarationNode.pFunctionNameToken, "Function '{0}' already defined!", name);
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
			ASSERT(parameter->Is<VARIABLE_DECLARATION>(), "TODO: add support for generics in codegen");

			// TODO: fix this
			// temporary hack to get the code compiling
			VARIABLE_DECLARATION* pParameterVariableDeclaration = parameter->Get<VARIABLE_DECLARATION>();

			TypeModifier modifier = TypeModifier::None;
			TypeSubtypePair identifierType = generateTypeIdentifier(moduleTable, state, *pParameterVariableDeclaration->pType,
																(functionDeclarationNode.pTypeIdentifier ? functionDeclarationNode.pTypeIdentifier->pNameToken : nullptr), 
																modifier, nullptr);

			if (!identifierType.type)
			{
				logErrorAtCurrentPosition(pParameterVariableDeclaration->pNameToken, "Function '{0}' parameter type error!", name);
				return nullptr;
			}

			// References should be passed as pointers
			if (modifier == TypeModifier::Reference) {
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

		// retrieve the return types
		llvm::Type* returnType = llvm::Type::getVoidTy(*state.Context);

		if (functionDeclarationNode.pFunctionType != nullptr
			&& functionDeclarationNode.pFunctionType->pReturnType != nullptr)
		{
			returnType = generateTypeDeclaration(moduleTable, state, *functionDeclarationNode.pFunctionType).type;
		}

		if (!returnType)
		{
			logErrorAtCurrentPosition(functionDeclarationNode.pFunctionNameToken, "Function '{0}' return type error!", name);
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
		for (size_t i = 0; i < function->arg_size(); i++)
		{
			// TODO: fix this
			// temporary hack to get the code compiling
			//const std::string_view paramName = functionDeclarationNode.pFunctionType->Parameters[i]->pNameToken->Value;
			const std::string_view paramName = functionDeclarationNode.pFunctionType->Parameters[i]->Get<VARIABLE_DECLARATION>()->pNameToken->Value;

			function->getArg(i)->setName(paramName);

			// set the ByRef attribute on parameters passed byref
			if (paramModifiers[i] == TypeModifier::Reference) {
				function->addAttributeAtIndex(i+1, llvm::Attribute::getWithByRefType(*state.Context, paramSubTypes[i]));
			}

			/* ReadOnly is not the same as const and LLVM does not accept ReadOnly on anything other than pointers
			// set the ReadOnly attribute on parameters passed as const
			if (paramVarTypes[i] == VariableType::Constant) {
				function->addAttributeAtIndex(i+1, llvm::Attribute::get(*state.Context, llvm::Attribute::AttrKind::ReadOnly));
			}
			*/
		}

		return function;
	}

#pragma endregion

#pragma region Expressions

	llvm::Value* generateConstructorExpression(ModuleTable& moduleTable, LLVMState& state, const CONSTRUCTOR& constructorExpression)
	{
		// create mangled structure name (if needed)
		std::string structName = NodeBuffer::GetMangledName(constructorExpression.pType->pNameToken->Value, constructorExpression.pType->GenericArguments);

		llvm::StructType* structType = static_cast<llvm::StructType*>(state.NamedValues.GetType(structName));

		if (!structType || structType->getTypeID() != llvm::Type::StructTyID)
		{
			logErrorAtCurrentPosition(nullptr, // constructorExpressionNode.StructIdentifierID
									"Unknown struct type '{0}'!", structName);
			return nullptr;
		}

		// create a mutable variable at the end of the insertion block
		llvm::IRBuilder<> tempBuilder(state.Builder->GetInsertBlock(), state.Builder->GetInsertBlock()->end());
		llvm::AllocaInst* structPtr = tempBuilder.CreateAlloca(structType, nullptr);

		// go through the list of initializers and initialize all members
		for (int i = 0; i < constructorExpression.Arguments.size(); i++)
		{
			// get the name of the member to initialize
			const std::string_view memberName = constructorExpression.Arguments[i].first->Value;

			NamedValues::StructMemberInfo memberInfo = state.NamedValues.GetMemberIndex(structName, memberName);

			if (memberInfo.memberIndex == -1)
			{
				logErrorAtCurrentPosition(nullptr, // constructorExpressionNode.MemberIdentifierIDs[i]
										"Type '{0}' is not a struct!", structName);
				return nullptr;
			}

			if (memberInfo.memberIndex == -2)
			{
				logErrorAtCurrentPosition(nullptr, // constructorExpressionNode.MemberIdentifierIDs[i]
										"Struct '{0}' does not have a member '{1}'!", structName, memberName);
				return nullptr;
			}

			// set the type that we are expecting for each structure element
			TypeSubtypePair identifierType = { structType->getElementType(memberInfo.memberIndex), memberInfo.containedType };

			llvm::Value* expressionVal = generateExpression(moduleTable, state, *constructorExpression.Arguments[i].second, identifierType);

			if (!expressionVal)
			{
				logErrorAtCurrentPosition(nullptr, // nodeID
										"Error evaluating '{0}.{1}'!", structName, memberName);
				return nullptr;
			}

			// convert our index to the format required by llvm
			std::vector<llvm::Value*> indices = getGEPIndex(state, memberInfo.memberIndex);

			if (llvm::isa<llvm::PointerType>(structType->getElementType(memberInfo.memberIndex))) {
				// TBD: check if special processing is needed in case of pointers (e.g. strings?)
			}

			llvm::Value* memberPtr = state.Builder->CreateGEP(structType, structPtr, indices, "memberptr");

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
		if (isa<llvm::PointerType>(elementType))
		{
			// for pointers, we need the underlying object type
			elementType = identifierType.containedType;
		}

		if (elementType == nullptr) {
			logErrorAtCurrentPosition(nullptr, // TBD *pointerInitializerNode... 
										"Unknown element type!");
			return nullptr;
		}

		// if this is a pointer to an array, we need the underlying element type
		if (llvm::isa<llvm::VectorType>(elementType)) {
			elementType = static_cast<llvm::VectorType*>(elementType)->getElementType();
		}
		llvm::PointerType* pointerType = llvm::PointerType::get(elementType, 0);	// note that elementType is not actually stored by llvm

		// get the value to set for each element
		llvm::Value* defaultValue = generateExpression(moduleTable, state, *pointerInitializerNode.pValue, { elementType, nullptr });
		// try to convert the value to the expected type	
		convertValueToType(state, defaultValue, elementType);

		// now get the count of objects to create on the heap
		llvm::Value* count;
		if (nullptr == pointerInitializerNode.pSize) {
			// creating a single object
			count = llvm::ConstantInt::get(*state.Context, llvm::APInt(64, 1, true));
		}
		else {
			count = generateExpression(moduleTable, state, *pointerInitializerNode.pSize, { llvm::IntegerType::getInt64Ty(*state.Context), nullptr });
		}

		// create a mutable variable on the heap
		llvm::Type* PointerType = llvm::Type::getInt64Ty(*state.Context);	// pointers are 64-bit values
		llvm::Constant* AllocSize = llvm::ConstantExpr::getSizeOf(elementType);
		AllocSize = llvm::ConstantExpr::getTruncOrBitCast(AllocSize, PointerType);
		llvm::CallInst* Ptr = state.Builder->CreateMalloc(PointerType, elementType, AllocSize, count);

		// create code to go through the list of members and initialize them to the given value
		// we need to create the loop within the llvm code because we don' t know before hand how many objects we are creating
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
		llvm::Value* memberPtr = tempBuilder.CreateGEP(pointerType, Ptr, current, "memberptr");
		tempBuilder.CreateStore(defaultValue, memberPtr, "savetmp");

		// increment loop variable (start) by step value
		current = tempBuilder.CreateAdd(current, step, "addtmp");
		tempBuilder.CreateStore(current, start);

		// emit the condition
		llvm::Value* conditionVal = tempBuilder.CreateICmpUGE(current, count, "eqtmp");;
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
											const TypeSubtypePair& expectedType)
	{
		//
		// move Identifier ;
		//
		TypeSubtypePair identifierType = { nullptr, nullptr };
		PtrValuePair ptrValue = generateIdentifier(moduleTable, state, *pointerMoveNode.pVariable, identifierType);

		// retrieve the name of the identifier in the right-side node in order to remove it from the NamedValues map
		const Token& rightNode = *pointerMoveNode.pVariable->pNameToken;
		const std::string name(rightNode.Value);
		state.NamedValues.RemoveValue(name);

		return ptrValue.Ptr;
	}
		
	llvm::Value* generateInitializerListExpression(ModuleTable& moduleTable, LLVMState& state, const INITIALIZER_LIST& initListExpressionNode,
													const TypeSubtypePair& expectedType)
	{
		//
		// Array initialization in the form of: { EXPRESSION, EXPRESSION, ... }
		//

		if (!expectedType.type || (expectedType.type->getTypeID() != llvm::Type::FixedVectorTyID && expectedType.type->getTypeID() != llvm::Type::ScalableVectorTyID))
		{
			logErrorAtCurrentPosition(nullptr, // TBD: initListExpressionNode...
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

			llvm::Value* expressionVal = generateExpression(moduleTable, state, *initListExpressionNode.Values[i], identifierType);

			if (!expressionVal)
			{
				logErrorAtCurrentPosition(nullptr, // TBD: initListExpressionNode.Values[i]...
											"Error evaluating expression!");
				return nullptr;
			}

			// convert our index to the format required by GEP operations
			std::vector<llvm::Value*> indices = getGEPIndex(state, i);

			llvm::Value* memberPtr = state.Builder->CreateGEP(vectorType, arrayPtr, indices, "memberptr");

			state.Builder->CreateStore(expressionVal, memberPtr, "savetmp");
		}

		return state.Builder->CreateLoad(arrayPtr->getAllocatedType(), arrayPtr);	// result contains the whole initialized array
	}

	llvm::Value* generateVariableDefinitionExpression(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DEFINITION& variableDefinition)
	{
		//
		// ( var | const ) identifier:TYPE = expression;
		//
		TypeSubtypePair identifierType = { nullptr, nullptr };

		// create the declaration
		llvm::Value* declarationVal = generateVariableDeclaration(moduleTable, state, *variableDefinition.pDeclaration, identifierType);

		if (!declarationVal)
		{
			logErrorAtCurrentPosition(nullptr, // TBD: variableDefinition...
										"Error evaluating expression!");
			return nullptr;
		}

		// create the value expression
		llvm::Value* value = generateExpression(moduleTable, state, *variableDefinition.pValue, identifierType);

		if (!value)
		{
			logErrorAtCurrentPosition(nullptr, // TBD 
									"Error evaluating expression!");
			return nullptr;
		}

		if (llvm::isa<llvm::AllocaInst>(declarationVal)) {
			llvm::AllocaInst* alloc = static_cast<llvm::AllocaInst*>(declarationVal);
			convertValueToType(state, value, alloc->getAllocatedType());
		}

		// store the value into the alloca
		return state.Builder->CreateStore(value, declarationVal);
	}

	PtrValuePair generateArrayAccessExpression(ModuleTable& moduleTable, LLVMState& state, const ARRAY_ACCESS& arrayAccessExpression,
												const TypeSubtypePair& identifierType)
	{
		llvm::Value* memberIndex = generateExpression(moduleTable, state, *arrayAccessExpression.pIndex, { llvm::IntegerType::getInt64Ty(*state.Context), nullptr });
		TypeSubtypePair tempType = { nullptr, nullptr };
		PtrValuePair left = { nullptr, nullptr, false };

		// handle identifier
		if (arrayAccessExpression.pArray->Is<PRIMARY>()
			&& arrayAccessExpression.pArray->Get<PRIMARY>()->Is<VARIABLE>()
			)
		{
			TypeSubtypePair identifierType = { nullptr, nullptr };
			left = generateIdentifier(moduleTable, state, *arrayAccessExpression.pArray->Get<PRIMARY>()->Get<VARIABLE>(), tempType);
		}
		else
		// not a variable, must be an expression that returns an array
		{
			left.Value = generateExpression(moduleTable, state, *arrayAccessExpression.pArray, tempType);
			left.Ptr = nullptr;		// we don't have a pointer to a variable, so set this to null
		}

		// get the type of the left
		llvm::Type* leftType = left.Value->getType();

		if (leftType->getTypeID() != llvm::Type::FixedVectorTyID && leftType->getTypeID() != llvm::Type::ScalableVectorTyID)
		{
			logErrorAtCurrentPosition(nullptr, // TBD: arrayAccessExpression.ArrayExpressionID
										"Expected vector type!");
			return {};
		}

		llvm::VectorType* vectorType = static_cast<llvm::VectorType*>(leftType);
		llvm::Value* memberPtr = nullptr;
		llvm::Value* memberValue = nullptr;

		if (left.Ptr != nullptr) {
			// case of an identifier			
			std::vector<llvm::Value*> indices(2);
			indices[0] = llvm::ConstantInt::get(*state.Context, llvm::APInt(32, 0, true));
			indices[1] = memberIndex;

			memberPtr = state.Builder->CreateGEP(vectorType, left.Ptr, indices, "memberptr");
			memberValue = state.Builder->CreateLoad(vectorType->getElementType(), memberPtr, "loadtmp");
		}
		else {
			// case of an expression, we don't have a pointer to the array
			memberValue = state.Builder->CreateExtractElement(left.Value, memberIndex, "extractelem");
		}

		// array of pointers, we need to load the underlying value
		if (llvm::isa<llvm::PointerType>(memberValue->getType())) {
			if (tempType.containedType == nullptr) {
				logErrorAtCurrentPosition(nullptr, // TBD: moduleTable.GetErrorInfo(arrayAccessExpression.ArrayExpressionID)
										"Array contains pointers of unknown type!");
				return {};
			}

			llvm::Value* Value = state.Builder->CreateLoad(tempType.containedType, memberValue, "loadtmp");

			return PtrValuePair{ .Ptr = memberValue, .Value = Value, .isConst = left.isConst };
		}

		return PtrValuePair{ .Ptr = memberPtr, .Value = memberValue, .isConst = left.isConst };
	}

	PtrValuePair generateMemberAccessExpression(ModuleTable& moduleTable, LLVMState& state, const MEMBER_ACCESS& memberAccessExpression,
												const TypeSubtypePair& expectedType)
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
			TypeSubtypePair identifierType = { nullptr, nullptr };
			left = generateIdentifier(moduleTable, state, *memberAccessExpression.pObject->Get<PRIMARY>()->Get<VARIABLE>(), identifierType);
		}
		else
		// neither an identifier nor a nested member access, must be an expression that returns a structure
		{
			left.Value = generateExpression(moduleTable, state, *memberAccessExpression.pObject, expectedType);
			left.Ptr = nullptr;		// we don't have a pointer to a variable, so set this to null
		}

		// get the type of the left
		llvm::Type* leftType = left.Value->getType();

		if (leftType->getTypeID() != llvm::Type::StructTyID)
		{
			logErrorAtCurrentPosition(nullptr, // TBD: moduleTable.GetErrorInfo(memberAccessExpressionNode.LeftID)
									"Expected struct type!");
			return {};
		}

		llvm::StructType* structType = static_cast<llvm::StructType*>(leftType);

		// get the name of the struct type
		const std::string_view structName = structType->getName();

		// get the index of the member
		NamedValues::StructMemberInfo memberInfo = state.NamedValues.GetMemberIndex(structName, memberName);

		if (memberInfo.memberIndex == -1)
		{
			logErrorAtCurrentPosition(nullptr, // TBD: memberAccessExpressionNode.RightID
									"Type '{0}' is not a struct type!", structName);
			return {};
		}

		if (memberInfo.memberIndex == -2)
		{
			logErrorAtCurrentPosition(nullptr, // TBD: memberAccessExpressionNode.RightID
									"Struct '{0}' does not have a member '{1}'!", structName, memberName);
			return {};
		}

		if (llvm::isa<llvm::PointerType>(structType->getTypeAtIndex(memberInfo.memberIndex))) {
			// TBD: check if special processing is needed in case of pointers (e.g. strings?)
		}

		if (left.Ptr == nullptr) {
			// case where the structure is returned by evaluating an expression, we can directly access the structure member
			std::vector<unsigned int> indices(1);
			indices[0] = memberInfo.memberIndex;
			llvm::Value* memberValue = state.Builder->CreateExtractValue(left.Value, indices);
			return PtrValuePair{ .Ptr = nullptr, .Value = memberValue, .isConst = left.isConst };
		}
		else {
			// get the member index in the format required by llvm
			std::vector<llvm::Value*> indices = getGEPIndex(state, memberInfo.memberIndex);

			llvm::Value* memberPtr = state.Builder->CreateGEP(structType, left.Ptr, indices, "memberptr");
			llvm::Value* memberValue = state.Builder->CreateLoad(structType->getTypeAtIndex(memberInfo.memberIndex), memberPtr, "loadtmp");
			return PtrValuePair{ .Ptr = memberPtr, .Value = memberValue, .isConst = left.isConst };
		}
	}

	llvm::Value* generateFunctionCallExpression(ModuleTable& moduleTable, LLVMState& state, const FUNCTION_CALL& functionCallExpressionNode)
	{
		std::string functionName(functionCallExpressionNode.pFunctionNameToken->Value);
		std::vector<llvm::Value*> args;
		int argi = 0;	// argument index currently processed
		llvm::Value* result = nullptr;
		llvm::Function* calleeFunc = nullptr;
		FUNCTION_TYPE* pCalleeFunctionType = nullptr;	// in addition to the LLVM function definition, we need the original function definition in order to properly handle const parameters
		bool insertSelfAsFirstParam = false;		// indicates whether the first parameter should be the variable value in the case of member function calls
		std::vector<EXPRESSION*> Arguments(functionCallExpressionNode.Arguments);
													// creating a copy of the arguments as we might need to insert new elements

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
		if (functionCallExpressionNode.pTypeOrVariableName != nullptr)
		{
			const std::string varOrTypeName(functionCallExpressionNode.pTypeOrVariableName->pNameToken->Value);
			if (state.NamedValues.GetValue(varOrTypeName) != nullptr)
			{
				// non-static member function call
				std::string_view typeName = state.NamedValues.GetTypeName(state.NamedValues.GetValue(varOrTypeName)->value->getAllocatedType());
				std::string mangledName = NodeBuffer::GetMangledName(typeName, functionName);

				// look up the function in the global module table
				calleeFunc = state.Module->getFunction(mangledName);
				if (calleeFunc == nullptr)
				{
					logErrorAtCurrentPosition(functionCallExpressionNode.pFunctionNameToken, "Cannot find member function '{0}:{1}'!", typeName, functionName);
					goto error;
				}

				// also retrieve the original function definition
				SearchResult<FUNCTION_DEFINITION> result = moduleTable.GetFunctionDefinition(mangledName);
				
				if (result.Code == SearchResultCode::NotFound)
				{
					// TODO: error not found
					goto error;
				}
				else if (result.Code == SearchResultCode::Inaccessible)
				{
					// TODO: error inaccessible
					goto error;
				}
				else
				{
					pCalleeFunctionType = result.pDefiniton->pFunctionType;
				}

				/* TBD:check that the first argument is of the right type
				if (calleeFunc->arg_size() < 1 || calleeFunc->getArg(0)->getType() != state.NamedValues.GetValue(varOrTypeName)->value->getAllocatedType()) {
					logErrorAtCurrentPosition(functionCallExpressionNode.pFunctionNameToken, "The first parameter of member function '{0}' is not of the right type!", functionName);
					goto error;
				}
				*/
				insertSelfAsFirstParam = true;
			}

			else if (state.NamedValues.GetType(varOrTypeName) != nullptr)
			{
				// static member function call
				std::string mangledName = NodeBuffer::GetMangledName(varOrTypeName, functionName);

				// look up the function in the global module table
				calleeFunc = state.Module->getFunction(mangledName);

				// also retrieve the original function definition
				SearchResult<FUNCTION_DEFINITION> result = moduleTable.GetFunctionDefinition(mangledName);

				if (result.Code == SearchResultCode::NotFound)
				{
					// TODO: error not found
					goto error;
				}
				else if (result.Code == SearchResultCode::Inaccessible)
				{
					// TODO: error inaccessible
					goto error;
				}
				else
				{
					pCalleeFunctionType = result.pDefiniton->pFunctionType;
				}
			}
			else
			{
				logErrorAtCurrentPosition(functionCallExpressionNode.pTypeOrVariableName->pNameToken, "Not a variable or type name '{0}'!", varOrTypeName);
				goto error;
			}
		}
		else
		{
			// look up the function in the global module table
			calleeFunc = state.Module->getFunction(std::string(functionName));

			// also retrieve the original function definition
			SearchResult<FUNCTION_DEFINITION> funcResult = moduleTable.GetFunctionDefinition(functionName);

			if (funcResult.Code == SearchResultCode::NotFound)
			{
				// could also be an extern
				SearchResult<EXTERN_DEFINITION> externResult = moduleTable.GetExternDefinition(functionName);

				if (externResult.Code == SearchResultCode::NotFound)
				{
					// TODO: error not found
					goto error;
				}

				else if (externResult.Code == SearchResultCode::Inaccessible)
				{
					// TODO: error inaccessible
					goto error;
				}

				else
				{
					pCalleeFunctionType = externResult.pDefiniton->pFunctionType;
				}
			}
			else if (funcResult.Code == SearchResultCode::Inaccessible)
			{
				// TODO: error inaccessible
				goto error;
			}
			else
			{
				pCalleeFunctionType = funcResult.pDefiniton->pFunctionType;
			}
		}

		// function not found, it might not have been processed yet
		// check if function is already in the parser and process it
		if (!calleeFunc)
		{
			ASSERT(false, "Defining a new function while another function is already being defined is not working, all functions have to be pre-processed in Generate!");

#if 0
			auto functionResult = moduleTable.GetFunctionDefinition(functionCallExpressionNode.pFunctionNameToken, /*emitErrors*/true);
			if (functionResult.Code == SearchErrorCode::Success)
			{

				// function found, process it
				calleeFunc = generateFunctionDefinition(moduleTable, state, *functionResult.pValue);

			}
			else
			{
				auto externResult = moduleTable.GetExternDefinition(functionCallExpressionNode.pFunctionNameToken, /*emitErrors*/true);

				if (externResult.Code == SearchErrorCode::Success)
				{
					// external function found, process it
					calleeFunc = generateExternDefinition(moduleTable, state, *externResult.pValue);
				}
			}
#endif
		}

		if (!calleeFunc
			|| !pCalleeFunctionType)
		{
			logErrorAtCurrentPosition(functionCallExpressionNode.pFunctionNameToken, "Cannot find function '{0}'!", functionName);
			goto error;
		}

		// first parameter is &Self
		if (insertSelfAsFirstParam)
		{
			VARIABLE var{ functionCallExpressionNode.pTypeOrVariableName->pNameToken };
			TypeSubtypePair identifierType = {};
			PtrValuePair ptrValue = generateIdentifier(moduleTable, state, var, identifierType);
			if (ptrValue.Ptr == nullptr)
			{
				logErrorAtCurrentPosition(functionCallExpressionNode.pTypeOrVariableName->pNameToken, "Error evaluating variable '{0}'!", functionCallExpressionNode.pTypeOrVariableName->pNameToken->Value);
				goto error;
			}

			args.push_back(ptrValue.Ptr);
			argi = 1;
		}

		// if the function was found, check for argument count mismatch (not counting the additional arguments that we added
		if (!calleeFunc->isVarArg() && calleeFunc->arg_size() != Arguments.size() + argi)
		{
			logErrorAtCurrentPosition(functionCallExpressionNode.pFunctionNameToken, "Function '{0}' argument mismatch!", functionName);
			goto error;
		}

		// evaluate all the arguments
		for (size_t i = 0; i < Arguments.size(); i++, argi++)
		{
			const EXPRESSION& argument = *Arguments[i];

			// for functions with a variable number of arguments, check the argument types till the first optional argument
			// e.g. if the function has 2 mandatory arguments and a number of optional arguments, check for only the first 2 types 
			TypeSubtypePair argType = { (i < calleeFunc->arg_size() ? calleeFunc->getArg(argi)->getType() : nullptr), nullptr };

			llvm::Value* argVal = nullptr;

			// check if the function parameter was declared as const
			bool isConst = isFunctionParameterConst(*pCalleeFunctionType, argi); 

			// check if the parameter is passed byref, in which case it should be a variable and we will pass the address of the identifier
			auto attr = calleeFunc->getAttributeAtIndex(i + 1, llvm::Attribute::AttrKind::ByRef);
			bool isByRef = attr.hasAttribute(llvm::Attribute::AttrKind::ByRef);
			if (isByRef) {
				bool foundVariable = false;
				// we need a variable to pass it byref, unless it is a const byref
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
							logErrorAtCurrentPosition(nullptr, // TBD: argumentID
								"Function argument {0} expects a reference to a variable preceded by the & symbol!", i + 1);
							goto error;
						}
						argVal = left.Ptr;
						foundVariable = true;
					}
				}

				// argument is not in the form of &variable, if we are expecting a constant, continue evaluating the expression
				if (!foundVariable) {
					if (!isConst) {
						logErrorAtCurrentPosition(nullptr, // TBD: argumentID
							"Function argument {0} expects a reference to a variable preceded by the & symbol!", i + 1);
						goto error;
					}

					argVal = generateExpression(moduleTable, state, argument, argType);
					// create a pointer to the evaluated expression and pass the pointer as argument
					llvm::AllocaInst* ptr = state.Builder->CreateAlloca(argVal->getType());
					state.Builder->CreateStore(argVal, ptr);
					argVal = ptr;
				}
			}
			else {
				argVal = generateExpression(moduleTable, state, argument, argType);

				if (argVal == nullptr)
				{
					logErrorAtCurrentPosition(nullptr, // nodeID
											"Error evaluating expression!");
					goto error;
				}

				if (i < calleeFunc->arg_size()) {
					convertValueToType(state, argVal, calleeFunc->getArg(argi)->getType());

					if (argVal->getType() != calleeFunc->getArg(argi)->getType())
					{
						logErrorAtCurrentPosition(nullptr, // TBD: argumentID
							"Function argument {0} expects value of type '{1}' but given type is '{2}'!", i + 1,
							state.NamedValues.GetTypeName(calleeFunc->getArg(argi)->getType()),
							state.NamedValues.GetTypeName(argVal->getType()));
						goto error;
					}
				}
			}
			args.push_back(argVal);
		}

		result = state.Builder->CreateCall(calleeFunc, args);

	error:
		return result;
	}

	llvm::Value* generateEnclosedExpression(ModuleTable& moduleTable, LLVMState& state, const ENCLOSED_EXPRESSION& enclosedExpressionNode,
												const TypeSubtypePair& expectedType)
	{
		return generateExpression(moduleTable, state, *enclosedExpressionNode.pExpression, expectedType);
	}

	llvm::Value* generateBinaryExpression(ModuleTable& moduleTable, LLVMState& state, const BINARY& binaryExpressionNode)
	{
		TypeSubtypePair leftExpressionType = { nullptr, nullptr };
		llvm::Value* leftVal = generateExpression(moduleTable, state, *binaryExpressionNode.pLeft, leftExpressionType);
		TypeSubtypePair rightExpressionType = { leftVal->getType(), nullptr};	// when computing rightVal, try to obtain a value of same type as leftVal
		llvm::Value* rightVal = generateExpression(moduleTable, state, *binaryExpressionNode.pRight, rightExpressionType);

		if (!leftVal || !rightVal)
		{
			logErrorAtCurrentPosition(nullptr, // TBD: nodeID, 
									"Error evaluating expression!");
			return nullptr;
		}

		llvm::Type* leftType = leftVal->getType();
		llvm::Type* rightType = rightVal->getType();

		// check that types match
		if (leftType != rightType)
		{
			logErrorAtCurrentPosition(nullptr, // TBD: nodeID
				"Binary operator must be applied to matching types! Current types are '{0}' and '{1}'.",
				state.NamedValues.GetTypeName(leftType), state.NamedValues.GetTypeName(rightType));
			return nullptr;
		}

		bool isFloatingPoint = leftType->isFloatingPointTy();

		std::string_view operatorStr = binaryExpressionNode.pOpToken->Value;

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
											const TypeSubtypePair& expectedType)
	{
		std::string_view operatorStr = unaryExpressionNode.pOpToken->Value;
		llvm::Value* result = nullptr;

		if (operatorStr == "-")
		{
			llvm::Value* expressionVal = generateExpression(moduleTable, state, *unaryExpressionNode.pExpression, expectedType);

			if (expressionVal)
			{
				result = state.Builder->CreateFNeg(expressionVal, "negtmp");
			}
		}
		else
			if (operatorStr == "!")
			{
				llvm::Value* expressionVal = generateExpression(moduleTable, state, *unaryExpressionNode.pExpression, expectedType);

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

	llvm::Value* generatePrimaryExpression(ModuleTable& moduleTable, LLVMState& state, const PRIMARY& primary,
									const TypeSubtypePair& expectedType)
	{
		//
		// using PRIMARY = VariantNode<LITERAL, VARIABLE, VARIABLE_DEFINITION, FUNCTION_CALL, CONSTRUCTOR,
		//  	POINTER_INIT, POINTER_MOVE, INITIALIZER_LIST, ENCLOSED_EXPRESSION>;
		///
		llvm::Value* result = nullptr;
		TypeSubtypePair identiferType = expectedType;

		if (primary.Is<LITERAL>()) {
			result = generateLiteral(moduleTable, state, *primary.Get<LITERAL>(), expectedType);
		}
		else if (primary.Is<VARIABLE>()) {
				result = generateIdentifier(moduleTable, state, *primary.Get<VARIABLE>(), identiferType).Value;
		}
		else if (primary.Is<CONSTRUCTOR>()) {
			result = generateConstructorExpression(moduleTable, state, *primary.Get<CONSTRUCTOR>());
		}
		else if (primary.Is<POINTER_INIT>()) {
			result = generatePointerInitializerExpression(moduleTable, state, *primary.Get<POINTER_INIT>(), expectedType);
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
			result = generateArrayAccessExpression(moduleTable, state, *primary.Get<ARRAY_ACCESS>(), expectedType).Value;
		}
		else if (primary.Is<MEMBER_ACCESS>()) {
			result = generateMemberAccessExpression(moduleTable, state, *primary.Get<MEMBER_ACCESS>(), expectedType).Value;
		}
		else if (primary.Is<FUNCTION_CALL>()) {
			result = generateFunctionCallExpression(moduleTable, state, *primary.Get<FUNCTION_CALL>());
		}
		else if (primary.Is<ENCLOSED_EXPRESSION>()) {
			result = generateEnclosedExpression(moduleTable, state, *primary.Get<ENCLOSED_EXPRESSION>(), expectedType);
		}
		else {
			ASSERT(false, "Unknown primary expression node kind!");
		}

		// TODO : The returned identifierType should match the expectedType. If it doesn't we should either return an error or convert to the right type
		assert(expectedType.type == nullptr || expectedType.type->isPointerTy() || identiferType == expectedType);

		return result;
	}

	llvm::Value* generateAssignmentExpression(ModuleTable& moduleTable, LLVMState& state, const ASSIGNMENT& assignment,
												const TypeSubtypePair& expectedType)
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
			TypeSubtypePair identifierType = expectedType;
			ptrValue = generateIdentifier(moduleTable, state, *assignment.pVariable->Get<PRIMARY>()->Get<VARIABLE>(), identifierType);
		}
		else if (assignment.pVariable->Is<POSTFIX>()
			&& assignment.pVariable->Get<POSTFIX>()->Is<ARRAY_ACCESS>())
		{
			ptrValue = generateArrayAccessExpression(moduleTable, state, *assignment.pVariable->Get<POSTFIX>()->Get<ARRAY_ACCESS>(), expectedType);
		}

		ptr = ptrValue.Ptr;

		if (!ptr)
		{
			logErrorAtCurrentPosition(nullptr, // TBD: nodeID 
									"Error evaluating identifier!");
			return nullptr;
		}

		if (ptrValue.isConst) {
			logErrorAtCurrentPosition(nullptr, // TBD: nodeID 
				"Assigning a value to a constant!");
			return nullptr;
		}

		llvm::Value* expressionVal = generateExpression(moduleTable, state, *assignment.pValue, expectedType);

		if (!expressionVal)
		{
			logErrorAtCurrentPosition(nullptr, // TBD: nodeID
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

	llvm::Value* generateExpression(ModuleTable& moduleTable, LLVMState& state, const EXPRESSION& expressionNode,
														const TypeSubtypePair& expectedType)
	{
		llvm::Value* result = nullptr;

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
				result = generateArrayAccessExpression(moduleTable, state, *postfix.Get<ARRAY_ACCESS>(), expectedType).Value;
			}
			else if (postfix.Is<MEMBER_ACCESS>()) {
				result = generateMemberAccessExpression(moduleTable, state, *postfix.Get<MEMBER_ACCESS>(), expectedType).Value;
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
			llvm::Value* initVal = generateExpression(moduleTable, state, *forLoop.pInitialization, { nullptr, nullptr });

			if (initVal == nullptr)
			{
				logErrorAtCurrentPosition(nullptr, // TBD: forLoop...
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
				logErrorAtCurrentPosition(nullptr,	// TBD: forLoop... 
								"Error evaluating expression!");
				return nullptr;
			}
		}

		// emit step value
		if (forLoop.pIncrement != nullptr) {
			llvm::Value* stepVal = generateExpression(moduleTable, state, *forLoop.pIncrement, { nullptr, nullptr });

			if (stepVal == nullptr)
			{
				logErrorAtCurrentPosition(nullptr,	// TBD: forLoop... 
								"Error evaluating expression!");
				return nullptr;
			}
		}

		// emit the condition
		llvm::Value* conditionVal;
		if (forLoop.pCondition != nullptr) {
			conditionVal = generateExpression(moduleTable, state, *forLoop.pCondition, { nullptr, nullptr });

			if (conditionVal == nullptr)
			{
				logErrorAtCurrentPosition(nullptr,	// TBD: forLoop... 
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
		llvm::Value* conditionVal = generateExpression(moduleTable, state, *whileLoop.pCondition, { nullptr, nullptr });

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

	llvm::Value* generateIfStatement(ModuleTable& moduleTable, LLVMState& state, const IF_STATEMENT& ifStatement)
	{

		llvm::Value* conditionVal = generateExpression(moduleTable, state, *ifStatement.pCondition, { nullptr, nullptr });

		if (conditionVal == nullptr)
		{
			return nullptr;
		}

		conditionVal = convertToBool(state, conditionVal);

		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();

		ASSERT(func != nullptr, "No function to insert into!");

		// create blocks for the then and else cases
		llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(*state.Context, "then", func);
		llvm::BasicBlock* elseBlock = llvm::BasicBlock::Create(*state.Context, "else");
		llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(*state.Context, "ifcont");

		state.Builder->CreateCondBr(conditionVal, thenBlock, elseBlock);

		// emit then value
		state.Builder->SetInsertPoint(thenBlock);

		llvm::Value* thenVal = generateStatement(moduleTable, state, *ifStatement.pStatement);

		if (thenVal == nullptr)
		{
			return nullptr;
		}

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

		return llvm::ConstantInt::getTrue(*state.Context);
	}

	llvm::Value* generateStatementBlock(ModuleTable& moduleTable, LLVMState& state, const STATEMENT_BLOCK& statementBlock)
	{
		llvm::Value* statementVal = nullptr;

		for (const STATEMENT* statement : statementBlock.Statements)
		{
			statementVal = generateStatement(moduleTable, state, *statement);

			if (statementVal == nullptr)
			{
				return nullptr;
			}
		}

		return statementVal;
	}

	llvm::Value* generateReturnStatement(ModuleTable& moduleTable, LLVMState& state, const RETURN& returnStatement)
	{
		// handle void return
		if (returnStatement.pValue == nullptr)
		{
			if (state.CurrentReturnValue != nullptr)
			{
				logErrorAtCurrentPosition(nullptr, // TBD: nodeID
							"Function expects a return value of type '{0}'!", state.NamedValues.GetTypeName(state.CurrentReturnValue->getType()));

				return nullptr;
			}

			return state.Builder->CreateBr(state.FuncExitBlock);
		}

		llvm::Value* expressionValue = generateExpression(moduleTable, state, *returnStatement.pValue, { nullptr, nullptr });

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
			logErrorAtCurrentPosition(nullptr, // TBD: nodeID,
				"Function has return type '{0}' but provided return value is of type '{1}'!",
				state.CurrentReturnValue == nullptr ? "void" : state.NamedValues.GetTypeName(state.CurrentReturnValue->getType()),
				state.NamedValues.GetTypeName(expressionValue->getType()));
			return nullptr;
		}

		// store the return value and branch to the exit block
		state.Builder->CreateStore(expressionValue, state.CurrentReturnValue);
		return state.Builder->CreateBr(state.FuncExitBlock);
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
		else {
			ASSERT(false, "Unknown statement node kind!");
		}

		return result;
	}

#pragma endregion

#pragma region Definitions

	llvm::Function* generateExternDefinition(ModuleTable& moduleTable, LLVMState& state, const EXTERN_DEFINITION& externDefinition )
	{
		FUNCTION_DEFINITION functionDefinition = { nullptr, externDefinition.pNameToken, externDefinition.pFunctionType, nullptr };
		return generateFunctionDeclaration(moduleTable, state, functionDefinition);
	}

	llvm::Value* generateVariableDefinition(ModuleTable& moduleTable, LLVMState& state, const VARIABLE_DEFINITION& variableDefinition)
	{
		return generateVariableDefinitionExpression(moduleTable, state, variableDefinition);
	}

	llvm::Type* generateStructDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_IDENTIFIER& typeIdentifier,
											const STRUCT_TYPE& structDefinition, const std::vector<TYPE*>& genericArguments)
	{
		llvm::StructType* structType = nullptr;
		int memberIndex, arg;
		std::vector<llvm::Type*> memberTypes;
		std::unordered_map<std::string_view, NamedValues::StructMemberInfo> memberNames;
		std::unordered_map<std::string, std::string> genericMap;

		// create mangled structure name (if needed)
		std::string structName = NodeBuffer::GetMangledName(typeIdentifier.pNameToken->Value, genericArguments);

		if (genericArguments.size() != typeIdentifier.GenericParameters.size())
		{
			logErrorAtCurrentPosition(typeIdentifier.pNameToken,
				"Invalid number of arguments for generic type '{0}'!", structName);
			goto failed;
		}

		// create the map from the structure's generic types to the actual types requested by the caller
		arg = 0;
		for (auto& type : typeIdentifier.GenericParameters) {
			if (genericArguments[arg]->Type.Is<TYPE_NAME>()) {
				genericMap[std::string(type->pIdentifierToken->Value)] = genericArguments[arg]->Type.Get<TYPE_NAME>()->pNameToken->Value;
			}
			arg++;
		}

		// get a vector of member types
		memberIndex = 0;
		for (auto id : structDefinition.Members)
		{
			TypeModifier modifier = TypeModifier::None;
			TypeSubtypePair identifierType = generateTypeIdentifier(moduleTable, state, *id.second, nullptr, modifier, &genericMap);

			if (!identifierType.type)
			{
				logErrorAtCurrentPosition(typeIdentifier.pNameToken,
									"Invalid structure member type for structure '{0}'!", structName);
				goto failed;
			}

			memberTypes.push_back(identifierType.type);

			// retrieve member name and add it to memberNames map
			const std::string_view memberName = id.first->Value;

			memberNames[memberName] = { memberIndex++, identifierType.containedType };
		}

		structType = llvm::StructType::create(*state.Context, memberTypes, structName);

		if (!structType)
		{
			logErrorAtCurrentPosition(typeIdentifier.pNameToken,
								"Invalid structure type for structure '{0}'!", structName);
			goto failed;
		}

		state.NamedValues.InsertType(structName, structType, true, // isStruct
										memberNames);

failed:
		return structType;
	}

	llvm::Function* generateFunctionDefinition(ModuleTable& moduleTable, LLVMState& state, 
												const FUNCTION_DEFINITION& functionDefinition)
	{
		//
		// Generate either a global function definition or a member function definition
		// If type is other than an empty string, we are defining a member function, in which case the name of the function is defined as Type@Name in LLVM
		//
		llvm::Function* func = generateFunctionDeclaration(moduleTable, state, functionDefinition);

		if (func == nullptr)
		{
			return nullptr;
		}

		// push a new scope for the function
		state.NamedValues.PushScope(func->getName().str());

		// create a new basic block to start insertion into
		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", func);
		state.Builder->SetInsertPoint(entryBlock);

		// create an alloca for the function's return value, if any
		llvm::AllocaInst* previousReturnValue = state.CurrentReturnValue;
		if (func->getReturnType() != nullptr && !func->getReturnType()->isVoidTy()) {
			state.CurrentReturnValue = createEntryBlockAlloca(func, "", func->getReturnType());
		}

		// create allocations for function arguments
		for (size_t index = 0; index < func->arg_size(); index++)
		{
			llvm::Argument& arg = *func->getArg(index);

			// check that we do not have a named value with the same name
			if (state.NamedValues.GetValue(arg.getName().str()))
			{
				logErrorAtCurrentPosition(nullptr, // TBD: nodeID
								"Variable '{0}' already defined!", arg.getName().str());

				func->eraseFromParent();
				state.NamedValues.FreeHeapPointers(*state.Builder);
				state.NamedValues.PopScope();
				state.CurrentReturnValue = nullptr;
				return nullptr;
			}

			llvm::Attribute attr = arg.getAttribute(llvm::Attribute::AttrKind::ByRef);
			llvm::Type* subType = nullptr;
			llvm::AllocaInst* allocaInst = nullptr;
			if (attr.hasAttribute(llvm::Attribute::AttrKind::ByRef)) {
				subType = arg.getParamByRefType();
			}
			// create an alloca for this variable
			allocaInst = createEntryBlockAlloca(func, arg.getName().str(), arg.getType());

			// store the initial value into the alloca
			state.Builder->CreateStore(&arg, allocaInst);

			// add arguments to named values
			state.NamedValues.InsertValue(std::string(arg.getName()), allocaInst, subType, isFunctionParameterConst(*functionDefinition.pFunctionType, index), false);
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

		state.CurrentReturnValue = nullptr;

		return func;
	}

	llvm::Type* generateTypeDefinition(ModuleTable& moduleTable, LLVMState& state, const TYPE_DEFINITION& typeDefinition,
										const std::vector<TYPE*>& genericArguments)
	{
		llvm::Type* result = nullptr;

		if (typeDefinition.pType->Type.Is<STRUCT_TYPE>()) {
			const STRUCT_TYPE& structType = *typeDefinition.pType->Type.Get<STRUCT_TYPE>();
			result = generateStructDefinition(moduleTable, state, *typeDefinition.pTypeIdentifier, structType, genericArguments);
		}
		else if (typeDefinition.pType->Type.Is<TYPE_NAME>()) {
			const TYPE_NAME* typeName = typeDefinition.pType->Type.Get<TYPE_NAME>();
			result = getTypeFromTypeName(moduleTable, state, typeName->pNameToken, typeName->GenericArguments, nullptr);

			// insert the new type (which is a copy of the type it refers to) into the namedValues of the current scope
			if (result != nullptr) {
				bool isStruct = result->isStructTy();
				if (isStruct) {
					std::unordered_map<std::string_view, NamedValues::StructMemberInfo> structMembers;
					state.NamedValues.GetStructMembers(state.NamedValues.GetTypeName(result), structMembers);
					state.NamedValues.InsertType(std::string(typeDefinition.pTypeIdentifier->pNameToken->Value), result, true, structMembers);
				}
				else {
					state.NamedValues.InsertType(std::string(typeName->pNameToken->Value), result, false);
				}
			}
		}
		else {
			ASSERT(false, "Type definition not implemented!");
		}

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
			logErrorAtCurrentPosition(nullptr, "Cannot find main entry point!");
			return false;
		}

		/* pre-process all types definitions as these might be used throughout the code
		* for this to work properly we need to process the types in the order they appear in the file because one type can reference a previous type
		* for now, we are processing each type the first time it is encountered in the code
		* also note that generic structures cannot be pre-processed as the generic type will not be defined
		for (auto t = moduleTable.TypeDefinitions.begin(); t != moduleTable.TypeDefinitions.end(); t++) {
			generateTypeDefinition(moduleTable, state, *t->second);
		}
		*/

		for (auto& [name, module] : moduleTable.GetModules())
		{
			// pre-process all extern function definitions
			for (auto& [name, func] : module.GetNodeBuffer().GetExternDefinitions())
			{
				generateExternDefinition(moduleTable, state, *func.pDefinition);
			}

			// pre-process all function definitions leaving the main function till the end
			for (auto& [name, func] : module.GetNodeBuffer().GetFunctionDefinitions())
			{
				if (name != "main")
				{
					generateFunctionDefinition(moduleTable, state, *func.pDefinition);
				}
			}
		}

		llvm::Function* result = generateFunctionDefinition(moduleTable, state, *pMainFunction);

		std::error_code errorCode;
		llvm::raw_fd_ostream out("c:\\temp\\out.ll", errorCode);
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
			auto mainAddr = jit->lookup("main");
			if (auto E = mainAddr.takeError()) {
				ASSERT(false, "Cannot find main entry point!");
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
