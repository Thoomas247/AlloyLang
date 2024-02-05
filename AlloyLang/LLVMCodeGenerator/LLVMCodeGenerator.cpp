#include <vector>
#include "LLVMCodeGenerator.hpp"

using namespace llvm;
using namespace AlloyCompiler;

LLVMCodeGenerator::LLVMCodeGenerator(const AlloyCompiler::TokenBuffers& tokenBuffers,
	const AlloyCompiler::NodeBuffers& nodeBuffers)
	: NodeBuffers(nodeBuffers), TokenBuffers(tokenBuffers) {

	// Open a new context and module.
	TheContext = std::make_unique<LLVMContext>();
	TheModule = std::make_unique<llvm::Module>("AlloyLang", *TheContext);

	// Create a new builder for the module.
	Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

LLVMCodeGenerator::~LLVMCodeGenerator() {

}

int LLVMCodeGenerator::Process() {
    int result = 0;
    NodeID root = NodeBuffers.GetRootNodeID();
    if (ERROR_NODE_ID == root) {
        // TBD: implement error handling
        assert(false);
        return -1;
    }

	const Node& rootNode = NodeBuffers.GetNode(root);

	result = (codegen(rootNode) ? 0 : -1);
	TheModule->print(errs(), nullptr);
	return result;
}

Value* LLVMCodeGenerator::codegen(const Node& node) {

	Value* result = nullptr;

	switch (node.Kind) {

	case NodeKind::LITERAL:							// integer_literal | float_literal | boolean_literal | string_literal | character_literal;
		result = codegen(node.Literal);
		break;

	case NodeKind::IDENTIFIER:						// identifier;
		result = codegen(node.Identifier);
		break;

	case NodeKind::TYPE_IDENTIFIER:					// [reference | pointer] IDENTIFIER;
	case NodeKind::TYPE_DECLARATION:				// [variable | constant] TYPE_IDENTIFIER;
		assert(false);	// To be implemented
		break;

	case NodeKind::VALUE_DECLARATION:				// (variable | constant) IDENTIFIER colon TYPE_IDENTIFIER;
		// nothing to do, handled by VALUE_DEFINITION_EXPRESSION, STRUCT_DEFINITION or FUNCTION_DEFINITION
		break;

	/// NodeKind::EXPRESSION group

	case NodeKind::VALUE_DEFINITION_EXPRESSION:		// VALUE_DECLARATION assignment_operator EXPRESSION;
		result = codegen(node.ValueDefinitionExpression);
		break;

	case NodeKind::FUNCTION_CALL_EXPRESSION:		// IDENTIFIER open_paren[EXPRESSION{ comma EXPRESSION }] close_paren;
		result = codegen(node.FunctionCallExpression);
		break;

	case NodeKind::ENCLOSED_EXPRESSION:				// open_paren EXPRESSION close_paren;
		result = codegen(node.EnclosedExpression);
		break;

	case NodeKind::BINARY_EXPRESSION:				// EXPRESSION binary_operator EXPRESSION;
		result = codegen(node.BinaryExpression);
		break;

	case NodeKind::UNARY_EXPRESSION:				// unary_operator PRIMARY_EXPRESSION;
	case NodeKind::ASSIGNMENT_EXPRESSION:			// IDENTIFIER assignment_operator EXPRESSION;
		assert(false);	// To be implemented
		break;

	/// End of NodeKind::EXPRESSION group

	/// NodeKind::STATEMENT group

	case NodeKind::VALUE_DEFINITION_STATEMENT:		// VALUE_DEFINITION_EXPRESSION semicolon;
	case NodeKind::ASSIGNMENT_STATEMENT:			// ASSIGNMENT_EXPRESSION semicolon;
	case NodeKind::FOR_LOOP_STATEMENT:				// for open_paren EXPRESSION semicolon EXPRESSION semicolon EXPRESSION close_paren STATEMENT;
	case NodeKind::WHILE_LOOP_STATEMENT:			// while ENCLOSED_EXPRESSION STATEMENT;
	case NodeKind::IF_STATEMENT:					// if ENCLOSED_EXPRESSION STATEMENT[else STATEMENT];
		assert(false);	// To be implemented
		break;

	case NodeKind::BLOCK_STATEMENT:					// open_brace{ STATEMENT } close_brace;
		result = codegen(node.BlockStatement);
		break;

	case NodeKind::RETURN_STATEMENT:				// return[EXPRESSION] semicolon;
		result = codegen(node.ReturnStatement);
		break;

	/// End of NodeKind::STATEMENT group

	/// NodeKind::DEFINITION group

	case NodeKind::VALUE_DEFINITION:				// VALUE_DEFINITION_EXPRESSION semicolon ;
		result = codegen(node.ValueDefinition);
		break;

	case NodeKind::STRUCT_DEFINITION:				// struct IDENTIFIER open_brace { VALUE_DECLARATION semicolon } close_brace ; (* TODO: force default values? *)
		assert(false);	// To be implemented
		break;

	case NodeKind::ENUM_DEFINITION:					// enum IDENTIFIER open_brace IDENTIFIER { comma IDENTIFIER } close_brace ;
		assert(false);	// To be implemented
		break;

	case NodeKind::FUNCTION_DEFINITION:				// function IDENTIFIER open_paren[VALUE_DECLARATION{ comma VALUE_DECLARATION }] close_paren[arrow TYPE_DECLARATION] BLOCK_STATEMENT;
		codegen(node.FunctionDefinition);
		break;

	case NodeKind::QUALIFIED_DEFINITION:
		result = codegen(node.QualifiedDefinition.DefinitionID);
		break;

	/// End of NodeKind::DEFINITION group

	case NodeKind::MODULE:
	{
		std::vector<NodeID>::const_iterator iter = node.Module.QualifiedDefinitionIDs.begin();
		for (iter; iter < node.Module.QualifiedDefinitionIDs.end(); iter++) {
			codegen(*iter);
		}
		break;
	}

	case NodeKind::PROGRAM:
	{
		std::vector<NodeID>::const_iterator iter = node.Program.ModuleIDs.begin();
		for (iter; iter < node.Program.ModuleIDs.end(); iter++) {
			codegen(*iter);
		}
		break;
	}

	default:
		assert(false);
		break;
	}
	return result;
}

Value* LLVMCodeGenerator::codegen(NodeID nodeID) {
	Value* result = nullptr;

	// TBD: how do we know if nodeID actually exists?
	// if (ERROR_NODE_ID != NodeBuffers.GetNode(nodeID))
	{
		result = codegen(NodeBuffers.GetNode(nodeID));
	}

	return result;
}

Value* LLVMCodeGenerator::codegen(const VALUE_DEFINITION& node) {
    //
    // Expression of type: [const] identifier = value;	(with the semi-colon at the end)
    //
	return codegen(node.ValueDefinitionExpressionID);
}

Value* LLVMCodeGenerator::codegen(const VALUE_DEFINITION_EXPRESSION& node) {
	//
	// Expression of type: [const] identifier = value
	// The identifier can be either global or local to the current function
	// 
	// get the name of the identifier
	assert(NodeBuffers.GetNode(node.ValueDeclarationID).Kind == NodeKind::VALUE_DECLARATION);
	const VALUE_DECLARATION& declaration = NodeBuffers.GetNode(node.ValueDeclarationID).ValueDeclaration;
	const IDENTIFIER& identifier = NodeBuffers.GetNode(declaration.IdentifierID).Identifier;
	std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

	// now get the value by recursively calling codegen
	Value* value = codegen(node.ValueID);

	if (Builder->GetInsertBlock() == nullptr) {
		// If no insertion block, we are creating global variables
		GlobalVariable* gv = new GlobalVariable(*TheModule,
			Type::getDoubleTy(*TheContext),
			(declaration.Kind == VALUE_DECLARATION::Type::Constant),   // isConstant
			GlobalValue::CommonLinkage,
			nullptr,                        // initializer specified below
			Name
		);
		gv->setAlignment(Align(sizeof(double)));

		// currently assuming the initializer is constant
		ConstantFP* ptr_2 = (ConstantFP*)value;
		gv->setInitializer(ptr_2);
	}
	else {
		// add the variable to the end of the insertion block (e.g. function local variables)
		IRBuilder<> TmpB(Builder->GetInsertBlock(), Builder->GetInsertBlock()->end());
		// create a mutable variable
		AllocaInst* A = TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr, Name);
		// set the value
		Builder->CreateStore(value, A);
		// store in the local variables map
		NamedValues[Name] = A;
	}

	return value;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::IDENTIFIER& node) {
    //
    // return the value of a local or global variable
    // 
    // Lookup this variable in the current function
    const std::string Name(TokenBuffers.GetValue(node.IdentifierTokenID).ToStringView());
    Value* value = nullptr;
    AllocaInst* A = NamedValues[Name];
    if (A) {
        // Found in the local variables, load the value
        value = Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
    }
    else {
        // not a local variable, check the globals
        GlobalVariable* gv = TheModule->getGlobalVariable(Name);
        if (gv) {
            value = gv->getInitializer();
        }
        else {
            // TBD: LogErrorV("Unknown variable name");
            assert(false);
        }
    }

    return value;
}

Value* LLVMCodeGenerator::codegen(const BINARY_EXPRESSION& node) {
	Value* result = nullptr;
	Value* L = codegen(node.LeftID);
	Value* R = codegen(node.RightID);

	const std::string op(TokenBuffers.GetValue(node.OperatorTokenID).ToStringView());


    if (L && R) {
        if (op == "+")
            result = Builder->CreateFAdd(L, R, "addtmp");
        else if (op == "-")
            result = Builder->CreateFSub(L, R, "subtmp");
        else if (op == "*")
            result = Builder->CreateFMul(L, R, "multmp");
        else if (op == "<") {
            L = Builder->CreateFCmpULT(L, R, "cmptmp");
            // Convert bool 0/1 to double 0.0 or 1.0
            result = Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
        }
        else {
            // TBD
            printf("invalid binary operator\n");
            assert(false);
        }
    }
    return result;
}

Value* LLVMCodeGenerator::codegen(const LITERAL& node) {
	// TBD: currently converting all numbers to float while we improve codegen(const Binary& node) 
	//    return ConstantInt::get(*TheContext, APInt(64, node.Value));
	const std::string val(TokenBuffers.GetValue(node.InfoTokenID).ToStringView());
	return ConstantFP::get(*TheContext, APFloat(atof(val.c_str())));
}

/* called when top level expression is encountered
Value* LLVMCodeGenerator::HandleTopLevelExpression(const AlloyCompiler::Node& node) {
	std::unique_ptr<PrototypeAST> Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
	std::unique_ptr<FunctionAST> Function = std::make_unique<FunctionAST>(std::move(Proto), node);

	// Evaluate a top-level expression into an anonymous function.
	if (auto* FnIR = codegen(*Function)) {
		FnIR->print(errs());
		fprintf(stderr, "\n");

		// Remove the anonymous expression.
		FnIR->eraseFromParent();
	}

	return nullptr;
}
*/

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
/// TBD: only works for doubles, need to implement other types
AllocaInst* LLVMCodeGenerator::CreateEntryBlockAlloca(Function* TheFunction, const std::string& VarName) {
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
		TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr,
		VarName);
}

Function* LLVMCodeGenerator::createFunctionPrototype(const std::string & Name, const AlloyCompiler::FUNCTION_DEFINITION & node) {
	//
	// Helper method to create the protoype of a function
	//

	// Make the function type: double(double,double) etc.
	std::vector<Type*> Doubles(node.ParameterIDs.size(), Type::getDoubleTy(*TheContext));
	FunctionType* FT = FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);

	Function* F = Function::Create(FT, Function::ExternalLinkage, Name, *TheModule);

	// Set names for all arguments.
	unsigned Idx = 0;
	for (auto& Arg : F->args()) {
		assert(NodeBuffers.GetNode(node.ParameterIDs[Idx]).Kind == NodeKind::VALUE_DECLARATION);		// make sure we are getting back the right node type
		const VALUE_DECLARATION& temp = NodeBuffers.GetNode(node.ParameterIDs[Idx]).ValueDeclaration;
		const IDENTIFIER& identifier = NodeBuffers.GetNode(temp.IdentifierID).Identifier;
		std::string argName(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());
		Arg.setName(argName);
		Idx++;
	}

	return F;
}

Function* LLVMCodeGenerator::codegen(const AlloyCompiler::FUNCTION_DEFINITION& node) {
	//
	// Function definition in the form of fn function( prarameter, parameter, ... ) ->  type { statements }
	//

	// retrieve the function name
	assert(NodeBuffers.GetNode(node.IdentifierID).Kind == NodeKind::IDENTIFIER);		// make sure we are getting back the right node type
	const IDENTIFIER& identifier = NodeBuffers.GetNode(node.IdentifierID).Identifier;
	std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

	// First, check for an existing function from a previous declaration
	Function* F = TheModule->getFunction(Name);

	if (F) {
		// TBD check for function signature
		// devise a way to have multiple function with same name but different parameters
		assert(false);
		return nullptr;
	}

	// create the llvm function prototype
	F = createFunctionPrototype(Name, node);
	if (!F) {
		// Error creating the prototype
		assert(false);
		return nullptr;
	}

	// Create a new basic block to start insertion into.
	BasicBlock* BB = BasicBlock::Create(*TheContext, "entry", F);
	Builder->SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues.clear();
	for (auto& Arg : F->args()) {
		// Create an alloca for this variable.
		AllocaInst* Alloca = CreateEntryBlockAlloca(F, std::string(Arg.getName()));

		// Store the initial value into the alloca.
		Builder->CreateStore(&Arg, Alloca);

		// Add arguments to variable symbol table.
		NamedValues[std::string(Arg.getName())] = Alloca;
	}

	if (Value* RetVal = codegen(node.BodyID)) {
		// Finish off the function.
		Builder->CreateRet(RetVal);

		// Validate the generated code, checking for consistency.
		verifyFunction(*F);

		return F;
	}


	// Error reading body, remove function.
	F->eraseFromParent();
	return nullptr;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::BLOCK_STATEMENT& node) {
	//
	// Block of code statements { statement; statement; ... }
	//
	Value* result = nullptr;

	for (auto& S : node.StatementIDs) {
		result = codegen(S);
	}

	return result;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::FUNCTION_CALL_EXPRESSION& node) {
	//
	// Function call: identifier(expression, expressoin, ...)
	//
	assert(NodeBuffers.GetNode(node.IdentifierID).Kind == NodeKind::IDENTIFIER);		// make sure we are getting back the right node type
	const IDENTIFIER& identifier = NodeBuffers.GetNode(node.IdentifierID).Identifier;
	std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

	/*
	Builder->CreateCall(FunctionCallee(Name))
	*/
	assert(false);
	return nullptr;	

}
