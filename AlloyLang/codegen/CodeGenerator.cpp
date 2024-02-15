#include "CodeGenerator.hpp"

#include "llvm/llvm.hpp"
#include "NamedValues.hpp"

namespace AlloyCompiler
{
	struct LLVMState
	{
		std::unique_ptr<llvm::LLVMContext> Context;
		std::unique_ptr<llvm::IRBuilder<>> Builder;
		std::unique_ptr<llvm::Module> Module;
		NamedValues NamedValues;

		// handling llvm copde optimizations passes
		std::unique_ptr<llvm::FunctionPassManager> FPM;
		std::unique_ptr<llvm::LoopAnalysisManager> LAM;
		std::unique_ptr<llvm::FunctionAnalysisManager> FAM;
		std::unique_ptr<llvm::CGSCCAnalysisManager> CGAM;
		std::unique_ptr<llvm::ModuleAnalysisManager> MAM;
		std::unique_ptr<llvm::PassInstrumentationCallbacks> PIC;
		std::unique_ptr<llvm::StandardInstrumentations> SI;

		bool Optimizations;

		LLVMState(bool optimizations)
			: Optimizations(optimizations)
		{
			Context = std::make_unique<llvm::LLVMContext>();
			Builder = std::make_unique<llvm::IRBuilder<>>(*Context);
			Module = std::make_unique<llvm::Module>("AlloyModule", *Context);


			if (optimizations)
			{
				// check the LLVM tutorial for details about these optimizations
				// create new pass and analysis managers
				FPM = std::make_unique<llvm::FunctionPassManager>();
				LAM = std::make_unique<llvm::LoopAnalysisManager>();
				FAM = std::make_unique<llvm::FunctionAnalysisManager>();
				CGAM = std::make_unique<llvm::CGSCCAnalysisManager>();
				MAM = std::make_unique<llvm::ModuleAnalysisManager>();
				PIC = std::make_unique<llvm::PassInstrumentationCallbacks>();
				SI = std::make_unique<llvm::StandardInstrumentations>(*Context, /*DebugLogging*/ true);

				SI->registerCallbacks(*PIC, MAM.get());

				// add transform passes

				// do simple "peephole" optimizations and bit-twiddling optzns
				FPM->addPass(llvm::InstCombinePass());

				// reassociate expressions
				FPM->addPass(llvm::ReassociatePass());

				// eliminate Common SubExpressions
				FPM->addPass(llvm::GVNPass());

				// simplify the control flow graph (deleting unreachable blocks, etc)
				FPM->addPass(llvm::SimplifyCFGPass());

				// register analysis passes used in these transform passes
				llvm::PassBuilder PB;
				PB.registerModuleAnalyses(*MAM);
				PB.registerFunctionAnalyses(*FAM);
				PB.crossRegisterProxies(*LAM, *FAM, *CGAM, *MAM);
			}
		}
	};

	/// <summary>
	/// Creates an alloca instruction in the given block.
	/// </summary>
	llvm::AllocaInst* createAlloca(llvm::BasicBlock* block, const std::string_view& varName, llvm::Type* type)
	{
		llvm::IRBuilder<> tempBuilder(block, block->begin());

		return tempBuilder.CreateAlloca(type, nullptr, varName);
	}

	llvm::Type* typeIdentifierToLLVMType(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const TYPE_IDENTIFIER& typeIdentifierNode = nodeBuffers.GetNode(nodeID).TypeIdentifier;

		const Node& identifierOrTypeIdentifierNode = nodeBuffers.GetNode(typeIdentifierNode.TypeIdentifierID);

		// handle array types
		if (typeIdentifierNode.ArraySizeID != ERROR_NODE_ID)
		{
			const LITERAL& arraySizeNode = nodeBuffers.GetNode(typeIdentifierNode.ArraySizeID).Literal;
			const SmallStringView& arraySizeStringView = tokenBuffers.GetValue(arraySizeNode.InfoTokenID);

			uint64_t arraySize;
			std::from_chars(arraySizeStringView.Data(), arraySizeStringView.Data() + arraySizeStringView.Size(), arraySize);

			if (arraySize < 2)
			{
				// TODO: log error
				return nullptr;
			}

			llvm::Type* elementType = nullptr;

			// handle array within array
			if (identifierOrTypeIdentifierNode.Kind == NodeKind::TYPE_IDENTIFIER)
			{
				elementType = typeIdentifierToLLVMType(tokenBuffers, nodeBuffers, state, typeIdentifierNode.TypeIdentifierID);
			}

			// handle other types
			else if (identifierOrTypeIdentifierNode.Kind == NodeKind::IDENTIFIER)
			{
				const IDENTIFIER& identifierNode = identifierOrTypeIdentifierNode.Identifier;
				const std::string_view typeName = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

				elementType = state.NamedValues.GetType(typeName);
			}

			else
			{
				ASSERT(false, "Invalid TYPE_IDENTIFIER node! .TypeIdentifierID should be ID of TYPE_IDENTIFIER or IDENTIFIER.");
				return nullptr;
			}

			llvm::ArrayType* arrayType = llvm::ArrayType::get(elementType, arraySize);

			return arrayType;
		}

		// handle non-array types
		else
		{
			ASSERT(identifierOrTypeIdentifierNode.Kind == NodeKind::IDENTIFIER, "Invalid TYPE_IDENTIFIER node! .TypeIdentifierID should be ID of IDENTIFIER for non-array types.");

			const IDENTIFIER& identifierNode = identifierOrTypeIdentifierNode.Identifier;
			const std::string_view typeName = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

			llvm::Type* type = state.NamedValues.GetType(typeName);

			if (type == nullptr)
			{
				// TODO: log error
			}

			return type;
		}
	}

	template <typename T>
	llvm::Value* generate(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID) = delete;

	template <>
	llvm::Value* generate<EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID);

	template <>
	llvm::Value* generate<LITERAL>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
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
			return llvm::ConstantDataArray::getString(*state.Context, literalStr);

		case LITERAL::Type::Character:
			return llvm::ConstantInt::get(*state.Context, llvm::APInt(64, literalStr[0]));

		default:
			ASSERT(false, "Unknown literal type!");
			return nullptr;
		}
	}

	template <>
	llvm::Value* generate<IDENTIFIER>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const IDENTIFIER& identifierNode = nodeBuffers.GetNode(nodeID).Identifier;

		const std::string_view name = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

		// look up the name in the function
		llvm::Value* value = state.NamedValues.GetValue(name);

		if (value)
		{
			return value;
		}

		// look up the name in the global module table
		value = state.Module->getGlobalVariable(name);

		if (value)
		{
			return value;
		}

		// TODO: log error

		return nullptr;
	}

	template <>
	llvm::Value* generate<TYPE_IDENTIFIER>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID) = delete;

	template <>
	llvm::Value* generate<TYPE_DECLARATION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(false, "Not yet implemented!");
		return nullptr;
	}

	template <>
	llvm::Value* generate<VALUE_DECLARATION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
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

			// create the alloca
			llvm::AllocaInst* allocaInst =
				createAlloca(
					state.Builder->GetInsertBlock(),
					name,
					state.Builder->getDoubleTy()
				);

			// add the variable to the named values
			state.NamedValues.InsertValue(name, allocaInst);

			return allocaInst;
		}
	}

	template <>
	llvm::Value* generate<FUNCTION_DECLARATION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const FUNCTION_DECLARATION& functionDeclarationNode = nodeBuffers.GetNode(nodeID).FunctionDeclaration;
		const IDENTIFIER& identifierNode = nodeBuffers.GetNode(functionDeclarationNode.IdentifierID).Identifier;

		const std::string_view functionName = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

		ASSERT(false, "Not yet implemented!");
		return nullptr;
	}

	template <>
	llvm::Value* generate<POINTER_INITIALIZER_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(false, "Not yet implemented!");
		return nullptr;
	}

	template <>
	llvm::Value* generate<INITIALIZER_LIST_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(false, "Not yet implemented!");
		return nullptr;
	}

	template <>
	llvm::Value* generate<VALUE_DEFINITION_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const VALUE_DEFINITION_EXPRESSION& valueDefinitionExpressionNode = nodeBuffers.GetNode(nodeID).ValueDefinitionExpression;

		// create the declaration
		llvm::Value* declarationVal = generate<VALUE_DECLARATION>(tokenBuffers, nodeBuffers, state, valueDefinitionExpressionNode.ValueDeclarationID);

		if (!declarationVal)
		{
			return nullptr;
		}

		// create the value expression
		llvm::Value* value = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, valueDefinitionExpressionNode.ValueID);

		if (!value)
		{
			return nullptr;
		}

		// store the value into the alloca
		return state.Builder->CreateStore(value, declarationVal);
	}

	template <>
	llvm::Value* generate<ARRAY_ACCESS_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(false, "Not yet implemented!");
		return nullptr;
	}

	template <>
	llvm::Value* generate<FUNCTION_CALL_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
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

		for (const NodeID argumentID : functionCallExpressionNode.ArgumentIDs)
		{
			llvm::Value* argVal = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, argumentID);

			if (argVal == nullptr)
			{
				return nullptr;
			}

			args.push_back(argVal);
		}

		return state.Builder->CreateCall(calleeFunc, args, "calltmp");
	}

	template <>
	llvm::Value* generate<ENCLOSED_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const ENCLOSED_EXPRESSION& enclosedExpressionNode = nodeBuffers.GetNode(nodeID).EnclosedExpression;

		return generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, enclosedExpressionNode.ExpressionID);
	}

	template <>
	llvm::Value* generate<BINARY_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const BINARY_EXPRESSION& binaryExpressionNode = nodeBuffers.GetNode(nodeID).BinaryExpression;

		llvm::Value* leftVal = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, binaryExpressionNode.LeftID);
		llvm::Value* rightVal = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, binaryExpressionNode.RightID);

		if (!leftVal || !rightVal)
		{
			return nullptr;
		}

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
			leftVal = state.Builder->CreateFCmpUEQ(leftVal, rightVal, "cmptmp");
			return state.Builder->CreateUIToFP(leftVal, llvm::Type::getDoubleTy(*state.Context), "booltmp");
		}

		if (operatorStr == "!=")
		{
			leftVal = state.Builder->CreateFCmpUNE(leftVal, rightVal, "cmptmp");
			return state.Builder->CreateUIToFP(leftVal, llvm::Type::getDoubleTy(*state.Context), "booltmp");
		}

		if (operatorStr == ">")
		{
			leftVal = state.Builder->CreateFCmpUGT(leftVal, rightVal, "cmptmp");
			return state.Builder->CreateUIToFP(leftVal, llvm::Type::getDoubleTy(*state.Context), "booltmp");
		}

		if (operatorStr == "<")
		{
			leftVal = state.Builder->CreateFCmpULT(leftVal, rightVal, "cmptmp");
			return state.Builder->CreateUIToFP(leftVal, llvm::Type::getDoubleTy(*state.Context), "booltmp");
		}

		if (operatorStr == ">=")
		{
			leftVal = state.Builder->CreateFCmpUGE(leftVal, rightVal, "cmptmp");
			return state.Builder->CreateUIToFP(leftVal, llvm::Type::getDoubleTy(*state.Context), "booltmp");
		}

		if (operatorStr == "<=")
		{
			leftVal = state.Builder->CreateFCmpULE(leftVal, rightVal, "cmptmp");
			return state.Builder->CreateUIToFP(leftVal, llvm::Type::getDoubleTy(*state.Context), "booltmp");
		}

		// additive operators

		if (operatorStr == "+")
		{
			return state.Builder->CreateFAdd(leftVal, rightVal, "addtmp");
		}

		if (operatorStr == "-")
		{
			return state.Builder->CreateFSub(leftVal, rightVal, "subtmp");
		}

		// multiplicative operators

		if (operatorStr == "*")
		{
			return state.Builder->CreateFMul(leftVal, rightVal, "multmp");
		}

		if (operatorStr == "/")
		{
			return state.Builder->CreateFDiv(leftVal, rightVal, "divtmp");
		}

		if (operatorStr == "%")
		{
			return state.Builder->CreateFRem(leftVal, rightVal, "modtmp");
		}

		ASSERT(false, "Unknown binary operator '{0}'!", operatorStr);
		return nullptr;
	}

	template <>
	llvm::Value* generate<UNARY_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const UNARY_EXPRESSION& unaryExpressionNode = nodeBuffers.GetNode(nodeID).UnaryExpression;

		std::string_view operatorStr = tokenBuffers.GetValue(unaryExpressionNode.OperatorTokenID).ToStringView();

		llvm::Value* expressionVal = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, unaryExpressionNode.OperandID);

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

	template <>
	llvm::Value* generate<PRIMARY_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const Node& node = nodeBuffers.GetNode(nodeID);

		switch (node.Kind)
		{
		case NodeKind::IDENTIFIER:
			return generate<IDENTIFIER>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::LITERAL:
			return generate<LITERAL>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::POINTER_INITIALIZER_EXPRESSION:
			return generate<POINTER_INITIALIZER_EXPRESSION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::INITIALIZER_LIST_EXPRESSION:
			return generate<INITIALIZER_LIST_EXPRESSION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::VALUE_DEFINITION_EXPRESSION:
			return generate<VALUE_DEFINITION_EXPRESSION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::ARRAY_ACCESS_EXPRESSION:
			return generate<ARRAY_ACCESS_EXPRESSION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::FUNCTION_CALL_EXPRESSION:
			return generate<FUNCTION_CALL_EXPRESSION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::ENCLOSED_EXPRESSION:
			return generate<ENCLOSED_EXPRESSION>(tokenBuffers, nodeBuffers, state, nodeID);

		default:
			ASSERT(false, "Unknown primary expression node kind!");
			return nullptr;
		}
	}

	template <>
	llvm::Value* generate<ASSIGNMENT_EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const ASSIGNMENT_EXPRESSION& assignmentExpressionNode = nodeBuffers.GetNode(nodeID).AssignmentExpression;
		const IDENTIFIER& identifierNode = nodeBuffers.GetNode(assignmentExpressionNode.IdentifierID).Identifier;

		const std::string_view identifierName = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

		llvm::Value* expressionVal = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, assignmentExpressionNode.ValueID);

		if (expressionVal == nullptr)
		{
			return nullptr;
		}

		// find the variable in the named values
		llvm::AllocaInst* allocaInst = state.NamedValues.GetValue(identifierName);

		if (allocaInst == nullptr)
		{
			return nullptr;
		}

		// store the value into the alloca
		return state.Builder->CreateStore(expressionVal, allocaInst);
	}

	template <>
	llvm::Value* generate<EXPRESSION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const auto& expressionNode = nodeBuffers.GetNode(nodeID);

		switch (expressionNode.Kind)
		{
		case NodeKind::BINARY_EXPRESSION:
			return generate<BINARY_EXPRESSION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::UNARY_EXPRESSION:
			return generate<UNARY_EXPRESSION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::IDENTIFIER:
		case NodeKind::LITERAL:
		case NodeKind::POINTER_INITIALIZER_EXPRESSION:
		case NodeKind::INITIALIZER_LIST_EXPRESSION:
		case NodeKind::VALUE_DEFINITION_EXPRESSION:
		case NodeKind::ARRAY_ACCESS_EXPRESSION:
		case NodeKind::FUNCTION_CALL_EXPRESSION:
		case NodeKind::ENCLOSED_EXPRESSION:
			return generate<PRIMARY_EXPRESSION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::ASSIGNMENT_EXPRESSION:
			return generate<ASSIGNMENT_EXPRESSION>(tokenBuffers, nodeBuffers, state, nodeID);

		default:
			ASSERT(false, "Unknown expression node kind!");
			return nullptr;

		}
	}

	template <>
	llvm::Value* generate<VALUE_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID);

	template <>
	llvm::Value* generate<STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID);

	template <>
	llvm::Value* generate<BLOCK_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID);

	template <>
	llvm::Value* generate<VALUE_DEFINITION_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const VALUE_DEFINITION_STATEMENT& valueDefinitionStatementNode = nodeBuffers.GetNode(nodeID).ValueDefinitionStatement;

		return generate<VALUE_DEFINITION>(tokenBuffers, nodeBuffers, state, valueDefinitionStatementNode.ValueDefinitionExpressionID);
	}

	template <>
	llvm::Value* generate<FUNCTION_CALL_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const FUNCTION_CALL_STATEMENT& functionCallStatementNode = nodeBuffers.GetNode(nodeID).FunctionCallStatement;

		return generate<FUNCTION_CALL_EXPRESSION>(tokenBuffers, nodeBuffers, state, functionCallStatementNode.FunctionCallExpressionID);
	}

	template <>
	llvm::Value* generate<ASSIGNMENT_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const ASSIGNMENT_STATEMENT& assignmentStatementNode = nodeBuffers.GetNode(nodeID).AssignmentStatement;

		return generate<ASSIGNMENT_EXPRESSION>(tokenBuffers, nodeBuffers, state, assignmentStatementNode.AssignmentExpressionID);
	}

	template <>
	llvm::Value* generate<FOR_LOOP_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const FOR_LOOP_STATEMENT& forLoopStatementNode = nodeBuffers.GetNode(nodeID).ForLoopStatement;

		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();

		ASSERT(func != nullptr, "No function to insert into!");

		// emit init code before the loop
		if (forLoopStatementNode.InitExpressionID != ERROR_NODE_ID)
		{
			llvm::Value* initVal = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, forLoopStatementNode.InitExpressionID);

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
		llvm::Value* bodyVal = generate<BLOCK_STATEMENT>(tokenBuffers, nodeBuffers, state, forLoopStatementNode.BodyID);

		if (bodyVal == nullptr)
		{
			return nullptr;
		}

		// emit step value
		if (forLoopStatementNode.IncrementExpressionID != ERROR_NODE_ID)
		{
			llvm::Value* stepVal = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, forLoopStatementNode.IncrementExpressionID);

			if (stepVal == nullptr)
			{
				return nullptr;
			}
		}

		// emit the condition
		llvm::Value* conditionVal = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, forLoopStatementNode.ConditionExpressionID);

		if (conditionVal == nullptr)
		{
			return nullptr;
		}

		// convert condition to a bool by comparing non-equal to 0.0
		conditionVal = state.Builder->CreateFCmpONE(conditionVal, llvm::ConstantFP::get(*state.Context, llvm::APFloat(0.0)), "loopcond");

		// create the "after loop" block and insert it
		llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(*state.Context, "afterloop", func);

		// insert the conditional branch into the end of afterBlock
		state.Builder->CreateCondBr(conditionVal, loopBlock, afterBlock);

		// any new code will be inserted in afterBlock
		state.Builder->SetInsertPoint(afterBlock);

		// for expr always returns 0.0
		return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*state.Context));
	}

	template <>
	llvm::Value* generate<WHILE_LOOP_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
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
		llvm::Value* bodyVal = generate<BLOCK_STATEMENT>(tokenBuffers, nodeBuffers, state, whileLoopStatement.BodyID);

		if (bodyVal == nullptr)
		{
			return nullptr;
		}

		// emit the condition
		llvm::Value* conditionVal = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, whileLoopStatement.ConditionExpressionID);

		if (conditionVal == nullptr)
		{
			return nullptr;
		}

		// convert condition to a bool by comparing non-equal to 0.0
		conditionVal = state.Builder->CreateFCmpONE(conditionVal, llvm::ConstantFP::get(*state.Context, llvm::APFloat(0.0)), "loopcond");

		// create the "after loop" block and insert it
		llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(*state.Context, "afterloop", func);

		// insert the conditional branch into the end of afterBlock
		state.Builder->CreateCondBr(conditionVal, loopBlock, afterBlock);

		// any new code will be inserted in afterBlock
		state.Builder->SetInsertPoint(afterBlock);

		// while expr always returns 0.0
		return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*state.Context));
	}

	template <>
	llvm::Value* generate<IF_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const IF_STATEMENT& ifStatementNode = nodeBuffers.GetNode(nodeID).IfStatement;

		llvm::Value* conditionVal = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, ifStatementNode.ConditionExpressionID);

		if (conditionVal == nullptr)
		{
			return nullptr;
		}

		// convert condition to a bool by comparing non-equal to 0.0
		conditionVal = state.Builder->CreateFCmpONE(conditionVal, llvm::ConstantFP::get(*state.Context, llvm::APFloat(0.0)), "ifcond");

		llvm::Function* func = state.Builder->GetInsertBlock()->getParent();

		ASSERT(func != nullptr, "No function to insert into!");

		// create blocks for the then and else cases
		// insert the 'then' block at the end of the function
		llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(*state.Context, "then", func);

		// insert the 'else' block at the end of the function
		llvm::BasicBlock* elseBlock = llvm::BasicBlock::Create(*state.Context, "else");

		// insert the 'merge' block at the end of the function
		llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(*state.Context, "ifcont");

		// insert the conditional branch into the end of the current block
		state.Builder->CreateCondBr(conditionVal, thenBlock, elseBlock);

		// emit then value
		state.Builder->SetInsertPoint(thenBlock);

		llvm::Value* thenVal = generate<STATEMENT>(tokenBuffers, nodeBuffers, state, ifStatementNode.BodyID);

		if (thenVal == nullptr)
		{
			return nullptr;
		}

		// insert the conditional branch into the end of the then block
		state.Builder->CreateBr(mergeBlock);

		// codegen of 'then' can change the current block, update thenBlock for the PHI
		thenBlock = state.Builder->GetInsertBlock();

		// emit else block if any

		// emit else block if any
		llvm::Value* elseVal = nullptr;

		func->insert(func->end(), elseBlock);
		state.Builder->SetInsertPoint(elseBlock);

		if (ifStatementNode.ElseID == ERROR_NODE_ID)
		{
			// no else statement, assume a null value for the PHINode object
			elseVal = llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*state.Context));
		}

		else
		{
			elseVal = generate<STATEMENT>(tokenBuffers, nodeBuffers, state, ifStatementNode.ElseID);

			if (elseVal == nullptr)
			{
				return nullptr;
			}
		}

		// insert the conditional branch into the end of the else block
		state.Builder->CreateBr(mergeBlock);

		// codegen of 'else' can change the current block, update elseBlock for the PHI
		elseBlock = state.Builder->GetInsertBlock();

		// emit merge block
		func->insert(func->end(), mergeBlock);
		state.Builder->SetInsertPoint(mergeBlock);

		// this call will assert if SDL checks are enabled in Visual Studio as per this article:
		// https://stackoverflow.com/questions/34892732/error-when-call-createphi-in-llvm
		// need to disable SDL checks for this code to run
		llvm::PHINode* phiNode = state.Builder->CreatePHI(llvm::Type::getDoubleTy(*state.Context), 2, "iftmp");

		phiNode->addIncoming(thenVal, thenBlock);
		phiNode->addIncoming(elseVal, elseBlock);

		return phiNode;
	}

	template <>
	llvm::Value* generate<BLOCK_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const BLOCK_STATEMENT& blockStatementNode = nodeBuffers.GetNode(nodeID).BlockStatement;

		llvm::Value* statementVal = nullptr;

		for (const NodeID statementID : blockStatementNode.StatementIDs)
		{
			statementVal = generate<STATEMENT>(tokenBuffers, nodeBuffers, state, statementID);

			if (statementVal == nullptr)
			{
				return nullptr;
			}
		}

		return statementVal;
	}

	template <>
	llvm::Value* generate<RETURN_STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const RETURN_STATEMENT& returnStatementNode = nodeBuffers.GetNode(nodeID).ReturnStatement;

		llvm::Value* expressionValue = generate<EXPRESSION>(tokenBuffers, nodeBuffers, state, returnStatementNode.ExpressionID);

		if (expressionValue == nullptr)
		{
			return nullptr;
		}

		return state.Builder->CreateRet(expressionValue);
	}

	template <>
	llvm::Value* generate<STATEMENT>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const Node& node = nodeBuffers.GetNode(nodeID);

		switch (node.Kind)
		{
		case NodeKind::VALUE_DEFINITION_STATEMENT:
			return generate<VALUE_DEFINITION_STATEMENT>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::FUNCTION_CALL_STATEMENT:
			return generate<FUNCTION_CALL_STATEMENT>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::ASSIGNMENT_STATEMENT:
			return generate<ASSIGNMENT_STATEMENT>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::FOR_LOOP_STATEMENT:
			return generate<FOR_LOOP_STATEMENT>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::WHILE_LOOP_STATEMENT:
			return generate<WHILE_LOOP_STATEMENT>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::IF_STATEMENT:
			return generate<IF_STATEMENT>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::BLOCK_STATEMENT:
			return generate<BLOCK_STATEMENT>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::RETURN_STATEMENT:
			return generate<RETURN_STATEMENT>(tokenBuffers, nodeBuffers, state, nodeID);

		default:
			ASSERT(false, "Unknown statement node kind!");
			return nullptr;
		}
	}

	template <>
	llvm::Value* generate<EXTERN_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const EXTERN_DEFINITION& externDefinitionNode = nodeBuffers.GetNode(nodeID).ExternDefinition;

		return generate<FUNCTION_DECLARATION>(tokenBuffers, nodeBuffers, state, externDefinitionNode.FunctionDeclarationID);
	}

	template <>
	llvm::Value* generate<VALUE_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const VALUE_DEFINITION& valueDefinitionNode = nodeBuffers.GetNode(nodeID).ValueDefinition;

		return generate<VALUE_DEFINITION_EXPRESSION>(tokenBuffers, nodeBuffers, state, valueDefinitionNode.ValueDefinitionExpressionID);
	}

	template <>
	llvm::Value* generate<STRUCT_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const STRUCT_DEFINITION& structDefinitionNode = nodeBuffers.GetNode(nodeID).StructDefinition;
		const IDENTIFIER& identifierNode = nodeBuffers.GetNode(structDefinitionNode.IdentifierID).Identifier;

		const std::string_view structName = tokenBuffers.GetValue(identifierNode.IdentifierTokenID).ToStringView();

		// collect all the member types
		std::vector<llvm::Type*> memberTypes;

		for (const NodeID memberTypeID : structDefinitionNode.MemberIDs)
		{
			llvm::Type* memberType = typeIdentifierToLLVMType(tokenBuffers, nodeBuffers, state, memberTypeID);

			if (memberType == nullptr)
			{
				return nullptr;
			}

			memberTypes.push_back(memberType);
		}

		llvm::StructType* structType = llvm::StructType::create(*state.Context, memberTypes, structName);

		if (structType == nullptr)
		{
			// TODO: log error
			return nullptr;
		}

		state.NamedValues.InsertType(structName, structType);

		return nullptr;
	}

	template <>
	llvm::Value* generate<ENUM_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		ASSERT(false, "To be implemented.");
		return nullptr;
	}

	template <>
	llvm::Value* generate<FUNCTION_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const FUNCTION_DEFINITION& functionDefinitionNode = nodeBuffers.GetNode(nodeID).FunctionDefinition;

		llvm::Function* func = static_cast<llvm::Function*>(generate<FUNCTION_DECLARATION>(tokenBuffers, nodeBuffers, state, functionDefinitionNode.FunctionDeclarationID));

		// push a new scope for the function
		state.NamedValues.PushScope(func->getName().str());

		// create a new basic block to start insertion into
		llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*state.Context, "entry", func);
		state.Builder->SetInsertPoint(entryBlock);

		// create allocations for function arguments
		for (auto& arg : func->args())
		{
			// create an alloca for this variable
			llvm::AllocaInst* allocaInst = createAlloca(entryBlock, std::string(arg.getName()), state.Builder->getDoubleTy());

			// store the initial value into the alloca
			state.Builder->CreateStore(&arg, allocaInst);

			// check that we do not have a named value with the same name
			if (state.NamedValues.GetValue(arg.getName().str()))
			{
				// TODO: log error
				func->removeFromParent();
				state.NamedValues.PopScope();
				return nullptr;
			}

			// add arguments to named values
			state.NamedValues.InsertValue(arg.getName().str(), allocaInst);
		}

		llvm::Value* bodyVal = generate<BLOCK_STATEMENT>(tokenBuffers, nodeBuffers, state, functionDefinitionNode.BodyID);

		if (!bodyVal)
		{
			// TODO: log error
			func->removeFromParent();
			state.NamedValues.PopScope();
			return nullptr;
		}

		// finish off the function
		state.Builder->CreateRet(bodyVal);

		// validate the generated code, checking for consistency
		llvm::verifyFunction(*func);

		if (state.Optimizations)
		{
			// run the optimizer on the function.
			state.FPM->run(*func, *state.FAM);
		}

		// restore the named values of the higher level
		state.NamedValues.PopScope();

		return func;
	}

	template <>
	llvm::Value* generate<DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const Node& node = nodeBuffers.GetNode(nodeID);

		switch (node.Kind)
		{
		case NodeKind::EXTERN_DEFINITION:
			return generate<EXTERN_DEFINITION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::VALUE_DEFINITION:
			return generate<VALUE_DEFINITION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::STRUCT_DEFINITION:
			return generate<STRUCT_DEFINITION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::ENUM_DEFINITION:
			return generate<ENUM_DEFINITION>(tokenBuffers, nodeBuffers, state, nodeID);

		case NodeKind::FUNCTION_DEFINITION:
			return generate<FUNCTION_DEFINITION>(tokenBuffers, nodeBuffers, state, nodeID);

		default:
			ASSERT(false, "Unknown definition node kind!");
			return nullptr;
		}
	}

	template <>
	llvm::Value* generate<QUALIFIED_DEFINITION>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const QUALIFIED_DEFINITION& qualifiedDefinitionNode = nodeBuffers.GetNode(nodeID).QualifiedDefinition;

		// TODO: where does the qualifier come in?

		return generate<DEFINITION>(tokenBuffers, nodeBuffers, state, qualifiedDefinitionNode.DefinitionID);
	}

	template <>
	llvm::Value* generate<MODULE>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const MODULE& moduleNode = nodeBuffers.GetNode(nodeID).Module;

		// generate code for each qualified definition in the module
		for (const NodeID qualifiedDefinitionID : moduleNode.QualifiedDefinitionIDs)
		{
			generate<QUALIFIED_DEFINITION>(tokenBuffers, nodeBuffers, state, qualifiedDefinitionID);
		}

		return nullptr;
	}

	template <>
	llvm::Value* generate<PROGRAM>(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, LLVMState& state, NodeID nodeID)
	{
		const PROGRAM& programNode = nodeBuffers.GetNode(nodeID).Program;

		// generate code for each module in the program
		// TODO: support for multiple modules
		return generate<MODULE>(tokenBuffers, nodeBuffers, state, programNode.ModuleIDs[0]);
	}

	llvm::Value* Generate(const TokenBuffers& tokenBuffers, const NodeBuffers& nodeBuffers, bool optimize)
	{
		const NodeID rootNodeID = nodeBuffers.GetRootNodeID();

		ASSERT(nodeBuffers.GetNode(rootNodeID).Kind == NodeKind::PROGRAM, "Root node must be a program node!");

		LLVMState state(optimize);

		llvm::Value* pValue = generate<PROGRAM>(tokenBuffers, nodeBuffers, state, rootNodeID);

		std::error_code errorCode;
		llvm::raw_fd_ostream out("out.ll", errorCode);
		state.Module->print(out, nullptr);

		return pValue;
	}
}
