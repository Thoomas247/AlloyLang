#include "CodeGenerator.hpp"

namespace AlloyCompiler
{

#pragma region Util

	struct PtrValuePair
	{
		llvm::Value* Ptr = nullptr;
		llvm::Value* Value = nullptr;
	};

	template<typename ...Args>
	constexpr void logErrorAtPosition(const TokenBuffers& tokenBuffers, TokenID tokenID, const std::string& format, Args && ...args)
	{
		const Location& location = tokenBuffers.GetLocation(tokenID);

		Log::Error("Error at location ({0} : {1}):", location.Line, location.Column);
		Log::Error("\t{0}", tokenBuffers.GetLine(location.LineStart));
		Log::Error("\t{0}^", std::string(location.Column - 1, ' '));
		Log::Error("\t{0}{1}", std::string(location.Column - 1, ' '), std::vformat(format, std::make_format_args(args...)));

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

	std::string unescapeString(const AlloyCompiler::SmallStringView& str)
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

#pragma endregion

	// forward declarations
	llvm::Value* generateExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID, 
									const TypeSubtypePair& expectedType);

#pragma region Literals

	llvm::Value* generateLiteral(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
								const TypeSubtypePair& identifierType)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::LITERAL, "Expected LITERAL!");

		const LITERAL& literalNode = nodeBuffers.GetNode(nodeID).Literal;

		const std::string_view literalStr = tokenBuffers.GetValue(literalNode.InfoTokenID).ToStringView();
		llvm::Value* result = nullptr;

		// get the correct type of value that we are generating
		llvm::Type* thisType = identifierType.type;
		if (thisType && isa<llvm::VectorType>(thisType)) {
			thisType = static_cast<llvm::VectorType*>(thisType)->getElementType();		// for arrays, we want the element type, not the array itself
		}

		switch (literalNode.Kind)
		{
		case LITERAL::Type::Integer:
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

		case LITERAL::Type::Float:
		{
			double dblValue;
			std::from_chars_result temp = std::from_chars(literalStr.data(), literalStr.data() + literalStr.size(), dblValue);
			result = llvm::ConstantFP::get(*state.Context, llvm::APFloat(dblValue));
			break;
		}

		case LITERAL::Type::Boolean:
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

		case LITERAL::Type::String:
			result = state.Builder->CreateGlobalStringPtr(literalStr);
			break;

		case LITERAL::Type::Character:
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

	PtrValuePair generateIdentifier(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
									TypeSubtypePair& identifierType)
	{
		if (nodeBuffers.GetNode(nodeID).Kind != NodeKind::IDENTIFIER) {
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Expected IDENTIFIER!");
			return {};
		}

		const IDENTIFIER& node = nodeBuffers.GetNode(nodeID).Identifier;
		const std::string_view name = tokenBuffers.GetValue(node.IdentifierTokenID).ToStringView();

		// check if we have a local variable with this name
		ValueTypePair* valueTypePair = state.NamedValues.GetValue(name);

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
				return PtrValuePair{ .Ptr = valueOrPtr, .Value = Value };
			}
			else {
				identifierType.containedType = valueTypePair->containedType;
				return PtrValuePair{ .Ptr = valueTypePair->value, .Value = valueOrPtr };
			}
		}

		// check if we have a global
		llvm::GlobalVariable* globalVar = state.Module->getGlobalVariable(name, true);

		if (globalVar)
		{
			llvm::Value* value = state.Builder->CreateLoad(globalVar->getValueType(), globalVar, name);
			return PtrValuePair{ .Ptr = globalVar, .Value = value };
		}

		logErrorAtPosition(tokenBuffers, node.IdentifierTokenID, "Unknown variable name '{0}'!", name);
		return {};
	}

	TypeSubtypePair generateTypeIdentifier(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
														TYPE_IDENTIFIER::Modifier& modifier
		)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::TYPE_IDENTIFIER, "Expected TYPE_IDENTIFIER!");

		TypeSubtypePair identifierType = { nullptr, nullptr };

		const TYPE_IDENTIFIER& typeIdentifierNode = nodeBuffers.GetNode(nodeID).TypeIdentifier;
		const Node& identifierOrTypeIdentifierNode = nodeBuffers.GetNode(typeIdentifierNode.IdentifierOrTypeIdentifierID);

		// let the caller know if this is a pointer, a reference or none
		modifier = typeIdentifierNode.Mod;

		// handle non-array types
		if (identifierOrTypeIdentifierNode.Kind == NodeKind::IDENTIFIER)
		{
			const IDENTIFIER& identifierNode = identifierOrTypeIdentifierNode.Identifier;
			const std::string_view name = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

			identifierType.type = state.NamedValues.GetType(name);

			if (!identifierType.type)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(typeIdentifierNode.IdentifierOrTypeIdentifierID),
					"Unknown type name '{0}'!", name);
				identifierType = { nullptr, nullptr };
				goto exit;
			}
		}

		// handle array types
		else
		{
			ASSERT(identifierOrTypeIdentifierNode.Kind == NodeKind::TYPE_IDENTIFIER, "Expected type identifier node!");

			uint64_t arraySize = 0;

			// get size if given
			if (typeIdentifierNode.ArraySizeID != ERROR_NODE_ID)
			{
				const LITERAL& arraySizeNode = nodeBuffers.GetNode(typeIdentifierNode.ArraySizeID).Literal;
				const std::string_view arraySizeStr = tokenBuffers.GetValue(arraySizeNode.InfoTokenID).ToStringView();

				std::from_chars(arraySizeStr.data(), arraySizeStr.data() + arraySizeStr.size(), arraySize);

				if (arraySize == 0)
				{
					logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(typeIdentifierNode.ArraySizeID), "Array size must be greater than 0!");
					identifierType = { nullptr, nullptr };
					goto exit;
				}
			}

			const NodeID elementTypeIdentifierNode = typeIdentifierNode.IdentifierOrTypeIdentifierID;
			TYPE_IDENTIFIER::Modifier modifier = TYPE_IDENTIFIER::Modifier::None;
			identifierType = generateTypeIdentifier(tokenBuffers, nodeBuffers, state, elementTypeIdentifierNode, modifier);
			llvm::Type* elementType = identifierType.type;

			if (identifierType.type == nullptr)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(elementTypeIdentifierNode), "Unknown array element type!");
				identifierType = { nullptr, nullptr };
				goto exit;
			}

			switch (modifier) {
				case TYPE_IDENTIFIER::Modifier::None:
					break;

				case TYPE_IDENTIFIER::Modifier::Pointer:
				case TYPE_IDENTIFIER::Modifier::Reference:
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
														(arraySize == 0 ? true : false) /*scalable*/);
		}

		// return the identifier type and subtype
		// e.g. var array : [i64; 10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }; when we encounter var array : [i64; 10] we know that we are
		// initializing an i64 array of 10 elements
	exit:
		return identifierType;
	}

#pragma endregion

#pragma region Declarations

	TypeSubtypePair generateTypeDeclaration(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::TYPE_DECLARATION, "Expected TYPE_DECLARATION!");

		// TODO: var and const

		const TYPE_DECLARATION& typeDeclarationNode = nodeBuffers.GetNode(nodeID).TypeDeclaration;
		TYPE_IDENTIFIER::Modifier modifier = TYPE_IDENTIFIER::Modifier::None;
		return generateTypeIdentifier(tokenBuffers, nodeBuffers, state, typeDeclarationNode.TypeIdentifierID, modifier);
	}

	llvm::Value* generateValueDeclaration(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
											TypeSubtypePair& identifierType)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::VALUE_DECLARATION, "Expected VALUE_DECLARATION!");

		const VALUE_DECLARATION& valueDeclarationNode = nodeBuffers.GetNode(nodeID).ValueDeclaration;
		const IDENTIFIER& identifierNode = nodeBuffers.GetNode(valueDeclarationNode.IdentifierID).Identifier;

		const std::string_view name = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();
		TYPE_IDENTIFIER::Modifier modifier = TYPE_IDENTIFIER::Modifier::None;
		identifierType = generateTypeIdentifier(tokenBuffers, nodeBuffers, state, valueDeclarationNode.TypeIdentifierID, modifier);

		if (!identifierType.type)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Variable '{0}' type error!", name);
			return nullptr;
		}

		switch (modifier) {
			case TYPE_IDENTIFIER::Modifier::None:
				break;

			case TYPE_IDENTIFIER::Modifier::Pointer:
			{
				// allocating a pointer to this type
				identifierType.containedType = identifierType.type;
				identifierType.type = llvm::PointerType::get(identifierType.containedType, 0);
				break;
			}

			case TYPE_IDENTIFIER::Modifier::Reference:
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
		state.NamedValues.InsertValue(name, allocaInst, identifierType.containedType, (modifier == TYPE_IDENTIFIER::Modifier::Pointer));

		return allocaInst;
	}

	llvm::Value* generateFunctionDeclaration(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::FUNCTION_DECLARATION, "Expected FUNCTION_DECLARATION!");

		const FUNCTION_DECLARATION& functionDeclarationNode = nodeBuffers.GetNode(nodeID).FunctionDeclaration;
		const IDENTIFIER& identifier = nodeBuffers.GetNode(functionDeclarationNode.IdentifierID).Identifier;

		const std::string_view name = tokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView();

		// check if function already exists
		if (state.Module->getFunction(name))
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Function '{0}' already defined!", name);
			return nullptr;
		}

		// retrieve the parameter types
		std::vector<llvm::Type*> paramTypes;
		std::vector<llvm::Type*> paramSubTypes;
		std::vector<TYPE_IDENTIFIER::Modifier> paramModifiers;

		for (NodeID parameterID : functionDeclarationNode.ParameterIDs)
		{
			const VALUE_DECLARATION& valueDeclaration = nodeBuffers.GetNode(parameterID).ValueDeclaration;
			TYPE_IDENTIFIER::Modifier modifier = TYPE_IDENTIFIER::Modifier::None;
			TypeSubtypePair identifierType = generateTypeIdentifier(tokenBuffers, nodeBuffers, state, valueDeclaration.TypeIdentifierID, modifier);

			if (!identifierType.type)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Function '{0}' parameter type error!", name);
				return nullptr;
			}

			// Reference should be passed as pointers
			if (modifier == TYPE_IDENTIFIER::Modifier::Reference) {
				paramTypes.push_back(llvm::PointerType::get(identifierType.type, 0));
				paramSubTypes.push_back(identifierType.type);
			}
			else {
				paramTypes.push_back(identifierType.type);
				paramSubTypes.push_back(nullptr);
			}
			paramModifiers.push_back(modifier);
		}

		// retrieve the return type
		llvm::Type* returnType = llvm::Type::getVoidTy(*state.Context);

		if (functionDeclarationNode.ReturnTypeID != ERROR_NODE_ID)
		{
			const TYPE_DECLARATION& typeDeclaration = nodeBuffers.GetNode(functionDeclarationNode.ReturnTypeID).TypeDeclaration;
			returnType = generateTypeDeclaration(tokenBuffers, nodeBuffers, state, functionDeclarationNode.ReturnTypeID).type;
		}

		if (!returnType)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Function '{0}' return type error!", name);
			return nullptr;
		}

		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, true /*functionDeclarationNode.IsVariadic*/);

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
			const VALUE_DECLARATION& valueDeclaration = nodeBuffers.GetNode(functionDeclarationNode.ParameterIDs[i]).ValueDeclaration;
			const IDENTIFIER& identifier = nodeBuffers.GetNode(valueDeclaration.IdentifierID).Identifier;

			const std::string_view paramName = tokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView();

			function->getArg(i)->setName(paramName);

			// set the ByRef attribute on parameters passed byref
			if (paramModifiers[i] == TYPE_IDENTIFIER::Modifier::Reference) {
				function->addAttributeAtIndex(i+1, llvm::Attribute::getWithByRefType(*state.Context, paramSubTypes[i]));
			}
		}

		return function;
	}

#pragma endregion

#pragma region Expressions

	llvm::Value* generateConstructorExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::CONSTRUCTOR_EXPRESSION, "Expected CONSTRUCTOR_EXPRESSION!");

		const CONSTRUCTOR_EXPRESSION& constructorExpressionNode = nodeBuffers.GetNode(nodeID).ConstructorExpression;

		const IDENTIFIER& structIdentifier = nodeBuffers.GetNode(constructorExpressionNode.StructIdentifierID).Identifier;
		const std::string_view structName = tokenBuffers.GetValue(structIdentifier.IdentifierTokenID).ToStringView();

		llvm::StructType* structType = static_cast<llvm::StructType*>(state.NamedValues.GetType(structName));

		if (!structType || structType->getTypeID() != llvm::Type::StructTyID)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(constructorExpressionNode.StructIdentifierID), "Unknown struct type '{0}'!", structName);
			return nullptr;
		}

		// create a mutable variable at the end of the insertion block
		llvm::IRBuilder<> tempBuilder(state.Builder->GetInsertBlock(), state.Builder->GetInsertBlock()->end());
		llvm::AllocaInst* structPtr = tempBuilder.CreateAlloca(structType, nullptr);

		// go through the list of initializers and initialize all members
		for (int i = 0; i < constructorExpressionNode.MemberIdentifierIDs.size(); i++)
		{
			// get the name of the member to initialize
			const IDENTIFIER& memberIdentifier = nodeBuffers.GetNode(constructorExpressionNode.MemberIdentifierIDs[i]).Identifier;
			const std::string_view memberName = tokenBuffers.GetValue(memberIdentifier.IdentifierTokenID).ToStringView();

			NamedValues::StructMemberInfo memberInfo = state.NamedValues.GetMemberIndex(structName, memberName);

			if (memberInfo.memberIndex == -1)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(constructorExpressionNode.MemberIdentifierIDs[i]), "Type '{0}' is not a struct!", structName);
				return nullptr;
			}

			if (memberInfo.memberIndex == -2)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(constructorExpressionNode.MemberIdentifierIDs[i]), "Struct '{0}' does not have a member '{1}'!", structName, memberName);
				return nullptr;
			}

			// set the type that we are expecting for each structure element
			TypeSubtypePair identifierType = { structType->getElementType(memberInfo.memberIndex), memberInfo.containedType };

			llvm::Value* expressionVal = generateExpression(tokenBuffers, nodeBuffers, state, constructorExpressionNode.MemberValueIDs[i], identifierType);

			if (!expressionVal)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Error evaluating '{0}.{1}'!", structName, memberName);
				return nullptr;
			}

			// convert our index to the format required by llvm
			std::vector<llvm::Value*> indices = getGEPIndex(state, memberInfo.memberIndex);

			if (llvm::isa<llvm::PointerType>(structType->getElementType(memberInfo.memberIndex))) {
				assert(false);		// TODO: not implemented
			}

			llvm::Value* memberPtr = state.Builder->CreateGEP(structType, structPtr, indices, "memberptr");

			convertValueToType(state, expressionVal, structType->getElementType(memberInfo.memberIndex));

			state.Builder->CreateStore(expressionVal, memberPtr, "savetmp");
		}

		return state.Builder->CreateLoad(structPtr->getAllocatedType(), structPtr);	// result contains the whole initialized structure
	}

	llvm::Value* generatePointerInitializerExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
													  const TypeSubtypePair& identifierType)
	{
		//
		// new ( EXPRESSION | ( [ EXPRESSION; EXPRESSION ] ) ) ;
		//
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::POINTER_INITIALIZER_EXPRESSION, "Expected POINTER_INITIALIZER_EXPRESSION!");
		const POINTER_INITIALIZER_EXPRESSION& pointerInitializerNode = nodeBuffers.GetNode(nodeID).PointerInitializerExpression;

		llvm::Type* elementType = identifierType.type;
		if (isa<llvm::PointerType>(elementType))
		{
			// for pointers, we need the underlying object type
			elementType = identifierType.containedType;
		}

		if (elementType == nullptr) {
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Unknown element type!");
			return nullptr;
		}

		// if this is a pointer to an array, we need the underlying element type
		if (llvm::isa<llvm::VectorType>(elementType)) {
			elementType = static_cast<llvm::VectorType*>(elementType)->getElementType();
		}
		llvm::PointerType* pointerType = llvm::PointerType::get(elementType, 0);	// note that elementType is not actually stored by llvm

		// get the value to set for each element
		llvm::Value* defaultValue = generateExpression(tokenBuffers, nodeBuffers, state, pointerInitializerNode.ValueID, { elementType, nullptr });
		// try to convert the value to the expected type	
		convertValueToType(state, defaultValue, elementType);

		// now get the count of objects to create on the heap
		llvm::Value* count;
		if (ERROR_NODE_ID == pointerInitializerNode.CountID) {
			// creating a single object
			count = llvm::ConstantInt::get(*state.Context, llvm::APInt(64, 1, true));
		}
		else {
			count = generateExpression(tokenBuffers, nodeBuffers, state, pointerInitializerNode.CountID, { llvm::IntegerType::getInt64Ty(*state.Context), nullptr });
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

	llvm::Value* generatePointerMoveExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
											const TypeSubtypePair& expectedType)
	{
		//
		// move Identifier ;
		//
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::POINTER_MOVE_EXPRESSION, "Expected POINTER_MOVE_EXPRESSION!");
		const POINTER_MOVE_EXPRESSION& pointerMoveNode = nodeBuffers.GetNode(nodeID).PointerMoveExpression;

		TypeSubtypePair identifierType = { nullptr, nullptr };
		PtrValuePair ptrValue = generateIdentifier(tokenBuffers, nodeBuffers, state, pointerMoveNode.IdentifierID, identifierType);

		// retrieve the name of the identifier in the right-side node in order to remove it from the NamedValues map
		const IDENTIFIER& rightNode = nodeBuffers.GetNode(pointerMoveNode.IdentifierID).Identifier;
		const std::string_view name = tokenBuffers.GetValue(rightNode.IdentifierTokenID).ToStringView();
		state.NamedValues.RemoveValue(name);

		return ptrValue.Ptr;
	}
		
	llvm::Value* generateInitializerListExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
													const TypeSubtypePair& expectedType)
	{
		//
		// Array initialization in the form of: { EXPRESSION, EXPRESSION, ... }
		//
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::INITIALIZER_LIST_EXPRESSION, "Expected INITIALIZER_LIST_EXPRESSION!");

		const INITIALIZER_LIST_EXPRESSION& initListExpressionNode = nodeBuffers.GetNode(nodeID).InitializerListExpression;

		if (!expectedType.type || (expectedType.type->getTypeID() != llvm::Type::FixedVectorTyID && expectedType.type->getTypeID() != llvm::Type::ScalableVectorTyID))
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Unknown vector type!");
			return nullptr;
		}

		llvm::VectorType* vectorType = static_cast<llvm::VectorType*>(expectedType.type);

		// create a mutable variable at the end of the insertion block
		llvm::IRBuilder<> tempBuilder(state.Builder->GetInsertBlock(), state.Builder->GetInsertBlock()->end());
		llvm::AllocaInst* arrayPtr = tempBuilder.CreateAlloca(vectorType, nullptr);

		// go through the list of initializers and initialize all members
		for (int i = 0; i < initListExpressionNode.ValueIDs.size(); i++)
		{
			// set the type that we are expecting for each array element
			TypeSubtypePair identifierType = { vectorType->getElementType(), expectedType.containedType };

			llvm::Value* expressionVal = generateExpression(tokenBuffers, nodeBuffers, state, initListExpressionNode.ValueIDs[i], identifierType);

			if (!expressionVal)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Error evaluating expression!");
				return nullptr;
			}

			// convert our index to the format required by GEP operations
			std::vector<llvm::Value*> indices = getGEPIndex(state, i);

			llvm::Value* memberPtr = state.Builder->CreateGEP(vectorType, arrayPtr, indices, "memberptr");

			state.Builder->CreateStore(expressionVal, memberPtr, "savetmp");
		}

		return state.Builder->CreateLoad(arrayPtr->getAllocatedType(), arrayPtr);	// result contains the whole initialized array
	}


	llvm::Value* generateValueDefinitionExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::VALUE_DEFINITION_EXPRESSION, "Expected VALUE_DEFINITION_EXPRESSION!");

		const VALUE_DEFINITION_EXPRESSION& valueDefinitionExpressionNode = nodeBuffers.GetNode(nodeID).ValueDefinitionExpression;
		TypeSubtypePair identifierType = { nullptr, nullptr };

		// create the declaration
		llvm::Value* declarationVal = generateValueDeclaration(tokenBuffers, nodeBuffers, state, valueDefinitionExpressionNode.ValueDeclarationID, identifierType);

		if (!declarationVal)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Error evaluating expression!");
			return nullptr;
		}

		// create the value expression
		llvm::Value* value = generateExpression(tokenBuffers, nodeBuffers, state, valueDefinitionExpressionNode.ValueID, identifierType);

		if (!value)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Error evaluating expression!");
			return nullptr;
		}

		if (llvm::isa<llvm::AllocaInst>(declarationVal)) {
			llvm::AllocaInst* alloc = static_cast<llvm::AllocaInst*>(declarationVal);
			convertValueToType(state, value, alloc->getAllocatedType());
		}

		// store the value into the alloca
		return state.Builder->CreateStore(value, declarationVal);
	}

	PtrValuePair generateArrayAccessExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
												const TypeSubtypePair& identifierType)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::ARRAY_ACCESS_EXPRESSION, "Expected ARRAY_ACCESS_EXPRESSION!");

		const ARRAY_ACCESS_EXPRESSION& arrayAccessExpressionNode = nodeBuffers.GetNode(nodeID).ArrayAccessExpression;

		llvm::Value* memberIndex = generateExpression(tokenBuffers, nodeBuffers, state, arrayAccessExpressionNode.IndexExpressionID, { llvm::IntegerType::getInt64Ty(*state.Context), nullptr });
		TypeSubtypePair tempType = { nullptr, nullptr };
		PtrValuePair left = { nullptr, nullptr };

		// handle identifier
		if (nodeBuffers.GetNode(arrayAccessExpressionNode.ArrayExpressionID).Kind == NodeKind::IDENTIFIER)
		{
			TypeSubtypePair identifierType = { nullptr, nullptr };
			left = generateIdentifier(tokenBuffers, nodeBuffers, state, arrayAccessExpressionNode.ArrayExpressionID, tempType);
		}
		else
		// not an identifier, must be an expression that returns an array
		{
			left.Value = generateExpression(tokenBuffers, nodeBuffers, state, arrayAccessExpressionNode.ArrayExpressionID, tempType);
			left.Ptr = nullptr;		// we don't have a pointer to a variable, so set this to null
		}

		// get the type of the left
		llvm::Type* leftType = left.Value->getType();

		if (leftType->getTypeID() != llvm::Type::FixedVectorTyID && leftType->getTypeID() != llvm::Type::ScalableVectorTyID)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(arrayAccessExpressionNode.ArrayExpressionID), "Expected vector type!");
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
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(arrayAccessExpressionNode.ArrayExpressionID), "Array contains pointers of unknown type!");
				return {};
			}

			llvm::Value* Value = state.Builder->CreateLoad(tempType.containedType, memberValue, "loadtmp");

			return PtrValuePair{ .Ptr = memberValue, .Value = Value };
		}

		return PtrValuePair{ .Ptr = memberPtr, .Value = memberValue };
	}

	PtrValuePair generateMemberAccessExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
												const TypeSubtypePair& expectedType)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::MEMBER_ACCESS_EXPRESSION, "Expected MEMBER_ACCESS_EXPRESSION!");

		const MEMBER_ACCESS_EXPRESSION& memberAccessExpressionNode = nodeBuffers.GetNode(nodeID).MemberAccessExpression;

		const IDENTIFIER& right = nodeBuffers.GetNode(memberAccessExpressionNode.RightID).Identifier;
		const std::string_view memberName = tokenBuffers.GetValue(right.IdentifierTokenID).ToStringView();

		PtrValuePair left;

		// handle nested member access
		if (nodeBuffers.GetNode(memberAccessExpressionNode.LeftID).Kind == NodeKind::MEMBER_ACCESS_EXPRESSION)
		{
			left = generateMemberAccessExpression(tokenBuffers, nodeBuffers, state, memberAccessExpressionNode.LeftID, expectedType);
		}
		// handle identifier
		else if (nodeBuffers.GetNode(memberAccessExpressionNode.LeftID).Kind == NodeKind::IDENTIFIER)
		{
			TypeSubtypePair identifierType = { nullptr, nullptr };
			left = generateIdentifier(tokenBuffers, nodeBuffers, state, memberAccessExpressionNode.LeftID, identifierType);
		}
		else
		// neither an identifier nor a nested member access, must be an expression that returns a structure
		{
			left.Value = generateExpression(tokenBuffers, nodeBuffers, state, memberAccessExpressionNode.LeftID, expectedType);
			left.Ptr = nullptr;		// we don't have a pointer to a variable, so set this to null
		}

		// get the type of the left
		llvm::Type* leftType = left.Value->getType();

		if (leftType->getTypeID() != llvm::Type::StructTyID)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(memberAccessExpressionNode.LeftID), "Expected struct type!");
			return {};
		}

		llvm::StructType* structType = static_cast<llvm::StructType*>(leftType);

		// get the name of the struct type
		const std::string_view structName = structType->getName();

		// get the index of the member
		NamedValues::StructMemberInfo memberInfo = state.NamedValues.GetMemberIndex(structName, memberName);

		if (memberInfo.memberIndex == -1)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(memberAccessExpressionNode.RightID), "Type '{0}' is not a struct type!", structName);
			return {};
		}

		if (memberInfo.memberIndex == -2)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(memberAccessExpressionNode.RightID), "Struct '{0}' does not have a member '{1}'!", structName, memberName);
			return {};
		}

		if (llvm::isa<llvm::PointerType>(structType->getTypeAtIndex(memberInfo.memberIndex))) {
			assert(false);	// TODO: not implemented
		}

		if (left.Ptr == nullptr) {
			// case where the structure is returned by evaluating an expression, we can directly access the structure member
			std::vector<unsigned int> indices(1);
			indices[0] = memberInfo.memberIndex;
			llvm::Value* memberValue = state.Builder->CreateExtractValue(left.Value, indices);
			return PtrValuePair{ .Ptr = nullptr, .Value = memberValue };
		}
		else {
			// get the member index in the format required by llvm
			std::vector<llvm::Value*> indices = getGEPIndex(state, memberInfo.memberIndex);

			llvm::Value* memberPtr = state.Builder->CreateGEP(structType, left.Ptr, indices, "memberptr");
			llvm::Value* memberValue = state.Builder->CreateLoad(structType->getTypeAtIndex(memberInfo.memberIndex), memberPtr, "loadtmp");
			return PtrValuePair{ .Ptr = memberPtr, .Value = memberValue };
		}
	}

	llvm::Value* generateFunctionCallExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::FUNCTION_CALL_EXPRESSION, "Expected FUNCTION_CALL_EXPRESSION!");

		const FUNCTION_CALL_EXPRESSION& functionCallExpressionNode = nodeBuffers.GetNode(nodeID).FunctionCallExpression;
		const IDENTIFIER& identifierNode = nodeBuffers.GetNode(functionCallExpressionNode.IdentifierID).Identifier;

		const std::string_view functionName = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

		// look up the name in the global module table
		llvm::Function* calleeFunc = state.Module->getFunction(std::string(functionName));

		if (!calleeFunc)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Cannot find function '{0}'!", functionName);
			return nullptr;
		}

		// if the function was found, check for argument mismatch
		if (calleeFunc->arg_size() != functionCallExpressionNode.ArgumentIDs.size())
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Function '{0}' argument mismatch!", functionName);
			return nullptr;
		}

		// evaluate all the arguments
		std::vector<llvm::Value*> args;

		for (size_t i = 0; i < functionCallExpressionNode.ArgumentIDs.size(); i++)
		{
			const NodeID argumentID = functionCallExpressionNode.ArgumentIDs[i];
			llvm::Value* argVal = nullptr;

			// check if the parameter is passed byref, in which case it should be an identifier and we will pass the address of the identifier
			auto attr = calleeFunc->getAttributeAtIndex(i + 1, llvm::Attribute::AttrKind::ByRef);
			if (attr.hasAttribute(llvm::Attribute::AttrKind::ByRef)) {
				// we need an identifier to pass it byref
				if (nodeBuffers.GetNode(argumentID).Kind != NodeKind::IDENTIFIER) {
					logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(argumentID),
						"Function argument {0} expects an lvalue!", i + 1);
					return nullptr;
				}
				TypeSubtypePair identifierType = { nullptr, nullptr };
				argVal = generateIdentifier(tokenBuffers, nodeBuffers, state, argumentID, identifierType).Ptr;
				/* TBD the argument type is a pointer in case of references, how to compare the underlying type?
				if (identifierType.type != calleeFunc->getArg(i)->getType())
				{
					logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(argumentID),
						"Function argument {0} expects value of type '{1}' but given type is '{2}'!", i + 1,
						state.NamedValues.GetTypeName(calleeFunc->getArg(i)->getType()),
						state.NamedValues.GetTypeName(identifierType.type));
					return nullptr;
				}
				*/
			}
			else {
				argVal = generateExpression(tokenBuffers, nodeBuffers, state, argumentID, { calleeFunc->getArg(i)->getType(), nullptr });

				if (argVal == nullptr)
				{
					logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Error evaluating expression!");
					return nullptr;
				}

				convertValueToType(state, argVal, calleeFunc->getArg(i)->getType());

				if (argVal->getType() != calleeFunc->getArg(i)->getType())
				{
					logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(argumentID),
						"Function argument {0} expects value of type '{1}' but given type is '{2}'!", i + 1,
						state.NamedValues.GetTypeName(calleeFunc->getArg(i)->getType()),
						state.NamedValues.GetTypeName(argVal->getType()));
					return nullptr;
				}
			}
			args.push_back(argVal);
		}

		return state.Builder->CreateCall(calleeFunc, args, "calltmp");
	}

	llvm::Value* generateEnclosedExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
												const TypeSubtypePair& expectedType)
	{
		const ENCLOSED_EXPRESSION& enclosedExpressionNode = nodeBuffers.GetNode(nodeID).EnclosedExpression;

		return generateExpression(tokenBuffers, nodeBuffers, state, enclosedExpressionNode.ExpressionID, expectedType);
	}

	llvm::Value* generateBinaryExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const BINARY_EXPRESSION& binaryExpressionNode = nodeBuffers.GetNode(nodeID).BinaryExpression;

		TypeSubtypePair leftExpressionType = { nullptr, nullptr };
		llvm::Value* leftVal = generateExpression(tokenBuffers, nodeBuffers, state, binaryExpressionNode.LeftID, leftExpressionType);
		TypeSubtypePair rightExpressionType = { leftVal->getType(), nullptr};	// when computing rightVal, try to obtain a value of same type as leftVal
		llvm::Value* rightVal = generateExpression(tokenBuffers, nodeBuffers, state, binaryExpressionNode.RightID, rightExpressionType);

		if (!leftVal || !rightVal)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Error evaluating expression!");
			return nullptr;
		}

		llvm::Type* leftType = leftVal->getType();
		llvm::Type* rightType = rightVal->getType();

		// check that types match
		if (leftType != rightType)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Binary operator must be applied to matching types! Current types are '{0}' and '{1}'.",
				state.NamedValues.GetTypeName(leftType), state.NamedValues.GetTypeName(rightType));
			return nullptr;
		}

		bool isFloatingPoint = leftType->isFloatingPointTy();

		std::string_view operatorStr = tokenBuffers.GetValue(binaryExpressionNode.OperatorTokenID).ToStringView();

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

	llvm::Value* generateUnaryExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
											const TypeSubtypePair& expectedType)
	{
		const UNARY_EXPRESSION& unaryExpressionNode = nodeBuffers.GetNode(nodeID).UnaryExpression;

		std::string_view operatorStr = tokenBuffers.GetValue(unaryExpressionNode.OperatorTokenID).ToStringView();
		llvm::Value* result = nullptr;

		if (operatorStr == "-")
		{
			llvm::Value* expressionVal = generateExpression(tokenBuffers, nodeBuffers, state, unaryExpressionNode.OperandID, expectedType);

			if (expressionVal)
			{
				result = state.Builder->CreateFNeg(expressionVal, "negtmp");
			}
		}
		else
			if (operatorStr == "!")
			{
				llvm::Value* expressionVal = generateExpression(tokenBuffers, nodeBuffers, state, unaryExpressionNode.OperandID, expectedType);

				if (expressionVal)
				{
					result = state.Builder->CreateNot(expressionVal, "nottmp");
				}
			}
			else
				if (operatorStr == "&")
				{
					TypeSubtypePair tempType = { nullptr, nullptr };
					PtrValuePair left = generateIdentifier(tokenBuffers, nodeBuffers, state, unaryExpressionNode.OperandID, tempType);
					result = left.Ptr;
				}
				else
				{
					ASSERT(false, "Unknown unary operator!");
				}

		return result;
	}

	llvm::Value* generatePrimaryExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
									const TypeSubtypePair& expectedType)
	{
		const Node& node = nodeBuffers.GetNode(nodeID);
		llvm::Value* result = nullptr;
		TypeSubtypePair identiferType = expectedType;

		switch (node.Kind)
		{
		case NodeKind::IDENTIFIER:
			result = generateIdentifier(tokenBuffers, nodeBuffers, state, nodeID, identiferType).Value;
			break;

		case NodeKind::LITERAL:
			result = generateLiteral(tokenBuffers, nodeBuffers, state, nodeID, expectedType);
			break;

		case NodeKind::CONSTRUCTOR_EXPRESSION:
			result = generateConstructorExpression(tokenBuffers, nodeBuffers, state, nodeID);
			break;

		case NodeKind::POINTER_INITIALIZER_EXPRESSION:
			result = generatePointerInitializerExpression(tokenBuffers, nodeBuffers, state, nodeID, expectedType);
			break;

		case NodeKind::POINTER_MOVE_EXPRESSION:
			result = generatePointerMoveExpression(tokenBuffers, nodeBuffers, state, nodeID, expectedType);
			break;

		case NodeKind::INITIALIZER_LIST_EXPRESSION:
			result = generateInitializerListExpression(tokenBuffers, nodeBuffers, state, nodeID, expectedType);
			break;

		case NodeKind::VALUE_DEFINITION_EXPRESSION:
			result = generateValueDefinitionExpression(tokenBuffers, nodeBuffers, state, nodeID);
			break;

		case NodeKind::ARRAY_ACCESS_EXPRESSION:
			result = generateArrayAccessExpression(tokenBuffers, nodeBuffers, state, nodeID, expectedType).Value;
			break;

		case NodeKind::MEMBER_ACCESS_EXPRESSION:
			result = generateMemberAccessExpression(tokenBuffers, nodeBuffers, state, nodeID, expectedType).Value;
			break;

		case NodeKind::FUNCTION_CALL_EXPRESSION:
			result = generateFunctionCallExpression(tokenBuffers, nodeBuffers, state, nodeID);
			break;

		case NodeKind::ENCLOSED_EXPRESSION:
			result = generateEnclosedExpression(tokenBuffers, nodeBuffers, state, nodeID, expectedType);
			break;

		default:
			ASSERT(false, "Unknown primary expression node kind!");
			break;
		}

		// TODO : The returned identifierType should match the expectedType. If it doesn't we should either return an error or convert to the right type
		assert(expectedType.type == nullptr || expectedType.type->isPointerTy() || identiferType == expectedType);

		return result;
	}

	llvm::Value* generateAssignmentExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
												const TypeSubtypePair& expectedType)
	{
		const ASSIGNMENT_EXPRESSION& assignmentExpressionNode = nodeBuffers.GetNode(nodeID).AssignmentExpression;

		const Node& identifierOrMemberAccessNode = nodeBuffers.GetNode(assignmentExpressionNode.IdentifierOrMemberAccessID);

		llvm::Value* ptr = nullptr;

		if (identifierOrMemberAccessNode.Kind == NodeKind::MEMBER_ACCESS_EXPRESSION)
		{
			ptr = generateMemberAccessExpression(tokenBuffers, nodeBuffers, state, assignmentExpressionNode.IdentifierOrMemberAccessID, expectedType).Ptr;
		}

		else if (identifierOrMemberAccessNode.Kind == NodeKind::IDENTIFIER)
		{
			TypeSubtypePair identifierType = expectedType;
			ptr = generateIdentifier(tokenBuffers, nodeBuffers, state, assignmentExpressionNode.IdentifierOrMemberAccessID, identifierType).Ptr;
		}
		else if (identifierOrMemberAccessNode.Kind == NodeKind::ARRAY_ACCESS_EXPRESSION)
		{
			ptr = generateArrayAccessExpression(tokenBuffers, nodeBuffers, state, assignmentExpressionNode.IdentifierOrMemberAccessID, expectedType).Ptr;
		}

		if (!ptr)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Error evaluating identifier!");
			return nullptr;
		}

		llvm::Value* expressionVal = generateExpression(tokenBuffers, nodeBuffers, state, assignmentExpressionNode.ValueID, expectedType);

		if (!expressionVal)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Error evaluating expression!");
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

	llvm::Value* generateExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID,
														const TypeSubtypePair& expectedType)
	{
		const auto& expressionNode = nodeBuffers.GetNode(nodeID);

		switch (expressionNode.Kind)
		{
		case NodeKind::BINARY_EXPRESSION:
			return generateBinaryExpression(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::UNARY_EXPRESSION:
			return generateUnaryExpression(tokenBuffers, nodeBuffers, state, nodeID, expectedType);

		case NodeKind::IDENTIFIER:
		case NodeKind::LITERAL:
		case NodeKind::CONSTRUCTOR_EXPRESSION:
		case NodeKind::POINTER_INITIALIZER_EXPRESSION:
		case NodeKind::POINTER_MOVE_EXPRESSION:
		case NodeKind::INITIALIZER_LIST_EXPRESSION:
		case NodeKind::VALUE_DEFINITION_EXPRESSION:
		case NodeKind::ARRAY_ACCESS_EXPRESSION:
		case NodeKind::MEMBER_ACCESS_EXPRESSION:
		case NodeKind::FUNCTION_CALL_EXPRESSION:
		case NodeKind::ENCLOSED_EXPRESSION:
			return generatePrimaryExpression(tokenBuffers, nodeBuffers, state, nodeID, expectedType);

		case NodeKind::ASSIGNMENT_EXPRESSION:
			return generateAssignmentExpression(tokenBuffers, nodeBuffers, state, nodeID, expectedType);

		default:
			ASSERT(false, "Unknown expression node kind!");
			return nullptr;

		}
	}

#pragma endregion

	// forward declarations
	llvm::Value* generateValueDefinition(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID);
	llvm::Value* generateStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID);
	llvm::Value* generateBlockStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID);

#pragma region Statements

	llvm::Value* generateValueDefinitionStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const VALUE_DEFINITION_STATEMENT& valueDefinitionStatementNode = nodeBuffers.GetNode(nodeID).ValueDefinitionStatement;

		return generateValueDefinitionExpression(tokenBuffers, nodeBuffers, state, valueDefinitionStatementNode.ValueDefinitionExpressionID);
	}

	llvm::Value* generateFunctionCallStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const FUNCTION_CALL_STATEMENT& functionCallStatementNode = nodeBuffers.GetNode(nodeID).FunctionCallStatement;

		return generateFunctionCallExpression(tokenBuffers, nodeBuffers, state, functionCallStatementNode.FunctionCallExpressionID);
	}

	llvm::Value* generateAssignmentStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const ASSIGNMENT_STATEMENT& assignmentStatementNode = nodeBuffers.GetNode(nodeID).AssignmentStatement;
		TypeSubtypePair identifierType = { nullptr, nullptr };
		return generateAssignmentExpression(tokenBuffers, nodeBuffers, state, assignmentStatementNode.AssignmentExpressionID, identifierType);
	}

	llvm::Value* generateForLoopStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const FOR_LOOP_STATEMENT& forLoopStatementNode = nodeBuffers.GetNode(nodeID).ForLoopStatement;

		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();

		ASSERT(func != nullptr, "No function to insert into!");

		// emit init code before the loop
		if (forLoopStatementNode.InitExpressionID != ERROR_NODE_ID)
		{
			llvm::Value* initVal = generateExpression(tokenBuffers, nodeBuffers, state, forLoopStatementNode.InitExpressionID, { nullptr, nullptr });

			if (initVal == nullptr)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(forLoopStatementNode.InitExpressionID), "Error evaluating expression!");
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
		llvm::Value* bodyVal = generateBlockStatement(tokenBuffers, nodeBuffers, state, forLoopStatementNode.BodyID);

		if (bodyVal == nullptr)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(forLoopStatementNode.BodyID), "Error evaluating expression!");
			return nullptr;
		}

		// emit step value
		if (forLoopStatementNode.IncrementExpressionID != ERROR_NODE_ID)
		{
			llvm::Value* stepVal = generateExpression(tokenBuffers, nodeBuffers, state, forLoopStatementNode.IncrementExpressionID, { nullptr, nullptr });

			if (stepVal == nullptr)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(forLoopStatementNode.IncrementExpressionID), "Error evaluating expression!");
				return nullptr;
			}
		}

		// emit the condition
		llvm::Value* conditionVal = generateExpression(tokenBuffers, nodeBuffers, state, forLoopStatementNode.ConditionExpressionID, { nullptr, nullptr });

		if (conditionVal == nullptr)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(forLoopStatementNode.ConditionExpressionID), "Error evaluating expression!");
			return nullptr;
		}

		conditionVal = convertToBool(state, conditionVal);

		// create the "after loop" block and insert it
		llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(*state.Context, "afterloop", func);

		// insert the conditional branch into the end of afterBlock
		state.Builder->CreateCondBr(conditionVal, loopBlock, afterBlock);

		// any new code will be inserted in afterBlock
		state.Builder->SetInsertPoint(afterBlock);

		// for expr always returns 0.0
		return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*state.Context));
	}

	llvm::Value* generateWhileLoopStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const WHILE_LOOP_STATEMENT& whileLoopStatement = nodeBuffers.GetNode(nodeID).WhileLoopStatement;

		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();

		ASSERT(func != nullptr, "No function to insert into!");

		// create a new basic block to start insertion into
		llvm::BasicBlock* loopBlock = llvm::BasicBlock::Create(*state.Context, "loop", func);

		// insert an explicit fall through from the current block to the loopBlock
		state.Builder->CreateBr(loopBlock);

		// start insertion into the loopBlock
		state.Builder->SetInsertPoint(loopBlock);

		// generate the body of the loop
		llvm::Value* bodyVal = generateBlockStatement(tokenBuffers, nodeBuffers, state, whileLoopStatement.BodyID);

		if (bodyVal == nullptr)
		{
			return nullptr;
		}

		// emit the condition
		llvm::Value* conditionVal = generateExpression(tokenBuffers, nodeBuffers, state, whileLoopStatement.ConditionExpressionID, { nullptr, nullptr });

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

	llvm::Value* generateIfStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const IF_STATEMENT& ifStatementNode = nodeBuffers.GetNode(nodeID).IfStatement;

		llvm::Value* conditionVal = generateExpression(tokenBuffers, nodeBuffers, state, ifStatementNode.ConditionExpressionID, { nullptr, nullptr });

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

		llvm::Value* thenVal = generateStatement(tokenBuffers, nodeBuffers, state, ifStatementNode.BodyID);

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

		if (ifStatementNode.ElseID != ERROR_NODE_ID)
		{
			elseVal = generateStatement(tokenBuffers, nodeBuffers, state, ifStatementNode.ElseID);

			if (elseVal == nullptr)
			{
				return nullptr;
			}
		}

		// insert the branch into the end of the if block
		// insert the branch into the end of the if block
		branchIfNotDuplicate(state, mergeBlock);

		// emit merge block
		func->insert(func->end(), mergeBlock);
		state.Builder->SetInsertPoint(mergeBlock);

		return llvm::ConstantInt::getTrue(*state.Context);
	}

	llvm::Value* generateBlockStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const BLOCK_STATEMENT& blockStatementNode = nodeBuffers.GetNode(nodeID).BlockStatement;

		llvm::Value* statementVal = nullptr;

		for (const NodeID statementID : blockStatementNode.StatementIDs)
		{
			statementVal = generateStatement(tokenBuffers, nodeBuffers, state, statementID);

			if (statementVal == nullptr)
			{
				return nullptr;
			}
		}

		return statementVal;
	}

	llvm::Value* generateReturnStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::RETURN_STATEMENT, "Expected RETURN_STATEMENT!");

		const RETURN_STATEMENT& returnStatementNode = nodeBuffers.GetNode(nodeID).ReturnStatement;

		// handle void return
		if (returnStatementNode.ExpressionID == ERROR_NODE_ID)
		{
			if (state.CurrentReturnValue != nullptr)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID),
					"Function expects a return value of type '{0}'!", state.NamedValues.GetTypeName(state.CurrentReturnValue->getType()));

				return nullptr;
			}

			return state.Builder->CreateBr(state.FuncExitBlock);
		}

		llvm::Value* expressionValue = generateExpression(tokenBuffers, nodeBuffers, state, returnStatementNode.ExpressionID, { nullptr, nullptr });

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
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID),
				"Function has return type '{0}' but provided return value is of type '{1}'!",
				state.CurrentReturnValue == nullptr ? "void" : state.NamedValues.GetTypeName(state.CurrentReturnValue->getType()),
				state.NamedValues.GetTypeName(expressionValue->getType()));
			return nullptr;
		}

		// store the return value and branch to the exit block
		state.Builder->CreateStore(expressionValue, state.CurrentReturnValue);
		return state.Builder->CreateBr(state.FuncExitBlock);
	}

	llvm::Value* generateStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const Node& node = nodeBuffers.GetNode(nodeID);

		switch (node.Kind)
		{
		case NodeKind::VALUE_DEFINITION_STATEMENT:
			return generateValueDefinitionStatement(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::FUNCTION_CALL_STATEMENT:
			return generateFunctionCallStatement(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::ASSIGNMENT_STATEMENT:
			return generateAssignmentStatement(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::FOR_LOOP_STATEMENT:
			return generateForLoopStatement(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::WHILE_LOOP_STATEMENT:
			return generateWhileLoopStatement(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::IF_STATEMENT:
			return generateIfStatement(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::BLOCK_STATEMENT:
			return generateBlockStatement(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::RETURN_STATEMENT:
			return generateReturnStatement(tokenBuffers, nodeBuffers, state, nodeID);

		default:
			ASSERT(false, "Unknown statement node kind!");
			return nullptr;
		}
	}

#pragma endregion

#pragma region Definitions

	llvm::Value* generateExternDefinition(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const EXTERN_DEFINITION& externDefinitionNode = nodeBuffers.GetNode(nodeID).ExternDefinition;

		return generateFunctionDeclaration(tokenBuffers, nodeBuffers, state, externDefinitionNode.FunctionDeclarationID);
	}

	llvm::Value* generateValueDefinition(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const VALUE_DEFINITION& valueDefinitionNode = nodeBuffers.GetNode(nodeID).ValueDefinition;

		return generateValueDefinitionExpression(tokenBuffers, nodeBuffers, state, valueDefinitionNode.ValueDefinitionExpressionID);
	}

	llvm::Type* generateStructDefinition(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::STRUCT_DEFINITION, "Expected STRUCT_DEFINITION!");

		const STRUCT_DEFINITION& structDefinitionNode = nodeBuffers.GetNode(nodeID).StructDefinition;
		const IDENTIFIER& identifier = nodeBuffers.GetNode(structDefinitionNode.IdentifierID).Identifier;

		const std::string_view name = tokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView();

		llvm::Value* result = nullptr;

		// get a vector of member types
		std::vector<llvm::Type*> memberTypes;
		std::unordered_map<std::string_view, NamedValues::StructMemberInfo> memberNames;

		int memberIndex = 0;
		for (auto id : structDefinitionNode.MemberIDs)
		{
			const VALUE_DECLARATION& valueDeclaration = nodeBuffers.GetNode(id).ValueDeclaration;
			TYPE_IDENTIFIER::Modifier modifier = TYPE_IDENTIFIER::Modifier::None;
			TypeSubtypePair identifierType = generateTypeIdentifier(tokenBuffers, nodeBuffers, state, valueDeclaration.TypeIdentifierID, modifier);

			if (!identifierType.type)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(valueDeclaration.TypeIdentifierID), "Invalid structure member type for structure '{0}'!", name);
				return nullptr;
			}

			memberTypes.push_back(identifierType.type);

			// retrieve member name and add it to memberNames map
			const IDENTIFIER& memberID = nodeBuffers.GetNode(valueDeclaration.IdentifierID).Identifier;
			const std::string_view memberName = tokenBuffers.GetValue(memberID.IdentifierTokenID).ToStringView();

			memberNames[memberName] = { memberIndex++, identifierType.containedType };
		}

		llvm::StructType* structType = llvm::StructType::create(*state.Context, memberTypes, name);

		if (!structType)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Invalid structure type for structure '{0}'!", name);
			return nullptr;
		}

		state.NamedValues.InsertType(name, structType, /*isStruct*/true, memberNames);

		return structType;
	}

	llvm::Value* generateEnumDefinition(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(false, "To be implemented.");
		return nullptr;
	}

	llvm::Value* generateFunctionDefinition(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::FUNCTION_DEFINITION, "Expected FUNCTION_DEFINITION!")

		const FUNCTION_DEFINITION& functionDefinitionNode = nodeBuffers.GetNode(nodeID).FunctionDefinition;

		llvm::Function* func = static_cast<llvm::Function*>(generateFunctionDeclaration(tokenBuffers, nodeBuffers, state, functionDefinitionNode.FunctionDeclarationID));

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
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Variable '{0}' already defined!", arg.getName().str());

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
			state.NamedValues.InsertValue(arg.getName(), allocaInst, subType, false);
		}

		// every function has an exit block for cleanup and setting the return value
		llvm::BasicBlock* previousExitBlock = state.FuncExitBlock;
		state.FuncExitBlock = llvm::BasicBlock::Create(*state.Context, "exit", func);

		llvm::Value* bodyVal = generateBlockStatement(tokenBuffers, nodeBuffers, state, functionDefinitionNode.BodyID);

		if (!bodyVal)
		{
			func->eraseFromParent();
			state.NamedValues.FreeHeapPointers(*state.Builder);
			state.NamedValues.PopScope();
			state.CurrentReturnValue = previousReturnValue;
			state.FuncExitBlock = previousExitBlock;
			return nullptr;
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

	llvm::Value* generateDefinition(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const Node& node = nodeBuffers.GetNode(nodeID);

		switch (node.Kind)
		{
		case NodeKind::EXTERN_DEFINITION:
			return generateExternDefinition(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::VALUE_DEFINITION:
			return generateValueDefinition(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::STRUCT_DEFINITION:
			return generateStructDefinition(tokenBuffers, nodeBuffers, state, nodeID) ? llvm::ConstantInt::getTrue(*state.Context) : nullptr;

		case NodeKind::ENUM_DEFINITION:
			return generateEnumDefinition(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::FUNCTION_DEFINITION:
			return generateFunctionDefinition(tokenBuffers, nodeBuffers, state, nodeID);

		default:
			ASSERT(false, "Unknown definition node kind!");
			return nullptr;
		}
	}

	llvm::Value* generateQualifiedDefinition(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const QUALIFIED_DEFINITION& qualifiedDefinitionNode = nodeBuffers.GetNode(nodeID).QualifiedDefinition;

		// TODO: where does the qualifier come in?

		return generateDefinition(tokenBuffers, nodeBuffers, state, qualifiedDefinitionNode.DefinitionID);
	}

#pragma endregion

	llvm::Value* generateModule(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const MODULE& moduleNode = nodeBuffers.GetNode(nodeID).Module;

		// generate code for each qualified definition in the module
		for (const NodeID qualifiedDefinitionID : moduleNode.QualifiedDefinitionIDs)
		{
			generateQualifiedDefinition(tokenBuffers, nodeBuffers, state, qualifiedDefinitionID);
		}

		return nullptr;
	}

	llvm::Value* generateProgram(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::PROGRAM, "Expected PROGRAM!");

		const PROGRAM& programNode = nodeBuffers.GetNode(nodeID).Program;

		// generate code for each module in the program
		// TODO: support for multiple modules
		return generateModule(tokenBuffers, nodeBuffers, state, programNode.ModuleIDs[0]);
	}

	bool Generate(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state)
	{
		//const NodeID rootNodeID = nodeBuffers.GetRootNodeID();
		//
		//ASSERT(nodeBuffers.GetNode(rootNodeID).Kind == NodeKind::PROGRAM, "Root node must be a program node!");
		//
		//llvm::Value* program = generateProgram(tokenBuffers, nodeBuffers, state, rootNodeID);
		//
		//std::error_code errorCode;
		//llvm::raw_fd_ostream out("c:\\temp\\out.ll", errorCode);
		//state.Module->print(out, nullptr);

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
