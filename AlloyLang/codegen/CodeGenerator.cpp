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

	llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function, const std::string_view& varName, llvm::Type* type)
	{
		llvm::IRBuilder<> tempBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
		return tempBuilder.CreateAlloca(type, nullptr, varName);
	}

#pragma endregion

	// forward declarations
	llvm::Value* generateExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID);

#pragma region Literals

	llvm::Value* generateLiteral(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::LITERAL, "Expected LITERAL!");

		const LITERAL& literalNode = nodeBuffers.GetNode(nodeID).Literal;

		const std::string_view literalStr = tokenBuffers.GetValue(literalNode.InfoTokenID).ToStringView();

		switch (literalNode.Kind)
		{
		case LITERAL::Type::Integer:
		{
			// TODO: allow negative values
			uint64_t uintValue;
			std::from_chars_result result = std::from_chars(literalStr.data(), literalStr.data() + literalStr.size(), uintValue);
			return llvm::ConstantInt::get(*state.Context, llvm::APInt(64, uintValue));
		}

		case LITERAL::Type::Float:
		{
			double floatValue;
			std::from_chars_result result = std::from_chars(literalStr.data(), literalStr.data() + literalStr.size(), floatValue);
			return llvm::ConstantFP::get(*state.Context, llvm::APFloat(floatValue));
		}

		case LITERAL::Type::Boolean:
		{
			if (literalStr == "true")
			{
				return llvm::ConstantInt::getTrue(*state.Context);
			}

			if (literalStr == "false")
			{
				return llvm::ConstantInt::getFalse(*state.Context);
			}

			ASSERT(false, "Unknown boolean literal '{0}'!", literalStr);
			return nullptr;
		}

		case LITERAL::Type::String:
			return state.Builder->CreateGlobalStringPtr(literalStr);

		case LITERAL::Type::Character:
			return llvm::ConstantInt::get(*state.Context, llvm::APInt(64, literalStr[0]));

		default:
			ASSERT(false, "Unknown literal type!");
			return nullptr;
		}
	}

#pragma endregion

#pragma region Identifiers

	PtrValuePair generateIdentifier(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::IDENTIFIER, "Expected IDENTIFIER!");

		const IDENTIFIER& node = nodeBuffers.GetNode(nodeID).Identifier;
		const std::string_view name = tokenBuffers.GetValue(node.IdentifierTokenID).ToStringView();

		// check if we have a local variable with this name
		llvm::AllocaInst* allocaInst = state.NamedValues.GetValue(name);

		if (allocaInst)
		{
			llvm::Value* value = state.Builder->CreateLoad(allocaInst->getAllocatedType(), allocaInst, name);
			return PtrValuePair{ .Ptr = allocaInst, .Value = value };
		}

		// check if we have a global
		llvm::GlobalVariable* globalVar = state.Module->getGlobalVariable(name);

		if (globalVar)
		{
			llvm::Value* value = globalVar->getInitializer();
			return PtrValuePair{ .Ptr = allocaInst, .Value = value };
		}

		logErrorAtPosition(tokenBuffers, node.IdentifierTokenID, "Unknown variable name '{0}'!", name);
		return {};
	}

	llvm::Type* generateTypeIdentifier(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::TYPE_IDENTIFIER, "Expected TYPE_IDENTIFIER!");

		// TODO: references and pointers

		const TYPE_IDENTIFIER& typeIdentifierNode = nodeBuffers.GetNode(nodeID).TypeIdentifier;

		const Node& identifierOrTypeIdentifierNode = nodeBuffers.GetNode(typeIdentifierNode.IdentifierOrTypeIdentifierID);

		// handle non-array types
		if (identifierOrTypeIdentifierNode.Kind == NodeKind::IDENTIFIER)
		{
			const IDENTIFIER& identifierNode = identifierOrTypeIdentifierNode.Identifier;
			const std::string_view name = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

			llvm::Type* type = state.NamedValues.GetType(name);

			if (!type)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(typeIdentifierNode.IdentifierOrTypeIdentifierID),
					"Unknown type name '{0}'!", name);
			}

			return type;
		}


		// handle array types
		ASSERT(identifierOrTypeIdentifierNode.Kind == NodeKind::TYPE_IDENTIFIER, "Expected type identifier node!");
		ASSERT(typeIdentifierNode.ArraySizeID != ERROR_NODE_ID, "Expected array size node!");

		const LITERAL& arraySizeNode = nodeBuffers.GetNode(typeIdentifierNode.ArraySizeID).Literal;

		const std::string_view arraySizeStr = tokenBuffers.GetValue(arraySizeNode.InfoTokenID).ToStringView();

		uint64_t arraySize;
		std::from_chars(arraySizeStr.data(), arraySizeStr.data() + arraySizeStr.size(), arraySize);

		if (arraySize == 0)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(typeIdentifierNode.ArraySizeID), "Array size must be greater than 0!");
			return nullptr;
		}

		const NodeID elementTypeIdentifierNode = typeIdentifierNode.IdentifierOrTypeIdentifierID;

		llvm::Type* elementType = generateTypeIdentifier(tokenBuffers, nodeBuffers, state, elementTypeIdentifierNode);

		if (!elementType)
		{
			return nullptr;
		}

		return llvm::ArrayType::get(elementType, arraySize);
	}

#pragma endregion

#pragma region Declarations

	llvm::Type* generateTypeDeclaration(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::TYPE_DECLARATION, "Expected TYPE_DECLARATION!");

		// TODO: var and const

		const TYPE_DECLARATION& typeDeclarationNode = nodeBuffers.GetNode(nodeID).TypeDeclaration;

		return generateTypeIdentifier(tokenBuffers, nodeBuffers, state, typeDeclarationNode.TypeIdentifierID);
	}

	llvm::Value* generateValueDeclaration(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::VALUE_DECLARATION, "Expected VALUE_DECLARATION!");

		const VALUE_DECLARATION& valueDeclarationNode = nodeBuffers.GetNode(nodeID).ValueDeclaration;
		const IDENTIFIER& identifierNode = nodeBuffers.GetNode(valueDeclarationNode.IdentifierID).Identifier;

		// handle globals
		if (state.Builder->GetInsertBlock() == nullptr)
		{
			ASSERT(false, "Not yet implemented!");
			return nullptr;
		}

		// handle locals
		else
		{
			const std::string_view name = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

			llvm::Type* type = generateTypeIdentifier(tokenBuffers, nodeBuffers, state, valueDeclarationNode.TypeIdentifierID);

			if (!type)
			{
				return nullptr;
			}

			// create the alloca
			llvm::AllocaInst* allocaInst = createEntryBlockAlloca(
				state.Builder->GetInsertBlock()->getParent(),
				name,
				type
			);

			// add the variable to the named values
			state.NamedValues.InsertValue(name, allocaInst);

			return allocaInst;
		}
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

		for (NodeID parameterID : functionDeclarationNode.ParameterIDs)
		{
			const VALUE_DECLARATION& valueDeclaration = nodeBuffers.GetNode(parameterID).ValueDeclaration;

			llvm::Type* type = generateTypeIdentifier(tokenBuffers, nodeBuffers, state, valueDeclaration.TypeIdentifierID);

			if (!type)
			{
				return nullptr;
			}

			paramTypes.push_back(type);
		}

		// retrieve the return type
		llvm::Type* returnType = llvm::Type::getVoidTy(*state.Context);

		if (functionDeclarationNode.ReturnTypeID != ERROR_NODE_ID)
		{
			const TYPE_DECLARATION& typeDeclaration = nodeBuffers.GetNode(functionDeclarationNode.ReturnTypeID).TypeDeclaration;
			returnType = generateTypeDeclaration(tokenBuffers, nodeBuffers, state, functionDeclarationNode.ReturnTypeID);
		}

		if (!returnType)
		{
			return nullptr;
		}

		llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, functionDeclarationNode.IsVariadic);

		llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, name, state.Module.get());

		// set names for all arguments
		for (size_t i = 0; i < function->arg_size(); i++)
		{
			const VALUE_DECLARATION& valueDeclaration = nodeBuffers.GetNode(functionDeclarationNode.ParameterIDs[i]).ValueDeclaration;
			const IDENTIFIER& identifier = nodeBuffers.GetNode(valueDeclaration.IdentifierID).Identifier;

			const std::string_view paramName = tokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView();

			function->getArg(i)->setName(paramName);
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
			// get the name of the member to initiliaze
			const IDENTIFIER& memberIdentifier = nodeBuffers.GetNode(constructorExpressionNode.MemberIdentifierIDs[i]).Identifier;
			const std::string_view memberName = tokenBuffers.GetValue(memberIdentifier.IdentifierTokenID).ToStringView();

			int memberIndex = state.NamedValues.GetMemberIndex(structName, memberName);

			if (memberIndex == -1)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(constructorExpressionNode.MemberIdentifierIDs[i]), "Type '{0}' is not a struct!", structName);
				return nullptr;
			}

			if (memberIndex == -2)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(constructorExpressionNode.MemberIdentifierIDs[i]), "Struct '{0}' does not have a member '{1}'!", structName, memberName);
				return nullptr;
			}

			llvm::Value* expressionVal = generateExpression(tokenBuffers, nodeBuffers, state, constructorExpressionNode.MemberValueIDs[i]);

			if (!expressionVal)
			{
				return nullptr;
			}

			// convert our index to an llvm 32-bit Int
			llvm::Value* llvmIndex = llvm::ConstantInt::get(*state.Context, llvm::APInt(32, memberIndex, true));

			// struct members are accessed through 2 indices, this is described in detail here:
			// https://llvm.org/docs/GetElementPtr.html
			std::vector<llvm::Value*> indices(2);
			indices[0] = llvm::ConstantInt::get(*state.Context, llvm::APInt(32, 0, true));
			indices[1] = llvmIndex;

			llvm::Value* memberPtr = state.Builder->CreateGEP(structType, structPtr, indices, "memberptr");
			state.Builder->CreateStore(expressionVal, memberPtr, "savetmp");
		}

		return state.Builder->CreateLoad(structPtr->getAllocatedType(), structPtr);	// result contains the whole initialized structure
	}

	llvm::Value* generatePointerInitializerExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(false, "Not yet implemented!");
		return nullptr;
	}

	llvm::Value* generateInitializerListExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(false, "Not yet implemented!");
		return nullptr;
	}

	llvm::Value* generateValueDefinitionExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::VALUE_DEFINITION_EXPRESSION, "Expected VALUE_DEFINITION_EXPRESSION!");

		const VALUE_DEFINITION_EXPRESSION& valueDefinitionExpressionNode = nodeBuffers.GetNode(nodeID).ValueDefinitionExpression;

		// create the declaration
		llvm::Value* declarationVal = generateValueDeclaration(tokenBuffers, nodeBuffers, state, valueDefinitionExpressionNode.ValueDeclarationID);

		if (!declarationVal)
		{
			return nullptr;
		}

		// create the value expression
		llvm::Value* value = generateExpression(tokenBuffers, nodeBuffers, state, valueDefinitionExpressionNode.ValueID);

		if (!value)
		{
			return nullptr;
		}

		// store the value into the alloca
		return state.Builder->CreateStore(value, declarationVal);
	}

	llvm::Value* generateArrayAccessExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(false, "Not yet implemented!");
		return nullptr;
	}

	PtrValuePair generateMemberAccessExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(nodeBuffers.GetNode(nodeID).Kind == NodeKind::MEMBER_ACCESS_EXPRESSION, "Expected MEMBER_ACCESS_EXPRESSION!");

		const MEMBER_ACCESS_EXPRESSION& memberAccessExpressionNode = nodeBuffers.GetNode(nodeID).MemberAccessExpression;

		const IDENTIFIER& right = nodeBuffers.GetNode(memberAccessExpressionNode.RightID).Identifier;
		const std::string_view memberName = tokenBuffers.GetValue(right.IdentifierTokenID).ToStringView();

		PtrValuePair left;

		// handle nested member access
		if (nodeBuffers.GetNode(memberAccessExpressionNode.LeftID).Kind == NodeKind::MEMBER_ACCESS_EXPRESSION)
		{
			left = generateMemberAccessExpression(tokenBuffers, nodeBuffers, state, memberAccessExpressionNode.LeftID);
		}

		// handle identifier
		else if (nodeBuffers.GetNode(memberAccessExpressionNode.LeftID).Kind == NodeKind::IDENTIFIER)
		{
			left = generateIdentifier(tokenBuffers, nodeBuffers, state, memberAccessExpressionNode.LeftID);
		}

		if (!left.Ptr)
		{
			return {};
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
		int memberIndex = state.NamedValues.GetMemberIndex(structName, memberName);

		if (memberIndex == -1)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(memberAccessExpressionNode.RightID), "Type '{0}' is not a struct type!", structName);
			return {};
		}

		if (memberIndex == -2)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(memberAccessExpressionNode.RightID), "Struct '{0}' does not have a member '{1}'!", structName, memberName);
			return {};
		}

		// convert our index to an llvm 32-bit Int
		llvm::Value* llvmIndex = llvm::ConstantInt::get(*state.Context, llvm::APInt(32, memberIndex, true));

		// struct members are accessed through 2 indices, this is described in detail here:
		// https://llvm.org/docs/GetElementPtr.html
		std::vector<llvm::Value*> indices(2);
		indices[0] = llvm::ConstantInt::get(*state.Context, llvm::APInt(32, 0, true));
		indices[1] = llvmIndex;

		llvm::Value* memberPtr = state.Builder->CreateGEP(structType, left.Ptr, indices, "memberptr");
		llvm::Value* memberValue = state.Builder->CreateLoad(structType->getTypeAtIndex(memberIndex), memberPtr, "loadtmp");

		return PtrValuePair{ .Ptr = memberPtr, .Value = memberValue };
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
			return nullptr;
		}

		// if the function was found, check for argument mismatch
		if (calleeFunc->arg_size() != functionCallExpressionNode.ArgumentIDs.size())
		{
			return nullptr;
		}

		// evaluate all the arguments
		std::vector<llvm::Value*> args;

		for (size_t i = 0; i < functionCallExpressionNode.ArgumentIDs.size(); i++)
		{
			const NodeID argumentID = functionCallExpressionNode.ArgumentIDs[i];

			llvm::Value* argVal = generateExpression(tokenBuffers, nodeBuffers, state, argumentID);

			if (argVal == nullptr)
			{
				return nullptr;
			}

			if (argVal->getType() != calleeFunc->getArg(i)->getType())
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(argumentID), 
					"Function argument {0} expects value of type '{1}' but given type is '{2}'!", i + 1,
					state.NamedValues.GetTypeName(calleeFunc->getArg(i)->getType()),
					state.NamedValues.GetTypeName(argVal->getType()));
				return nullptr;
			}

			args.push_back(argVal);
		}

		return state.Builder->CreateCall(calleeFunc, args, "calltmp");
	}

	llvm::Value* generateEnclosedExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const ENCLOSED_EXPRESSION& enclosedExpressionNode = nodeBuffers.GetNode(nodeID).EnclosedExpression;

		return generateExpression(tokenBuffers, nodeBuffers, state, enclosedExpressionNode.ExpressionID);
	}

	llvm::Value* generateBinaryExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const BINARY_EXPRESSION& binaryExpressionNode = nodeBuffers.GetNode(nodeID).BinaryExpression;

		llvm::Value* leftVal = generateExpression(tokenBuffers, nodeBuffers, state, binaryExpressionNode.LeftID);
		llvm::Value* rightVal = generateExpression(tokenBuffers, nodeBuffers, state, binaryExpressionNode.RightID);

		if (!leftVal || !rightVal)
		{
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

	llvm::Value* generateUnaryExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const UNARY_EXPRESSION& unaryExpressionNode = nodeBuffers.GetNode(nodeID).UnaryExpression;

		std::string_view operatorStr = tokenBuffers.GetValue(unaryExpressionNode.OperatorTokenID).ToStringView();

		llvm::Value* expressionVal = generateExpression(tokenBuffers, nodeBuffers, state, unaryExpressionNode.OperandID);

		if (!expressionVal)
		{
			return nullptr;
		}

		if (operatorStr == "-")
		{
			return state.Builder->CreateFNeg(expressionVal, "negtmp");
		}

		if (operatorStr == "!")
		{
			return state.Builder->CreateNot(expressionVal, "nottmp");
		}

		ASSERT(false, "Unknown unary operator!");

		return nullptr;
	}

	llvm::Value* generatePrimaryExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const Node& node = nodeBuffers.GetNode(nodeID);

		switch (node.Kind)
		{
		case NodeKind::IDENTIFIER:
			return generateIdentifier(tokenBuffers, nodeBuffers, state, nodeID).Value;

		case NodeKind::LITERAL:
			return generateLiteral(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::CONSTRUCTOR_EXPRESSION:
			return generateConstructorExpression(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::POINTER_INITIALIZER_EXPRESSION:
			return generatePointerInitializerExpression(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::INITIALIZER_LIST_EXPRESSION:
			return generateInitializerListExpression(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::VALUE_DEFINITION_EXPRESSION:
			return generateValueDefinitionExpression(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::ARRAY_ACCESS_EXPRESSION:
			return generateArrayAccessExpression(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::MEMBER_ACCESS_EXPRESSION:
			return generateMemberAccessExpression(tokenBuffers, nodeBuffers, state, nodeID).Value;

		case NodeKind::FUNCTION_CALL_EXPRESSION:
			return generateFunctionCallExpression(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::ENCLOSED_EXPRESSION:
			return generateEnclosedExpression(tokenBuffers, nodeBuffers, state, nodeID);

		default:
			ASSERT(false, "Unknown primary expression node kind!");
			return nullptr;
		}
	}

	llvm::Value* generateAssignmentExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const ASSIGNMENT_EXPRESSION& assignmentExpressionNode = nodeBuffers.GetNode(nodeID).AssignmentExpression;

		const Node& identifierOrMemberAccessNode = nodeBuffers.GetNode(assignmentExpressionNode.IdentifierOrMemberAccessID);

		llvm::Value* ptr = nullptr;

		if (identifierOrMemberAccessNode.Kind == NodeKind::MEMBER_ACCESS_EXPRESSION)
		{
			ptr = generateMemberAccessExpression(tokenBuffers, nodeBuffers, state, assignmentExpressionNode.IdentifierOrMemberAccessID).Ptr;
		}

		else if (identifierOrMemberAccessNode.Kind == NodeKind::IDENTIFIER)
		{
			ptr = generateIdentifier(tokenBuffers, nodeBuffers, state, assignmentExpressionNode.IdentifierOrMemberAccessID).Ptr;
		}

		if (!ptr)
		{
			return nullptr;
		}

		llvm::Value* expressionVal = generateExpression(tokenBuffers, nodeBuffers, state, assignmentExpressionNode.ValueID);

		if (!expressionVal)
		{
			return nullptr;
		}

		// store the value into the alloca
		return state.Builder->CreateStore(expressionVal, ptr);
	}

	llvm::Value* generateExpression(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const auto& expressionNode = nodeBuffers.GetNode(nodeID);

		switch (expressionNode.Kind)
		{
		case NodeKind::BINARY_EXPRESSION:
			return generateBinaryExpression(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::UNARY_EXPRESSION:
			return generateUnaryExpression(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::IDENTIFIER:
		case NodeKind::LITERAL:
		case NodeKind::CONSTRUCTOR_EXPRESSION:
		case NodeKind::POINTER_INITIALIZER_EXPRESSION:
		case NodeKind::INITIALIZER_LIST_EXPRESSION:
		case NodeKind::VALUE_DEFINITION_EXPRESSION:
		case NodeKind::ARRAY_ACCESS_EXPRESSION:
		case NodeKind::MEMBER_ACCESS_EXPRESSION:
		case NodeKind::FUNCTION_CALL_EXPRESSION:
		case NodeKind::ENCLOSED_EXPRESSION:
			return generatePrimaryExpression(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::ASSIGNMENT_EXPRESSION:
			return generateAssignmentExpression(tokenBuffers, nodeBuffers, state, nodeID);

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

		return generateAssignmentExpression(tokenBuffers, nodeBuffers, state, assignmentStatementNode.AssignmentExpressionID);
	}

	llvm::Value* generateForLoopStatement(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const FOR_LOOP_STATEMENT& forLoopStatementNode = nodeBuffers.GetNode(nodeID).ForLoopStatement;

		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();

		ASSERT(func != nullptr, "No function to insert into!");

		// emit init code before the loop
		if (forLoopStatementNode.InitExpressionID != ERROR_NODE_ID)
		{
			llvm::Value* initVal = generateExpression(tokenBuffers, nodeBuffers, state, forLoopStatementNode.InitExpressionID);

			if (initVal == nullptr)
			{
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
			return nullptr;
		}

		// emit step value
		if (forLoopStatementNode.IncrementExpressionID != ERROR_NODE_ID)
		{
			llvm::Value* stepVal = generateExpression(tokenBuffers, nodeBuffers, state, forLoopStatementNode.IncrementExpressionID);

			if (stepVal == nullptr)
			{
				return nullptr;
			}
		}

		// emit the condition
		llvm::Value* conditionVal = generateExpression(tokenBuffers, nodeBuffers, state, forLoopStatementNode.ConditionExpressionID);

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
		llvm::Value* conditionVal = generateExpression(tokenBuffers, nodeBuffers, state, whileLoopStatement.ConditionExpressionID);

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

		llvm::Value* conditionVal = generateExpression(tokenBuffers, nodeBuffers, state, ifStatementNode.ConditionExpressionID);

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

		state.Builder->CreateBr(mergeBlock);

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

		// insert the conditional branch into the end of the else block
		state.Builder->CreateBr(mergeBlock);

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
			if (state.CurrentReturnType != nullptr)
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID),
					"Function expects a return value of type '{0}'!", state.NamedValues.GetTypeName(state.CurrentReturnType));

				return nullptr;
			}

			return state.Builder->CreateRetVoid();
		}

		llvm::Value* expressionValue = generateExpression(tokenBuffers, nodeBuffers, state, returnStatementNode.ExpressionID);

		if (expressionValue == nullptr)
		{
			return nullptr;
		}

		if (expressionValue->getType() != state.CurrentReturnType)
		{
			logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), 
				"Function has return type '{0}' but provided return value is of type '{1}'!", 
				state.NamedValues.GetTypeName(state.CurrentReturnType),
				state.NamedValues.GetTypeName(expressionValue->getType()));
			return nullptr;
		}

		return state.Builder->CreateRet(expressionValue);
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
		std::unordered_map<std::string_view, int> memberNames;

		int memberIndex = 0;
		for (auto id : structDefinitionNode.MemberIDs)
		{
			const VALUE_DECLARATION& valueDeclaration = nodeBuffers.GetNode(id).ValueDeclaration;

			llvm::Type* type = generateTypeIdentifier(tokenBuffers, nodeBuffers, state, valueDeclaration.TypeIdentifierID);

			if (!type)
			{
				return nullptr;
			}

			memberTypes.push_back(type);

			// retrieve member name and add it to memberNames map
			const IDENTIFIER& memberID = nodeBuffers.GetNode(valueDeclaration.IdentifierID).Identifier;
			const std::string_view memberName = tokenBuffers.GetValue(memberID.IdentifierTokenID).ToStringView();

			memberNames[memberName] = memberIndex++;
		}

		llvm::StructType* structType = llvm::StructType::create(*state.Context, memberTypes, name);

		if (!structType)
		{
			// TODO: log error
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

		state.CurrentReturnType = func->getReturnType();

		// push a new scope for the function
		state.NamedValues.PushScope(func->getName().str());

		// create a new basic block to start insertion into
		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", func);
		state.Builder->SetInsertPoint(entryBlock);

		// create allocations for function arguments
		for (auto& arg : func->args())
		{
			// check that we do not have a named value with the same name
			if (state.NamedValues.GetValue(arg.getName().str()))
			{
				logErrorAtPosition(tokenBuffers, nodeBuffers.GetErrorInfo(nodeID), "Variable '{0}' already defined!", arg.getName().str());

				func->eraseFromParent();
				state.NamedValues.PopScope();
				state.CurrentReturnType = nullptr;
				return nullptr;
			}

			// create an alloca for this variable
			llvm::AllocaInst* allocaInst = createEntryBlockAlloca(func, arg.getName().str(), arg.getType());

			// store the initial value into the alloca
			state.Builder->CreateStore(&arg, allocaInst);

			// add arguments to named values
			state.NamedValues.InsertValue(arg.getName(), allocaInst);
		}

		llvm::Value* bodyVal = generateBlockStatement(tokenBuffers, nodeBuffers, state, functionDefinitionNode.BodyID);

		if (!bodyVal)
		{
			func->eraseFromParent();
			state.NamedValues.PopScope();
			state.CurrentReturnType = nullptr;
			return nullptr;
		}

		// validate the generated code, checking for consistency
		llvm::verifyFunction(*func);

		if (state.Optimizations)
		{
			// run the optimizer on the function.
			state.FPM->run(*func, *state.FAM);
		}

		// restore the named values of the higher level
		state.NamedValues.PopScope();

		state.CurrentReturnType = nullptr;

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

	LLVMState Generate(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, bool optimize)
	{
		const NodeID rootNodeID = nodeBuffers.GetRootNodeID();

		ASSERT(nodeBuffers.GetNode(rootNodeID).Kind == NodeKind::PROGRAM, "Root node must be a program node!");

		LLVMState state(optimize);

		llvm::Value* program = generateProgram(tokenBuffers, nodeBuffers, state, rootNodeID);

		std::error_code errorCode;
		llvm::raw_fd_ostream out("c:\\temp\\out.ll", errorCode);
		state.Module->print(out, nullptr);

		return std::move(state);
	}

	int Execute(LLVMState& state)
	{
#ifndef NO_CODE_EXECUTION
		llvm::InitializeNativeTarget();
		LLVMInitializeNativeAsmPrinter();

		// using the recommended LLJIT engine to execute the code
		llvm::ExitOnError exitOnErr;

		// create an LLJIT instance
		auto J = exitOnErr(llvm::orc::LLJITBuilder().create());
		exitOnErr(J->addIRModule(llvm::orc::ThreadSafeModule(std::move(state.Module), std::move(state.Context))));

		// look up the JIT'd function, cast it to a function pointer, then call it.
		auto mainAddr = exitOnErr(J->lookup("main"));
		int (*main)() = mainAddr.toPtr<int()>();
		return main();
#else
		ASSERT(false, "Code execution is disabled!");
		return 0;
#endif
	}
}
