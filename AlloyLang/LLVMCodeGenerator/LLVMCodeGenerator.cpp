#include "llvm.hpp"
#include "LLVMCodeGenerator.hpp"
#include "NamedValues.hpp"

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

	// tree of named values that is passed around the code generator
	RootNamedValues = std::make_unique<CGNamedValues>();
	NamedValues = std::move(RootNamedValues);

#ifndef NO_CODE_OPTIMIZATION
	// Check the LLVM tutorial for details about these optimizations
	// Create new pass and analysis managers.
	TheFPM = std::make_unique<FunctionPassManager>();
	TheLAM = std::make_unique<LoopAnalysisManager>();
	TheFAM = std::make_unique<FunctionAnalysisManager>();
	TheCGAM = std::make_unique<CGSCCAnalysisManager>();
	TheMAM = std::make_unique<ModuleAnalysisManager>();
	ThePIC = std::make_unique<PassInstrumentationCallbacks>();
	TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
		/*DebugLogging*/ true);
	TheSI->registerCallbacks(*ThePIC, TheMAM.get());

	// Add transform passes.
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	TheFPM->addPass(InstCombinePass());
	// Reassociate expressions.
	TheFPM->addPass(ReassociatePass());
	// Eliminate Common SubExpressions.
	TheFPM->addPass(GVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	TheFPM->addPass(SimplifyCFGPass());

	// Register analysis passes used in these transform passes.
	PassBuilder PB;
	PB.registerModuleAnalyses(*TheMAM);
	PB.registerFunctionAnalyses(*TheFAM);
	PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
#endif	// NO_CODE_OPTIMIZATION
}

LLVMCodeGenerator::~LLVMCodeGenerator() {

}

int LLVMCodeGenerator::Process(llvm::raw_ostream* llvmOutput /*= nullptr*/) {
	int result = 0;
	NodeID root = NodeBuffers.GetRootNodeID();
	if (ERROR_NODE_ID == root) {
		// TBD: implement error handling
		assert(false);
		return -1;
	}

	const Node& rootNode = NodeBuffers.GetNode(root);

	result = (codegen(rootNode) ? 0 : -1);
	if (llvmOutput) {
		TheModule->print(*llvmOutput, nullptr);
	}
	else {
		std::error_code EC;
		raw_fd_ostream out("out.ll", EC);
		TheModule->print(out, nullptr);
	}
	return result;
}

int LLVMCodeGenerator::Execute() {
	//
	// execute generated code
	//
#ifndef NO_CODE_EXECUTION
	InitializeNativeTarget();
	ExecutionEngine* executionEngine = EngineBuilder(std::move(TheModule)).setEngineKind(llvm::EngineKind::Interpreter).create();
	if (executionEngine) {
		executionEngine->DisableLazyCompilation();
		Function* main = executionEngine->FindFunctionNamed(StringRef("main"));
		auto result = executionEngine->runFunction(main, {});
		delete executionEngine;
	}
	else {
		assert(false);	// could not instantiate execution engine
	}

#endif // NO_CODE_EXECUTION
	return 0;
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
		result = nullptr;
		break;

	case NodeKind::FUNCTION_DECLARATION:			// function IDENTIFIER open_paren [ VALUE_DECLARATION { comma VALUE_DECLARATION } ] close_paren [ arrow TYPE_DECLARATION ] ;
		result = codegen(node.FunctionDeclaration);
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
		assert(false);	// To be implemented
		break;

	case NodeKind::ASSIGNMENT_EXPRESSION:			// IDENTIFIER assignment_operator EXPRESSION;
		result = codegen(node.AssignmentExpression);
		break;

		/// End of NodeKind::EXPRESSION group

	/// NodeKind::STATEMENT group

	case NodeKind::ASSIGNMENT_STATEMENT:			// ASSIGNMENT_EXPRESSION semicolon;
		result = codegen(node.AssignmentStatement);
		break;

	case NodeKind::WHILE_LOOP_STATEMENT:			// while ENCLOSED_EXPRESSION STATEMENT;
		assert(false);	// To be implemented
		break;

	case NodeKind::VALUE_DEFINITION_STATEMENT:		// VALUE_DEFINITION_EXPRESSION semicolon;
		result = codegen(node.ValueDefinitionStatement);
		break;

	case NodeKind::FUNCTION_CALL_STATEMENT:			// FUNCTION_CALL_EXPRESSION semicolon
		result = codegen(node.FunctionCallStatement);
		break;

	case NodeKind::FOR_LOOP_STATEMENT:				// for open_paren EXPRESSION semicolon EXPRESSION semicolon EXPRESSION close_paren STATEMENT;
		result = codegen(node.ForLoopStatement);
		break;

	case NodeKind::IF_STATEMENT:					// if ENCLOSED_EXPRESSION STATEMENT[else STATEMENT];
		result = codegen(node.IfStatement);
		break;

	case NodeKind::BLOCK_STATEMENT:					// open_brace{ STATEMENT } close_brace;
		result = codegen(node.BlockStatement);
		break;

	case NodeKind::RETURN_STATEMENT:				// return[EXPRESSION] semicolon;
		result = codegen(node.ReturnStatement);
		break;

		/// End of NodeKind::STATEMENT group

		/// NodeKind::DEFINITION group

	case NodeKind::EXTERN_DEFINITION:				// extern_keyword FUNCTION_DECLARATION semicolon ;
		result = codegen(node.ExternDefinition);
		break;

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
		result = codegen(node.FunctionDefinition);
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
			GlobalValue::InternalLinkage,	// TBD: check CLang source-code the proper options for creating globals
			nullptr,                        // initializer specified below
			Name
		);
		gv->setAlignment(Align(sizeof(double)));
		gv->setDSOLocal(true);

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
		NamedValues->insert(Name, A);
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
	AllocaInst* A = NamedValues->contains(Name, true);
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

bool LLVMCodeGenerator::updateValueOfLocalOrGlobalVariable(const std::string& Name, Value* Value) {
	//
	// update the value of a local or global variable
	//
	bool result = false;

	// Lookup this variable in the current function
	AllocaInst* A = NamedValues->contains(Name, true);
	if (A) {
		// Found in the local variables, save the value
		result = (Builder->CreateStore(Value, A) != nullptr);
	}
	else {
		// not a local variable, check the globals
		GlobalVariable* gv = TheModule->getGlobalVariable(Name);
		if (gv) {
			gv->setInitializer((llvm::Constant*)Value);
			result = true;
		}
		else {
			// TBD: LogErrorV("Unknown variable name");
			assert(false);
		}
	}

	return result;
}

Value* LLVMCodeGenerator::codegen(const BINARY_EXPRESSION& node) {
	Value* result = nullptr;
	const std::string op(TokenBuffers.GetValue(node.OperatorTokenID).ToStringView());

	if (op == "=") {
		// special case for the assignment operator
		// we encounter this in cases such as the last expression of: for (var i : i64 = 0; i < c; i = i + 1)
		ASSIGNMENT_EXPRESSION expr = { node.LeftID, node.OperatorTokenID, node.RightID };
		result = codegen(expr);
	}
	else {

		Value* L = codegen(node.LeftID);
		Value* R = codegen(node.RightID);

		// we need the 2 operands to be of the same type, if not convert to same
		Type* operandType = CGNamedValues::MakeCompatible(L, R);

		if (L && R) {
			if (op == "+")
				result = Builder->CreateFAdd(L, R, "addtmp");
			else if (op == "-")
				result = Builder->CreateFSub(L, R, "subtmp");
			else if (op == "*")
				result = Builder->CreateFMul(L, R, "multmp");
			else if (op == "==") {
				L = Builder->CreateFCmpOEQ(L, R, "equtmp");
				// Convert bool 0/1 to double 0.0 or 1.0
				result = Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
			}
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
	}
	return result;
}

Value* LLVMCodeGenerator::codegen(const LITERAL& node) {
	//
	// Convert a literal to an LLVM Value
	//
	Value* result = nullptr;

	const std::string val(TokenBuffers.GetValue(node.InfoTokenID).ToStringView());
	switch (node.Kind) {
		case LITERAL::Type::String:
			result = Builder->CreateGlobalStringPtr(val, "stringval");
			break;

		// TBD: currently converting all numbers to float while we improve codegen(const Binary& node) 
	default:
		result = ConstantFP::get(*TheContext, APFloat(atof(val.c_str())));
		break;

	}

	return result;
}

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
/// TBD: only works for doubles, need to implement other types
AllocaInst* LLVMCodeGenerator::CreateEntryBlockAlloca(Function* TheFunction, const std::string& VarName) {
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr, VarName);
}


Function* LLVMCodeGenerator::createFunctionPrototype(const std::string& Name, const AlloyCompiler::FUNCTION_DECLARATION& node) {
	//
	// Helper method to create the protoype of a function
	//

	// convert each of the parameter types into llvm types
	std::vector<Type*> ParamTypes;
	for (auto id : node.ParameterIDs) {
		assert(NodeBuffers.GetNode(id).Kind == NodeKind::VALUE_DECLARATION);
		const VALUE_DECLARATION& vd = NodeBuffers.GetNode(id).ValueDeclaration;
		llvm::Type* type = CGNamedValues::AlloyToLLVMType(*TheContext, NodeBuffers, TokenBuffers, vd.TypeIdentifierID);
		ParamTypes.push_back(type);
	}

	// now convert the return type to llvm
	const TYPE_DECLARATION& td = NodeBuffers.GetNode(node.ReturnTypeID).TypeDeclaration;
	Type* returnType = CGNamedValues::AlloyToLLVMType(*TheContext, NodeBuffers, TokenBuffers, td.TypeIdentifierID);

	// Make the function type: return type(param type, param type, ...)
	// TBD: the last parameter should be true only for variable number of parmeters (e.g. printf), currently assuming all functions are variable parameters
	FunctionType* FT = FunctionType::get(returnType, ParamTypes, true);

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

Function* LLVMCodeGenerator::codegen(const AlloyCompiler::FUNCTION_DECLARATION& node) {
	//
	// function declaration in the form of fn IDENTIFIER ( parameter, parameter, ... ) -> return type;
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

	return F;
}

Function* LLVMCodeGenerator::codegen(const AlloyCompiler::FUNCTION_DEFINITION& node) {
	//
	// Function definition in the form of fn function( prarameter, parameter, ... ) ->  type { statements }
	//

	Function* F = static_cast<llvm::Function*>(codegen(node.FunctionDeclarationID));

	// Create a new basic block to start insertion into.
	BasicBlock* BB = BasicBlock::Create(*TheContext, "entry", F);
	Builder->SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues->clear();
	for (auto& Arg : F->args()) {
		// Create an alloca for this variable.
		AllocaInst* Alloca = CreateEntryBlockAlloca(F, std::string(Arg.getName()));

		// Store the initial value into the alloca.
		Builder->CreateStore(&Arg, Alloca);

		// Add arguments to variable symbol table.
		NamedValues->insert(std::string(Arg.getName()), Alloca);
	}

	// create a new local names map for the function body
	NamedValues = std::make_unique<CGNamedValues>(std::move(NamedValues));

	ConstantInt* retval = static_cast<ConstantInt *>(codegen(node.BodyID));
	
	if (retval && retval->isOne()) {
		// Validate the generated code, checking for consistency.
		verifyFunction(*F);

#ifndef NO_CODE_OPTIMIZATION
		// Run the optimizer on the function.
		TheFPM->run(*F, *TheFAM);
#endif  // NO_CODE_OPTIMIZATION
	}
	else {
		// Error reading body, remove function
		assert(false);
		F->eraseFromParent();
		F = nullptr;
	}

	// restore the named values of the higher level
	NamedValues = std::move(NamedValues->getParent());

	return F;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::BLOCK_STATEMENT& node) {
	//
	// Block of code statements { statement; statement; ... }
	// returns ConstantInt True or False depending on success or failure
	//
	Value* result = ConstantInt::getTrue(*TheContext);

	// create a new local names map for each block statement, any variables declared inside the block are limited to this scope
	NamedValues = std::make_unique<CGNamedValues>(std::move(NamedValues));

	for (const NodeID& S : node.StatementIDs) {

		ConstantInt* stmtResult = static_cast<ConstantInt *>(codegen(S));
		if (stmtResult == nullptr || stmtResult->isZero()) {
			assert(false);
			result = ConstantInt::getFalse(*TheContext);
			break;
		}
		
		if (NodeKind::RETURN_STATEMENT == NodeBuffers.GetNode(S).Kind) {
			// TBD: generate a warning that we encountered unreachable statements
			break;			// ignore any statements after return

		} 
	}

	// restore the named values of the higher level
	NamedValues = std::move(NamedValues->getParent());

	return result;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::ASSIGNMENT_EXPRESSION& node) {
	//
	// identifier = expression
	//
	// TBD: what is ASSIGNMENT_EXPRESSION.OperatorTokenID for?
	Value* result = nullptr;

	assert(NodeBuffers.GetNode(node.IdentifierID).Kind == NodeKind::IDENTIFIER);		// make sure we are getting back the right node type
	const IDENTIFIER& identifier = NodeBuffers.GetNode(node.IdentifierID).Identifier;
	std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

	// now get the value by recursively calling codegen
	result = codegen(node.ValueID);

	if (!updateValueOfLocalOrGlobalVariable(Name, result)) {
		// error updating the value of local or global variable
		assert(false);
	}

	return result;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::FUNCTION_CALL_EXPRESSION& node) {
	//
	// Function call: identifier(expression, expression, ...)
	//
	assert(NodeBuffers.GetNode(node.IdentifierID).Kind == NodeKind::IDENTIFIER);		// make sure we are getting back the right node type
	const IDENTIFIER& identifier = NodeBuffers.GetNode(node.IdentifierID).Identifier;
	std::string Name(TokenBuffers.GetValue(identifier.IdentifierTokenID).ToStringView());

	// Look up the name in the global module table.
	Function* CalleeF = TheModule->getFunction(Name);
	if (!CalleeF) {
		// Unknown function referenced
		assert(false);
		return nullptr;
	}

	// If argument mismatch error.
	if (CalleeF->arg_size() != node.ArgumentIDs.size()) {
		// Incorrect # arguments passed
		assert(false);
		return nullptr;
	}

	// evaluate all arguments by calling codegen on each expression
	std::vector<Value*> ArgsV;
	for (size_t i = 0, e = node.ArgumentIDs.size(); i != e; ++i) {
		ArgsV.push_back(codegen(node.ArgumentIDs[i]));
		if (!ArgsV.back())
			return nullptr;
	}

	return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::IF_STATEMENT& node) {
	//
	// if (expression) statements [else statements]
	// returns ConstantInt True or False depending on success or failure
	//
	Value* result = ConstantInt::getFalse(*TheContext);

	Value* CondV = codegen(node.ConditionExpressionID);
	if (!CondV) {
		// error evaluating condition
		assert(false);
		return result;
	}

	// Convert condition to a bool by comparing non-equal to 0.0.
	CondV = Builder->CreateFCmpONE(CondV, ConstantFP::get(*TheContext, APFloat(0.0)), "ifcond");

	// we are supposed to be inside a function, otherwise this is going to fail
	Function* TheFunction = Builder->GetInsertBlock()->getParent();
	if (!TheFunction) {
		// not inside a function or something missing in the codegen of the function
		assert(false);
		return result;
	}

	// Create blocks for the then and else cases
	// Insert the 'then' block at the end of the function
	BasicBlock* ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
	// the else block is optional but we still need an empty else statement otherwise the optimizer crashes
	BasicBlock* ElseBB = BasicBlock::Create(*TheContext, "else");
	BasicBlock* MergeBB = BasicBlock::Create(*TheContext, "ifcont");
	Builder->CreateCondBr(CondV, ThenBB, ElseBB);

	// Emit then statements
	Builder->SetInsertPoint(ThenBB);
	ConstantInt* ThenV = static_cast<ConstantInt *>(codegen(node.BodyID));
	if (ThenV == nullptr || ThenV->isZero()) {
		assert(false);
		return result;
	}

	Builder->CreateBr(MergeBB);
	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = Builder->GetInsertBlock();

	// Emit else block if any
	ConstantInt* ElseV = nullptr;
	if (ElseBB) {
		TheFunction->insert(TheFunction->end(), ElseBB);
		Builder->SetInsertPoint(ElseBB);
		if (ERROR_NODE_ID == node.ElseID) {
			// no else statement, return success
			result = ConstantInt::getTrue(*TheContext);;
		}
		else {
			ElseV = static_cast<ConstantInt*>(codegen(node.ElseID));
			if (ElseV == nullptr || ElseV->isZero()) {
				assert(false);
				return result;
			}
		}
	}

	Builder->CreateBr(MergeBB);

	// codegen of 'Else' can change the current block, update ElseBB for the PHI.
	ElseBB = Builder->GetInsertBlock();

	// Emit merge block.
	TheFunction->insert(TheFunction->end(), MergeBB);
	Builder->SetInsertPoint(MergeBB);

	/* Note: This call will assert if SDL checks are enabled in Visual Studio as per this article :
	// https://stackoverflow.com/questions/34892732/error-when-call-createphi-in-llvm
	// Need to disable SDL checks for this code to run
	PHINode* PN = Builder->CreatePHI(Type::getDoubleTy(*TheContext), 2, "iftmp");

	PN->addIncoming(ThenV, ThenBB);
	PN->addIncoming(ElseV, ElseBB);
	*/
	result = ConstantInt::getTrue(*TheContext);
	return result;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::FOR_LOOP_STATEMENT& node) {
	// 
	// for ( EXPRESSION; EXPRESSION; EXPRESSION ) STATEMENT 
	// returns ConstantInt True or False depending on success or failure
	// 
	// Output for-loop as:
	//   var = alloca double
	//   ...
	//   start = startexpr
	//   store start -> var
	//   goto loop
	// loop:
	//   ...
	//   bodyexpr
	//   ...
	// loopend:
	//   step = stepexpr
	//   endcond = endexpr
	//
	//   curvar = load var
	//   nextvar = curvar + step
	//   store nextvar -> var
	//   br endcond, loop, endloop
	// outloop:

	ConstantInt* result = ConstantInt::getFalse(*TheContext);

	// we are supposed to be inside a function, otherwise this is going to fail
	Function* TheFunction = Builder->GetInsertBlock()->getParent();
	if (!TheFunction) {
		// not inside a function or something missing in the codegen of the function
		assert(false);
		return result;
	}

	// create a new local names map for each block statement, any variables declared inside the block are limited to this scope
	NamedValues = std::make_unique<CGNamedValues>(std::move(NamedValues));

	// Emit the start code first if any.
	if (ERROR_NODE_ID != node.InitExpressionID) {
		if (!codegen(node.InitExpressionID)) {
			// error evaluating start expression
			assert(false);

			// restore the named values of the higher level
			NamedValues = std::move(NamedValues->getParent());

			return result;
		}
	}

	// Make the new basic block for the loop header, inserting after current block.
	BasicBlock* LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);

	// Insert an explicit fall through from the current block to the LoopBB.
	Builder->CreateBr(LoopBB);

	// Start insertion in LoopBB.
	Builder->SetInsertPoint(LoopBB);

	// Emit the body of the loop.  This, like any other expr, can change the
	// current BB.  Note that we ignore the value computed by the body, but don't
	// allow an error.
	result = static_cast<ConstantInt *>(codegen(node.BodyID));
	if (nullptr == result || result->isZero()) {
		assert(false);
		return result;
	}

	// Emit the (optional) step value.
	Value* StepVal = nullptr;
	if (ERROR_NODE_ID != node.IncrementExpressionID) {
		StepVal = codegen(node.IncrementExpressionID);
		if (!StepVal) {
			// error evaluating expression
			assert(false);

			// restore the named values of the higher level
			NamedValues = std::move(NamedValues->getParent());

			return result;
		}
	}

	// Compute the end condition.
	Value* EndCond = codegen(node.ConditionExpressionID);
	if (!EndCond) {
		// error evaluating end expression
		assert(false);

		// restore the named values of the higher level
		NamedValues = std::move(NamedValues->getParent());

		return result;
	}

	// Convert condition to a bool by comparing non-equal to 0.0.
	EndCond = Builder->CreateFCmpONE(EndCond, ConstantFP::get(*TheContext, APFloat(0.0)), "loopcond");

	// Create the "after loop" block and insert it.
	BasicBlock* AfterBB = BasicBlock::Create(*TheContext, "afterloop", TheFunction);

	// Insert the conditional branch into the end of LoopEndBB.
	Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

	// Any new code will be inserted in AfterBB.
	Builder->SetInsertPoint(AfterBB);

	// for expr always returns nullptr
	return result;
}

Value* LLVMCodeGenerator::codegen(const AlloyCompiler::RETURN_STATEMENT& node) { 
	//
	// return expression;
	// returns ConstantInt True or False depending on success or failure
	//
	Value* result = ConstantInt::getFalse(*TheContext);
	if (ERROR_NODE_ID == node.ExpressionID) {
		Builder->CreateRetVoid();
		result = ConstantInt::getTrue(*TheContext);
	}
	else {
		result = codegen(node.ExpressionID);
		if (result != nullptr) {
			Builder->CreateRet(result);
			result = ConstantInt::getTrue(*TheContext);
		}
	}
	return result;
}

